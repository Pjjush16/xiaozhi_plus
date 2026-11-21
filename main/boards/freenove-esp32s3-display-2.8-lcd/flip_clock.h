#ifndef FLIP_CLOCK_H
#define FLIP_CLOCK_H

#include <lvgl.h>

/**
 * 翻页时钟 — 覆盖层实现（手势导航版）
 *
 * 作为默认待机页面，通过滑动手势切换：
 *   左滑 → 闹钟    右滑 → 音乐    上滑 → 小智
 * 页面上只显示三个方向箭头提示，不需要按钮。
 */
class FlipClock {
public:
    static FlipClock& GetInstance() {
        static FlipClock instance;
        return instance;
    }

    void Create();
    void Show();
    void Hide();
    void Start();
    void Stop();

    bool IsVisible() const { return visible_; }

private:
    FlipClock() = default;

    lv_obj_t* overlay_ = nullptr;

    // 数字标签
    lv_obj_t* hour_tens_label_ = nullptr;
    lv_obj_t* hour_ones_label_ = nullptr;
    lv_obj_t* min_tens_label_ = nullptr;
    lv_obj_t* min_ones_label_ = nullptr;

    // 数字卡片容器
    lv_obj_t* hour_tens_card_ = nullptr;
    lv_obj_t* hour_ones_card_ = nullptr;
    lv_obj_t* min_tens_card_ = nullptr;
    lv_obj_t* min_ones_card_ = nullptr;

    // 冒号
    lv_obj_t* colon_label_ = nullptr;

    // 秒数 + 日期
    lv_obj_t* sec_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;

    // 方向箭头提示（不是按钮，只是视觉提示）
    lv_obj_t* arrow_left_ = nullptr;   // ← 左滑去闹钟
    lv_obj_t* arrow_right_ = nullptr;  // → 右滑去音乐
    lv_obj_t* arrow_bottom_ = nullptr; // ↓ 上滑去小智

    lv_timer_t* timer_ = nullptr;
    bool visible_ = false;

    static void TimerCallback(lv_timer_t* timer);
    void UpdateTime();
    lv_obj_t* CreateDigitCard(lv_obj_t* parent, lv_coord_t x, lv_coord_t y);
};

#endif // FLIP_CLOCK_H
