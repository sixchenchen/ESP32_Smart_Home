#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

/**
 * @brief 保存字符串到 NVS
 * @param namespace NVS 命名空间
 * @param key 键名
 * @param value 字符串值
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_utils_save_str(const char *namespace, const char *key, const char *value);

/**
 * @brief 从 NVS 读取字符串
 * @param namespace NVS 命名空间
 * @param key 键名
 * @param out_value 输出缓冲区
 * @param out_len 输入：缓冲区大小；输出：实际长度
 * @return ESP_OK 成功，ESP_ERR_NVS_NOT_FOUND 不存在
 */
esp_err_t nvs_utils_load_str(const char *namespace, const char *key,
                              char *out_value, size_t *out_len);

/**
 * @brief 保存 u8 到 NVS
 */
esp_err_t nvs_utils_save_u8(const char *namespace, const char *key, uint8_t value);

/**
 * @brief 从 NVS 读取 u8
 */
esp_err_t nvs_utils_load_u8(const char *namespace, const char *key, uint8_t *out_value);

/**
 * @brief 保存 u32 到 NVS
 */
esp_err_t nvs_utils_save_u32(const char *namespace, const char *key, uint32_t value);

/**
 * @brief 从 NVS 读取 u32
 */
esp_err_t nvs_utils_load_u32(const char *namespace, const char *key, uint32_t *out_value);

/**
 * @brief 保存 blob 到 NVS
 */
esp_err_t nvs_utils_save_blob(const char *namespace, const char *key,
                               const void *value, size_t len);

/**
 * @brief 从 NVS 读取 blob
 */
esp_err_t nvs_utils_load_blob(const char *namespace, const char *key,
                               void *out_value, size_t *out_len);

/**
 * @brief 删除单个 key
 */
esp_err_t nvs_utils_erase_key(const char *namespace, const char *key);

/**
 * @brief 清空整个命名空间
 */
esp_err_t nvs_utils_erase_all(const char *namespace);

/**
 * @brief 检查 key 是否存在
 */
bool nvs_utils_exists(const char *namespace, const char *key);
