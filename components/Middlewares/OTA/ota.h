#pragma once

#include "esp_err.h"
#include "stdbool.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * OTA 状态机
 */
typedef enum {
    OTA_STATE_IDLE = 0,       /* 空闲，未开始 */
    OTA_STATE_CHECKING,       /* 正在检查远程版本 */
    OTA_STATE_DOWNLOADING,    /* 下载中 */
    OTA_STATE_VERIFYING,      /* 校验中（esp_https_ota_finish 内部） */
    OTA_STATE_SUCCEEDED,      /* 升级成功，等待重启 */
    OTA_STATE_FAILED,         /* 升级失败 */
} ota_state_t;

/*
 * 升级结果
 */
typedef enum {
    OTA_RESULT_OK = 0,
    OTA_RESULT_FAIL_ARG,         /* 参数错误 */
    OTA_RESULT_FAIL_VERSION,     /* 版本不允许（远程 <= 当前） */
    OTA_RESULT_FAIL_NO_PARTITION,/* 找不到 OTA 分区 */
    OTA_RESULT_FAIL_HTTP,        /* HTTP 错误 */
    OTA_RESULT_FAIL_DOWNLOAD,    /* 下载错误 */
    OTA_RESULT_FAIL_VERIFY,      /* 校验错误 */
    OTA_RESULT_FAIL_IN_PROGRESS, /* 已有 OTA 进行中 */
} ota_result_t;

/*
 * 进度回调
 * @param state      当前状态
 * @param percent    百分比 0-100（仅 DOWNLOADING 时有意义）
 * @param bytes_done 已下载字节
 * @param bytes_total 总字节（未知时为 0）
 * @param user_data  用户数据
 */
typedef void (*ota_progress_cb_t)(ota_state_t state,
                                   int percent,
                                   uint32_t bytes_done,
                                   uint32_t bytes_total,
                                   void *user_data);

/*
 * OTA 句柄（内部使用）
 */
typedef struct ota_handle_s *ota_handle_t;

/*
 * @brief  获取当前固件版本号
 * @return  版本字符串（例如 "1.0.3"），内部静态存储，不要 free
 */
const char *ota_get_current_version(void);

/*
 * @brief  获取 OTA 状态字符串
 */
const char *ota_state_to_string(ota_state_t state);

/*
 * @brief  获取当前 OTA 状态
 */
ota_state_t ota_get_state(void);

/*
 * @brief  查询是否正在 OTA 升级中
 */
bool ota_is_in_progress(void);

/*
 * @brief  OTA 初始化（app_main 早期调用）
 *         检查上次是否 OTA 完成，标记有效固件
 * @return ESP_OK / ESP_FAIL
 */
esp_err_t ota_post_init(void);

/*
 * @brief  启动 OTA 升级（异步，立即返回，内部起任务）
 *
 * @param url          固件下载 URL，例如 "http://192.168.1.100/firmware.bin"
 *                     支持 http:// 和 https://
 * @param remote_version 远程固件版本号（可为 NULL，不做版本比较）
 * @param cb           进度回调（可为 NULL）
 * @param user_data    用户数据（传递给 cb）
 *
 * @return OTA_RESULT_OK / OTA_RESULT_FAIL_xxx
 *
 * @note   升级成功后会调用 esp_restart()，不会返回
 */
ota_result_t ota_start(const char *url,
                       const char *remote_version,
                       ota_progress_cb_t cb,
                       void *user_data);

/*
 * @brief  中止正在进行的 OTA 升级
 * @note   仅在 DOWNLOADING 阶段安全，VERIFYING 阶段中止会导致该 OTA 分区无效
 */
esp_err_t ota_abort(void);

#ifdef __cplusplus
}
#endif
