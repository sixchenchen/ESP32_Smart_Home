#include "nvs_utils.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "nvs_utils";

// ============ 字符串操作 ============

esp_err_t nvs_utils_save_str(const char *namespace, const char *key, const char *value)
{
    if (namespace == NULL || key == NULL || value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", namespace);
        return ret;
    }

    ret = nvs_set_str(handle, key, value);
    if (ret == ESP_OK)
    {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);

    if (ret == ESP_OK)
    {
        ESP_LOGD(TAG, "Saved [%s] %s = %s", namespace, key, value);
    }
    return ret;
}

esp_err_t nvs_utils_load_str(const char *namespace, const char *key, char *out_value, size_t *out_len)
{
    if (namespace == NULL || key == NULL || out_value == NULL || out_len == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READONLY, &handle);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = nvs_get_str(handle, key, out_value, out_len);
    nvs_close(handle);

    if (ret == ESP_OK)
    {
        ESP_LOGD(TAG, "Loaded [%s] %s = %s", namespace, key, out_value);
    }
    return ret;
}

// ============ U8 操作 ============

esp_err_t nvs_utils_save_u8(const char *namespace, const char *key, uint8_t value)
{
    if (namespace == NULL || key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK)
        return ret;

    ret = nvs_set_u8(handle, key, value);
    if (ret == ESP_OK)
    {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);
    return ret;
}

esp_err_t nvs_utils_load_u8(const char *namespace, const char *key, uint8_t *out_value)
{
    if (namespace == NULL || key == NULL || out_value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READONLY, &handle);
    if (ret != ESP_OK)
        return ret;

    ret = nvs_get_u8(handle, key, out_value);
    nvs_close(handle);
    return ret;
}

// ============ U32 操作 ============

esp_err_t nvs_utils_save_u32(const char *namespace, const char *key, uint32_t value)
{
    if (namespace == NULL || key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK)
        return ret;

    ret = nvs_set_u32(handle, key, value);
    if (ret == ESP_OK)
    {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);
    return ret;
}

esp_err_t nvs_utils_load_u32(const char *namespace, const char *key, uint32_t *out_value)
{
    if (namespace == NULL || key == NULL || out_value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READONLY, &handle);
    if (ret != ESP_OK)
        return ret;

    ret = nvs_get_u32(handle, key, out_value);
    nvs_close(handle);
    return ret;
}

// ============ Blob 操作 ============

esp_err_t nvs_utils_save_blob(const char *namespace, const char *key,
                              const void *value, size_t len)
{
    if (namespace == NULL || key == NULL || value == NULL || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK)
        return ret;

    ret = nvs_set_blob(handle, key, value, len);
    if (ret == ESP_OK)
    {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);
    return ret;
}

esp_err_t nvs_utils_load_blob(const char *namespace, const char *key,
                              void *out_value, size_t *out_len)
{
    if (namespace == NULL || key == NULL || out_value == NULL || out_len == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READONLY, &handle);
    if (ret != ESP_OK)
        return ret;

    ret = nvs_get_blob(handle, key, out_value, out_len);
    nvs_close(handle);
    return ret;
}

// ============ 删除操作 ============

esp_err_t nvs_utils_erase_key(const char *namespace, const char *key)
{
    if (namespace == NULL || key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK)
        return ret;

    ret = nvs_erase_key(handle, key);
    if (ret == ESP_OK)
    {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);
    return ret;
}

esp_err_t nvs_utils_erase_all(const char *namespace)
{
    if (namespace == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK)
        return ret;

    ret = nvs_erase_all(handle);
    if (ret == ESP_OK)
    {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Erased all keys in namespace: %s", namespace);
    }
    return ret;
}

// ============ 检查存在性 ============

bool nvs_utils_exists(const char *namespace, const char *key)
{
    if (namespace == NULL || key == NULL)
    {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace, NVS_READONLY, &handle);
    if (ret != ESP_OK)
        return false;

    size_t len = 0;
    ret = nvs_get_str(handle, key, NULL, &len);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        // 尝试其他类型
        ret = nvs_get_u8(handle, key, NULL);
        if (ret == ESP_ERR_NVS_NOT_FOUND)
        {
            ret = nvs_get_blob(handle, key, NULL, &len);
        }
    }

    nvs_close(handle);
    return (ret == ESP_OK);
}