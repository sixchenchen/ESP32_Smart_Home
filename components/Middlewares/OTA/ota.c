/*
 * ota.c - ESP32 OTA 升级模块
 *
 * 功能：
 *   1. OTA 状态机（IDLE → DOWNLOADING → VERIFYING → SUCCEEDED/FAILED）
 *   2. 版本校验（远程 > 当前才升级）
 *   3. 互斥保护（FreeRTOS mutex），避免并发触发
 *   4. 进度回调（百分比 + 字节数）
 *   5. post_init()：新固件首次启动时 mark valid + 取消回滚
 *   6. 异步执行（内部起 FreeRTOS task），不阻塞调用者
 *
 * 使用 ESP-IDF v6.0.2 的 esp_https_ota 高级 API：
 *   esp_https_ota_begin → esp_https_ota_perform (循环) → esp_https_ota_finish
 */

#include "ota.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_app_desc.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "ota";

/* ─────────────── 模块常量 ─────────────── */

#define OTA_URL_MAX_LEN     256     /* 固件 URL 最大长度 */
#define OTA_VER_MAX_LEN     32      /* 版本号最大长度 */
#define OTA_HTTP_TIMEOUT_MS 10000   /* HTTP 连接/读取超时 */
#define OTA_TASK_STACK      8192    /* OTA 任务栈大小 */
#define OTA_TASK_PRIORITY   5       /* OTA 任务优先级 */
#define OTA_PERFORM_DELAY   5       /* perform 循环间 delay (ms) */
#define OTA_RESTART_DELAY   1000    /* 升级成功后延迟重启 (ms) */
#define OTA_FAIL_DELAY      100     /* 失败后清理延迟 (ms) */

/* ─────────────── 内部状态 ─────────────── */

static ota_state_t g_state = OTA_STATE_IDLE;
static SemaphoreHandle_t g_mutex = NULL;
static bool g_abort_flag = false;

/* 启动参数缓存（供 task 使用） */
typedef struct
{
    char url[OTA_URL_MAX_LEN];
    char remote_version[OTA_VER_MAX_LEN];
    ota_progress_cb_t cb;
    void *user_data;
} ota_job_t;

/* ─────────────── 状态辅助 ─────────────── */

static void ota_lock(void)
{
    if (g_mutex)
        xSemaphoreTake(g_mutex, portMAX_DELAY);
}

static void ota_unlock(void)
{
    if (g_mutex)
        xSemaphoreGive(g_mutex);
}

static void ota_set_state(ota_state_t new_state)
{
    ota_lock();
    g_state = new_state;
    ota_unlock();
    ESP_LOGI(TAG, "state → %s", ota_state_to_string(new_state));
}

ota_state_t ota_get_state(void)
{
    ota_lock();
    ota_state_t s = g_state;
    ota_unlock();
    return s;
}

bool ota_is_in_progress(void)
{
    ota_state_t s = ota_get_state();
    return s == OTA_STATE_CHECKING || s == OTA_STATE_DOWNLOADING || s == OTA_STATE_VERIFYING;
}

const char *ota_state_to_string(ota_state_t state)
{
    switch (state)
    {
    case OTA_STATE_IDLE:
        return "IDLE";
    case OTA_STATE_CHECKING:
        return "CHECKING";
    case OTA_STATE_DOWNLOADING:
        return "DOWNLOADING";
    case OTA_STATE_VERIFYING:
        return "VERIFYING";
    case OTA_STATE_SUCCEEDED:
        return "SUCCEEDED";
    case OTA_STATE_FAILED:
        return "FAILED";
    default:
        return "UNKNOWN";
    }
}

/* ─────────────── 版本相关 ─────────────── */

const char *ota_get_current_version(void)
{
    const esp_app_desc_t *desc = esp_app_get_description();
    if (desc == NULL)
        return "0.0.0";
    return desc->version;
}

/* 简单版本比较：v1 > v2 返回正数，相等返回 0，v1 < v2 返回负数
 * 只处理 "x.y.z" 这种数字格式 */
static int ota_version_compare(const char *v1, const char *v2)
{
    int a1 = 0, b1 = 0, c1 = 0;
    int a2 = 0, b2 = 0, c2 = 0;
    sscanf(v1, "%d.%d.%d", &a1, &b1, &c1);
    sscanf(v2, "%d.%d.%d", &a2, &b2, &c2);
    if (a1 != a2)
        return a1 - a2;
    if (b1 != b2)
        return b1 - b2;
    return c1 - c2;
}

/* ─────────────── 初始化：新固件启动后 mark valid ─────────────── */

esp_err_t ota_post_init(void)
{
    /* 延迟创建 mutex，因为这里是在 app_main 最早期被调用的 */
    if (g_mutex == NULL)
    {
        g_mutex = xSemaphoreCreateMutex();
        if (g_mutex == NULL)
        {
            ESP_LOGE(TAG, "create mutex failed");
            return ESP_FAIL;
        }
    }

    const esp_app_desc_t *desc = esp_app_get_description();
    ESP_LOGI(TAG, "current fw: name=%s version=%s",
             desc ? desc->project_name : "?",
             desc ? desc->version : "?");

    /*
     * 检查 bootloader 标记的升级状态
     * 只有从 OTA 分区（ota_0 / ota_1）启动的固件才需要 mark valid
     * factory 分区首次启动时调用会返回错误，直接跳过
     */
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running && running->subtype != ESP_PARTITION_SUBTYPE_APP_FACTORY)
    {
        esp_err_t ret = esp_ota_mark_app_valid_cancel_rollback();
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "current app marked valid (rollback cancelled)");
        }
        else
        {
            ESP_LOGW(TAG, "mark valid failed: %s", esp_err_to_name(ret));
        }
    }
    else
    {
        ESP_LOGI(TAG, "running from factory partition, skip mark valid");
    }

    return ESP_OK;
}

/* ─────────────── abort ─────────────── */

esp_err_t ota_abort(void)
{
    ota_lock();
    ota_state_t s = g_state;
    ota_unlock();

    if (s != OTA_STATE_DOWNLOADING && s != OTA_STATE_CHECKING)
    {
        ESP_LOGW(TAG, "abort not allowed in state %s", ota_state_to_string(s));
        return ESP_ERR_INVALID_STATE;
    }

    g_abort_flag = true;
    ESP_LOGW(TAG, "abort requested");
    return ESP_OK;
}

/* ─────────────── OTA 任务实现 ─────────────── */

static ota_result_t ota_do_upgrade(const ota_job_t *job)
{
    esp_err_t ret;
    esp_https_ota_handle_t h_ota = NULL;

    /* 1. 参数校验 */
    if (job == NULL || job->url[0] == '\0')
    {
        return OTA_RESULT_FAIL_ARG;
    }

    /* 2. 版本比较（如果提供了 remote_version） */
    if (job->remote_version[0] != '\0')
    {
        ota_set_state(OTA_STATE_CHECKING);

        const char *cur = ota_get_current_version();
        int cmp = ota_version_compare(job->remote_version, cur);
        ESP_LOGI(TAG, "version: remote=%s current=%s cmp=%d",
                 job->remote_version, cur, cmp);

        if (cmp <= 0)
        {
            ESP_LOGI(TAG, "remote (%s) <= current (%s), skip OTA",
                     job->remote_version, cur);
            return OTA_RESULT_FAIL_VERSION;
        }
    }

    /* 3. 检查下一个 OTA 分区是否存在 */
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    if (next == NULL)
    {
        ESP_LOGE(TAG, "no OTA update partition found!");
        return OTA_RESULT_FAIL_NO_PARTITION;
    }
    ESP_LOGI(TAG, "target partition: label=%s size=%lu addr=0x%lx",
             next->label, (unsigned long)next->size, (unsigned long)next->address);

    /* 4. 构造 HTTP 配置 */
    esp_http_client_config_t http_cfg = {
        .url = job->url,
        .timeout_ms = OTA_HTTP_TIMEOUT_MS,
        .keep_alive_enable = false,
        .cert_pem = NULL, /* 不校验服务器证书，方便测试 */
        .skip_cert_common_name_check = true,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    /* 5. esp_https_ota_begin —— 建立 HTTP 连接，读取固件头，分配分区 */
    ESP_LOGI(TAG, "OTA begin, url=%s", job->url);
    ret = esp_https_ota_begin(&ota_cfg, &h_ota);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(ret));
        return OTA_RESULT_FAIL_HTTP;
    }

    /* 6. 读取远程固件描述，打印版本号 */
    esp_app_desc_t remote_desc;
    if (esp_https_ota_get_img_desc(h_ota, &remote_desc) == ESP_OK)
    {
        ESP_LOGI(TAG, "remote fw: version=%s project=%s",
                 remote_desc.version, remote_desc.project_name);

        /* 再次校验（HTTP 头里的版本 vs 当前） */
        const char *cur = ota_get_current_version();
        if (ota_version_compare(remote_desc.version, cur) <= 0)
        {
            ESP_LOGI(TAG, "remote fw version (%s) <= current (%s), abort",
                     remote_desc.version, cur);
            esp_https_ota_abort(h_ota);
            return OTA_RESULT_FAIL_VERSION;
        }
    }

    int total_size = esp_https_ota_get_image_size(h_ota);
    ESP_LOGI(TAG, "firmware size: %d bytes", total_size);

    /* 7. 主循环 perform —— 每次下载一段，写入 flash */
    ota_set_state(OTA_STATE_DOWNLOADING);
    g_abort_flag = false;

    while (1)
    {
        if (g_abort_flag)
        {
            ESP_LOGW(TAG, "abort by user");
            esp_https_ota_abort(h_ota);
            return OTA_RESULT_FAIL_DOWNLOAD;
        }

        ret = esp_https_ota_perform(h_ota);

        /* 计算进度 */
        int done = esp_https_ota_get_image_len_read(h_ota);
        int total = total_size;
        int pct = (total > 0) ? (done * 100 / total) : 0;

        if (job->cb)
        {
            job->cb(OTA_STATE_DOWNLOADING, pct,
                    (uint32_t)done, (uint32_t)total,
                    job->user_data);
        }

        if (ret == ESP_OK)
        {
            /* 下载完成 */
            ESP_LOGI(TAG, "download complete: %d/%d bytes", done, total);
            break;
        }
        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS)
        {
            ESP_LOGE(TAG, "esp_https_ota_perform failed: %s", esp_err_to_name(ret));
            esp_https_ota_abort(h_ota);
            return OTA_RESULT_FAIL_DOWNLOAD;
        }

        vTaskDelay(pdMS_TO_TICKS(OTA_PERFORM_DELAY));
    }

    /* 8. finish —— 校验固件 + 设置启动分区 */
    ota_set_state(OTA_STATE_VERIFYING);

    ret = esp_https_ota_finish(h_ota);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(ret));
        return OTA_RESULT_FAIL_VERIFY;
    }

    /* 9. 成功，重启 */
    ota_set_state(OTA_STATE_SUCCEEDED);
    ESP_LOGI(TAG, "OTA SUCCEEDED, restarting in 1s...");

    if (job->cb)
    {
        job->cb(OTA_STATE_SUCCEEDED, 100, 0, 0, job->user_data);
    }

    vTaskDelay(pdMS_TO_TICKS(OTA_RESTART_DELAY));
    esp_restart();
    /* 不会到这里 */

    return OTA_RESULT_OK;
}

/* 任务入口 */
static void ota_task(void *arg)
{
    ota_job_t *job = (ota_job_t *)arg;

    ota_result_t r = ota_do_upgrade(job);

    if (r != OTA_RESULT_OK)
    {
        ota_set_state(OTA_STATE_FAILED);
        ESP_LOGE(TAG, "OTA failed result=%d", r);
        if (job->cb)
        {
            job->cb(OTA_STATE_FAILED, 0, 0, 0, job->user_data);
        }
    }

    vTaskDelay(pdMS_TO_TICKS(OTA_FAIL_DELAY));

    ota_lock();
    g_state = OTA_STATE_IDLE;
    ota_unlock();

    vTaskDelete(NULL);
    free(job);
}

/* ─────────────── 公共入口 ─────────────── */

ota_result_t ota_start(const char *url,
                       const char *remote_version,
                       ota_progress_cb_t cb,
                       void *user_data)
{
    /* 首次调用时创建 mutex */
    if (g_mutex == NULL)
    {
        g_mutex = xSemaphoreCreateMutex();
        if (g_mutex == NULL)
            return OTA_RESULT_FAIL_ARG;
    }

    /* 互斥检查 */
    if (ota_is_in_progress())
    {
        ESP_LOGW(TAG, "OTA already in progress (state=%s)", ota_state_to_string(ota_get_state()));
        return OTA_RESULT_FAIL_IN_PROGRESS;
    }

    /* 参数 */
    if (url == NULL || url[0] == '\0')
    {
        ESP_LOGE(TAG, "url is NULL/empty");
        return OTA_RESULT_FAIL_ARG;
    }
    if (strlen(url) >= OTA_URL_MAX_LEN)
    {
        ESP_LOGE(TAG, "url too long");
        return OTA_RESULT_FAIL_ARG;
    }

    /* 组装 job */
    ota_job_t *job = (ota_job_t *)malloc(sizeof(ota_job_t));
    if (job == NULL)
        return OTA_RESULT_FAIL_ARG;

    memset(job, 0, sizeof(*job));
    strncpy(job->url, url, sizeof(job->url) - 1);
    if (remote_version)
    {
        strncpy(job->remote_version, remote_version, sizeof(job->remote_version) - 1);
    }
    job->cb = cb;
    job->user_data = user_data;

    /* 启 task */
    BaseType_t ok = xTaskCreate(ota_task, "ota_task",
                                OTA_TASK_STACK,
                                job,
                                OTA_TASK_PRIORITY,
                                NULL);
    if (ok != pdPASS)
    {
        ESP_LOGE(TAG, "xTaskCreate failed");
        free(job);
        return OTA_RESULT_FAIL_ARG;
    }

    ESP_LOGI(TAG, "OTA task started");
    return OTA_RESULT_OK;
}
