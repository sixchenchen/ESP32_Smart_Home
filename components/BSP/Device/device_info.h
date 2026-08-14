#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#ifdef __cplusplus
extern "C" {
#endif

#define DEVICE_ID_LEN 13  // 12位MAC + 结束符

/**
 * @brief 获取设备唯一ID（基础MAC地址）
 * @return 指向设备ID字符串的指针，如 "A1B2C3D4E5F6"
 */
const char* device_get_id(void);

/**
 * @brief 获取设备名称
 */
const char* device_get_name(void);

/**
 * @brief 打印设备信息
 */
void device_print_info(void);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_INFO_H