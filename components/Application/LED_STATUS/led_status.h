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

    /**
     * @brief 锁定/解锁 LED 状态机
     * @param lock true: 锁定（外部接管 LED，状态机暂停切换）
     *             false: 解锁（恢复正常）
     * @note 长按恢复出厂的 LED 闪灯期间置 true，防止状态机覆盖反馈
     */
    void led_status_lock(bool lock);

    /**
     * @brief 查询 LED 状态机是否被锁定
     */
    bool led_status_lock_get(void);

#ifdef __cplusplus
}
#endif

#endif
