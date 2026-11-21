#ifndef FLIP_CLOCK_H
#define FLIP_CLOCK_H

#include <lvgl.h>

/**
 * 翻页时钟 — 覆盖层实现
 *
 * 使用 lv_layer_top() 上的全屏面板而非独立 screen，
 * 避免 lv_screen_load 销毁小智主界面。
 * 显示/隐藏通过 Show() / Hide() 控制。
 */
class FlipClock {
public:
    static FlipClock& GetInstance() {
        static FlipClock instance;
        return instance;
    }

    /** 创建覆盖层 UI（幂等，仅创建一次） */
    void Create();

    /** 显示翻页时钟（覆盖在小智界面之上） */
    void Show();

    /** 隐藏翻页时钟（露出小智界面） */
    void Hide();

    /** 启动每秒更新定时器 */
    void Start();

    /** 停止定时器 */
    void Stop();

    /** 当前是否正在显示 */
    bool IsVisible() const { return visible_; }

private:
    FlipClock() = default;

    // 覆盖层根面板
    lv_obj_t* overlay_ = nullptr;

    // 数字标签（时/分各位）
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

    // "小智"按钮
    lv_obj_t* xiaozhi_btn_ = nullptr;

    // 定时器
    lv_timer_t* timer_ = nullptr;
    bool visible_ = false;

    static void TimerCallback(lv_timer_t* timer);
    void UpdateTime();

    // 创建单个数字卡片，返回内部 label
    lv_obj_t* CreateDigitCard(lv_obj_t* parent, lv_coord_t x, lv_coord_t y);
};

#endif // FLIP_CLOCK_H
