#ifndef LED_STATUS_H
#define LED_STATUS_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief 启动 LED 状态监控任务
     * @note 根据 WiFi 状态自动控制 LED 模式
     */
    void led_status_start(void);

#ifdef __cplusplus
}
#endif

#endif