#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "http_server.h"
#include "wifi_manager.h"
#include "wifi_scan.h"

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
    char buffer[256] = {0};
    int recv_len = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    if (recv_len <= 0)
    {
        return ESP_FAIL;
    }
    buffer[recv_len] = 0;
    ESP_LOGI(TAG, "recv:%s", buffer);
    cJSON *root = cJSON_Parse(buffer);
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
    char ssid[32];
    char password[64];
    strcpy(ssid, ssid_json->valuestring);
    strcpy(password, pass_json->valuestring);
    ESP_LOGI(TAG, "SSID:%s PASSWORD:%s", ssid, password);
    wifi_manager_set_wifi(ssid, password);
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "wifi connecting...");
    return ESP_OK;
}

static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "start wifi scan");
    wifi_scan_result_t result[20];
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

static void factory_reset_task(void *arg)
{
    /*
        等待HTTP响应完成
    */
    vTaskDelay(pdMS_TO_TICKS(500));
    wifi_manager_factory_reset();
    vTaskDelete(NULL);
}

static esp_err_t factory_reset_handler(httpd_req_t *req)
{
    ESP_LOGW(TAG, "factory reset request");
    /*
        先回复网页
    */
    httpd_resp_sendstr(req, "factory reset ok");
    /*
        后台执行
    */
    xTaskCreate(factory_reset_task,
                "factory_reset",
                4096,
                NULL,
                5,
                NULL);
    return ESP_OK;
}

/*
    启动服务器
*/
void http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t index_uri =
        {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL};
    httpd_register_uri_handler(server, &index_uri);
    httpd_uri_t css_uri =
        {
            .uri = "/style.css",
            .method = HTTP_GET,
            .handler = style_handler,
            .user_ctx = NULL};

    httpd_register_uri_handler(server, &css_uri);
    httpd_uri_t js_uri =
        {
            .uri = "/app.js",
            .method = HTTP_GET,
            .handler = js_handler,
            .user_ctx = NULL};

    httpd_register_uri_handler(server, &js_uri);
    httpd_uri_t wifi_uri =
        {
            .uri = "/wifi_config",
            .method = HTTP_POST,
            .handler = wifi_config_handler,
            .user_ctx = NULL};
    httpd_register_uri_handler(server, &wifi_uri);

    httpd_uri_t wifi_scan =
        {
            .uri = "/scan",
            .method = HTTP_GET,
            .handler = wifi_scan_handler,
            .user_ctx = NULL};
    httpd_register_uri_handler(server, &wifi_scan);

    httpd_uri_t factory_reset_uri =
        {

            .uri = "/factory_reset",

            .method = HTTP_GET,

            .handler = factory_reset_handler,

            .user_ctx = NULL

        };

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &factory_reset_uri));
    ESP_LOGI(TAG, "HTTP SERVER START");
}
