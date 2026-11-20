#ifndef FLIP_CLOCK_H
#define FLIP_CLOCK_H

#include <lvgl.h>

class FlipClock {
public:
    static FlipClock& GetInstance() {
        static FlipClock instance;
        return instance;
    }

    // 创建翻页时钟页面，返回 screen 对象
    lv_obj_t* Create();

    // 启动时钟更新（注册定时器）
    void Start();

    // 停止时钟更新
    void Stop();

private:
    FlipClock() = default;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* hour_tens_ = nullptr;
    lv_obj_t* hour_ones_ = nullptr;
    lv_obj_t* min_tens_ = nullptr;
    lv_obj_t* min_ones_ = nullptr;
    lv_obj_t* sec_label_ = nullptr;
    lv_obj_t* colon1_ = nullptr;
    lv_obj_t* colon2_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* back_btn_ = nullptr;
    lv_timer_t* timer_ = nullptr;

    static void TimerCallback(lv_timer_t* timer);
    void UpdateTime();
    void SetDigit(lv_obj_t* label, int value);

    // 数字卡片样式
    lv_obj_t* CreateDigitCard(lv_obj_t* parent, int x, int y);
};

#endif // FLIP_CLOCK_H
