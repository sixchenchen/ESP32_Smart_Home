#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "http_server.h"
#include "wifi_manager.h"
#include "wifi_scan.h"
#include "ota.h"

static const char *TAG = "http_server";
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");

static httpd_handle_t server = NULL;

/*
主页
*/

static esp_err_t index_handler(httpd_req_t *req)
{
    size_t length = index_html_end - index_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, length);
    return ESP_OK;
}

static esp_err_t style_handler(httpd_req_t *req)
{
    size_t length = style_css_end - style_css_start;
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char *)style_css_start, length);
    return ESP_OK;
}
static esp_err_t js_handler(httpd_req_t *req)
{
    size_t length = app_js_end - app_js_start;
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)app_js_start, length);
    return ESP_OK;
}

/*
    接收WiFi配置
    POST
*/
static esp_err_t wifi_config_handler(httpd_req_t *req)
{
    // 获取 Content-Length
    size_t total_len = 0;
    char content_len_str[32] = {0};
    if (httpd_req_get_hdr_value_str(req, "Content-Length", content_len_str, sizeof(content_len_str)) == ESP_OK)
    {
        total_len = atoi(content_len_str);
    }

    //  限制最大长度（防止内存耗尽）
    const size_t MAX_BODY_SIZE = 512;
    if (total_len > MAX_BODY_SIZE)
    {
        ESP_LOGW(TAG, "Request body too large: %zu > %zu", total_len, MAX_BODY_SIZE);
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Body too large");
        return ESP_FAIL;
    }

    // 分配缓冲区（动态或固定）
    char *buffer = malloc(total_len + 1);
    if (buffer == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }
    memset(buffer, 0, total_len + 1);

    // 循环读取完整 body
    size_t received = 0;
    int ret = 0;
    while (received < total_len)
    {
        ret = httpd_req_recv(req, buffer + received, total_len - received);
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue; // 超时重试
            }
            ESP_LOGW(TAG, "Failed to receive body: %d", ret);
            free(buffer);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            return ESP_FAIL;
        }
        received += ret;
    }
    buffer[received] = '\0';

    ESP_LOGI(TAG, "Received %zu bytes: %s", received, buffer);

    // 解析 JSON
    cJSON *root = cJSON_Parse(buffer);
    free(buffer); // 解析完成后释放

    if (root == NULL)
    {
        httpd_resp_sendstr(req, "json error");
        return ESP_FAIL;
    }

    cJSON *ssid_json = cJSON_GetObjectItem(root, "ssid");
    cJSON *pass_json = cJSON_GetObjectItem(root, "password");

    if (!ssid_json || !pass_json)
    {
        cJSON_Delete(root);
        httpd_resp_sendstr(req, "parameter error");
        return ESP_FAIL;
    }

    // 检查是否为字符串
    if (!cJSON_IsString(ssid_json) || !cJSON_IsString(pass_json))
    {
        cJSON_Delete(root);
        httpd_resp_sendstr(req, "SSID and password must be strings");
        return ESP_FAIL;
    }

    char ssid[32];
    char password[64];

    // 检查长度
    size_t ssid_len = strlen(ssid_json->valuestring);
    size_t pass_len = strlen(pass_json->valuestring);

    if (ssid_len >= sizeof(ssid))
    {
        cJSON_Delete(root);
        httpd_resp_sendstr(req, "SSID too long (max 31 chars)");
        ESP_LOGW(TAG, "SSID too long: %zu", ssid_len);
        return ESP_FAIL;
    }

    if (pass_len >= sizeof(password))
    {
        cJSON_Delete(root);
        httpd_resp_sendstr(req, "Password too long (max 63 chars)");
        ESP_LOGW(TAG, "Password too long: %zu", pass_len);
        return ESP_FAIL;
    }

    strlcpy(ssid, ssid_json->valuestring, sizeof(ssid));
    strlcpy(password, pass_json->valuestring, sizeof(password));

    ESP_LOGI(TAG, "SSID:%s PASSWORD:%s", ssid, password);

    wifi_manager_set_wifi(ssid, password);
    cJSON_Delete(root);

    // 返回 JSON
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "connecting");
    cJSON_AddStringToObject(resp, "msg", "WiFi连接中，请稍候...");
    char *json = cJSON_PrintUnformatted(resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    cJSON_Delete(resp);
    return ESP_OK;
}
// WiFi 状态查询接口
static esp_err_t wifi_status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    wifi_manager_state_t state = wifi_manager_get_state();

    switch (state)
    {
    case WIFI_MANAGER_CONNECTED:
    {
        cJSON_AddStringToObject(root, "status", "connected");
        const char *ip = wifi_manager_get_ip_str();
        cJSON_AddStringToObject(root, "ip", ip ? ip : "");
        cJSON_AddStringToObject(root, "msg", "WiFi连接成功");
        break;
    }
    case WIFI_MANAGER_CONNECTING:
        cJSON_AddStringToObject(root, "status", "connecting");
        cJSON_AddStringToObject(root, "msg", "正在连接WiFi...");
        break;
    case WIFI_MANAGER_AP_CONFIG:
        cJSON_AddStringToObject(root, "status", "failed");
        cJSON_AddStringToObject(root, "reason", "连接失败，请检查密码或信号");
        cJSON_AddStringToObject(root, "msg", "连接失败");
        break;
    default:
        cJSON_AddStringToObject(root, "status", "idle");
        cJSON_AddStringToObject(root, "msg", "等待配置");
        break;
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "start wifi scan");
    static wifi_scan_result_t result[20];
    uint16_t count = 0;
    esp_err_t ret = wifi_scan_start(result, 20, &count);
    if (ret != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "scan failed");
        return ESP_FAIL;
    }

    /*
        创建JSON数组
        [
          {
            ssid:"xxx",
            rssi:-40,
            channel:11
          }
        ]
    */
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < count; i++)
    {
        cJSON *wifi = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi, "ssid", result[i].ssid);
        cJSON_AddNumberToObject(wifi, "rssi", result[i].rssi);
        cJSON_AddNumberToObject(wifi, "channel", result[i].channel);
        cJSON_AddItemToArray(root, wifi);
    }
    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// 工厂重置任务
static void factory_reset_task(void *arg)
{
    // 等待 HTTP 响应完成
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGW(TAG, "Starting factory reset process...");

    //  停止 HTTP 服务器（避免与 WiFi 重启竞态）
    extern httpd_handle_t server;
    if (server != NULL)
    {
        ESP_LOGI(TAG, "Stopping HTTP server...");
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }

    // 执行工厂重置
    wifi_manager_factory_reset();

    // 重新启动 HTTP 服务器（配网模式）
    ESP_LOGI(TAG, "Restarting HTTP server in AP config mode...");
    http_server_start();

    ESP_LOGI(TAG, "Factory reset completed, HTTP server restarted");
    vTaskDelete(NULL);
}
// ============ 工厂重置 Handler ============
static esp_err_t factory_reset_handler(httpd_req_t *req)
{
    //  检查请求方法
    if (req->method != HTTP_POST)
    {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }

    // 读取完整的请求体（防止截断）
    char buffer[FACTORY_RESET_BODY_MAX] = {0};
    size_t received = 0;
    int ret = 0;
    while (received < FACTORY_RESET_BODY_MAX - 1)
    {
        ret = httpd_req_recv(req, buffer + received, FACTORY_RESET_BODY_MAX - 1 - received);
        if (ret < 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue; // 超时重试
            }
            ESP_LOGW(TAG, "Receive error: %d", ret);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            return ESP_FAIL;
        }
        if (ret == 0)
        {
            break; // 没有更多数据
        }
        received += ret;
    }
    buffer[received] = '\0';

    // 检查是否完整接收
    if (received >= FACTORY_RESET_BODY_MAX - 1)
    {
        ESP_LOGW(TAG, "Body too large, truncated");
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Body too large");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Received %zu bytes: %s", received, buffer);

    // 解析 JSON
    cJSON *root = cJSON_Parse(buffer);
    if (root == NULL)
    {
        httpd_resp_sendstr(req, "JSON parse error");
        return ESP_FAIL;
    }

    // 检查确认字段
    cJSON *confirm = cJSON_GetObjectItem(root, "confirm");
    if (!confirm || !cJSON_IsString(confirm) ||
        strcmp(confirm->valuestring, "YES") != 0)
    {
        cJSON_Delete(root);
        httpd_resp_sendstr(req, "Missing or invalid 'confirm' field (must be 'YES')");
        return ESP_FAIL;
    }

    cJSON_Delete(root);

    ESP_LOGW(TAG, "factory reset confirmed, executing...");

    // 发送响应
    httpd_resp_sendstr(req, "factory reset ok");

    // 创建重置任务（延迟执行）
    xTaskCreate(factory_reset_task, "factory_reset", 4096, NULL, 5, NULL);
    return ESP_OK;
}

/*
    OTA 状态查询 GET /ota_status
*/
static esp_err_t ota_status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "state", ota_state_to_string(ota_get_state()));
    cJSON_AddBoolToObject(root, "in_progress", ota_is_in_progress());
    cJSON_AddStringToObject(root, "current_version", ota_get_current_version());

    char *resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);
    free(resp);
    return ESP_OK;
}

/*
    OTA 升级触发 POST http://192.168.124.7/api/ota
    Headers: 
        Content-Type: application/json
    Body JSON: 
        {
            "url": "http://192.168.124.6:8000/build/sample_project.bin",
            "version": "1.0.30"
        }
    
*/
static esp_err_t ota_trigger_handler(httpd_req_t *req)
{
    /* 限制 body 大小 */
    char body_buf[512] = {0};
    size_t total = req->content_len;
    if (total >= sizeof(body_buf))
    {
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "body too large");
        return ESP_FAIL;
    }
    int received = httpd_req_recv(req, body_buf, total);
    if (received <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "read body fail");
        return ESP_FAIL;
    }

    /* 解析 JSON */
    cJSON *root = cJSON_Parse(body_buf);
    if (!root)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_FAIL;
    }

    const char *url = cJSON_GetStringValue(cJSON_GetObjectItem(root, "url"));
    const char *ver = cJSON_GetStringValue(cJSON_GetObjectItem(root, "version"));

    if (!url || strlen(url) == 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing 'url'");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA trigger: url=%s version=%s", url, ver ? ver : "?");

    ota_result_t r = ota_start(url, ver, NULL, NULL);
    cJSON_Delete(root);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "result", r);
    cJSON_AddStringToObject(resp, "state", ota_state_to_string(ota_get_state()));
    char *resp_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp_str);
    free(resp_str);
    return ESP_OK;
}

/*
    启动服务器
*/
void http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 16; /* 从默认 8 提到 16，避免 slot 不够 */
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t index_uri =
        {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &index_uri));

    httpd_uri_t css_uri =
        {
            .uri = "/style.css",
            .method = HTTP_GET,
            .handler = style_handler,
            .user_ctx = NULL};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &css_uri));

    httpd_uri_t js_uri =
        {
            .uri = "/app.js",
            .method = HTTP_GET,
            .handler = js_handler,
            .user_ctx = NULL};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &js_uri));

    httpd_uri_t wifi_uri =
        {
            .uri = "/wifi_config",
            .method = HTTP_POST,
            .handler = wifi_config_handler,
            .user_ctx = NULL};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_uri));

    httpd_uri_t wifi_scan =
        {
            .uri = "/scan",
            .method = HTTP_GET,
            .handler = wifi_scan_handler,
            .user_ctx = NULL};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_scan));

    httpd_uri_t wifi_status_uri = {
        .uri = "/wifi_status",
        .method = HTTP_GET,
        .handler = wifi_status_handler,
        .user_ctx = NULL};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_status_uri));

    httpd_uri_t factory_reset_uri =
        {
            .uri = "/factory_reset",
            .method = HTTP_POST,
            .handler = factory_reset_handler,
            .user_ctx = NULL

    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &factory_reset_uri));

    httpd_uri_t ota_trigger_uri = {
        .uri = "/api/ota",
        .method = HTTP_POST,
        .handler = ota_trigger_handler,
        .user_ctx = NULL};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &ota_trigger_uri));

    httpd_uri_t ota_status_uri = {
        .uri = "/ota_status",
        .method = HTTP_GET,
        .handler = ota_status_handler,
        .user_ctx = NULL};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &ota_status_uri));

    ESP_LOGI(TAG, "HTTP SERVER START");
}
