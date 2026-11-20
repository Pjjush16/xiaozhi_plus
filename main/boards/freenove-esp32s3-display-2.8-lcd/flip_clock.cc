#include "flip_clock.h"
#include "page_manager.h"

#include <esp_log.h>
#include <esp_sntp.h>
#include <time.h>

#define TAG "FlipClock"

// 数字卡片：深色背景 + 大号白色数字
lv_obj_t* FlipClock::CreateDigitCard(lv_obj_t* parent, int x, int y) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 60, 80);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x2A2A3A), 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 4, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_50, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* label = lv_label_create(card);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(label, "0");

    return label;
}

lv_obj_t* FlipClock::Create() {
    if (screen_) return screen_;

    screen_ = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_grad_color(screen_, lv_color_hex(0x16213E), 0);
    lv_obj_set_style_bg_grad_dir(screen_, LV_GRAD_DIR_VER, 0);

    // 日期标签（顶部居中）
    date_label_ = lv_label_create(screen_);
    lv_obj_set_style_text_font(date_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(date_label_, lv_color_hex(0x8888AA), 0);
    lv_obj_align(date_label_, LV_ALIGN_TOP_MID, 0, 12);
    lv_label_set_text(date_label_, "2026-01-01 Mon");

    // 时:分 数字卡片 (居中排列)
    // 屏幕 320x240, 4 个卡片各 60px 宽 + 间隔
    int y_offset = 50;
    int x_start = 10;
    int card_w = 60;
    int gap = 8;

    hour_tens_ = CreateDigitCard(screen_, x_start, y_offset);
    hour_ones_ = CreateDigitCard(screen_, x_start + card_w + gap, y_offset);

    // 冒号
    colon1_ = lv_label_create(screen_);
    lv_obj_set_style_text_font(colon1_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(colon1_, lv_color_hex(0xFF6B6B), 0);
    lv_obj_set_pos(colon1_, x_start + 2 * (card_w + gap), y_offset + 12);
    lv_label_set_text(colon1_, ":");

    min_tens_ = CreateDigitCard(screen_, x_start + 2 * (card_w + gap) + 24, y_offset);
    min_ones_ = CreateDigitCard(screen_, x_start + 3 * (card_w + gap) + 24, y_offset);

    // 秒标签（底部居中）
    sec_label_ = lv_label_create(screen_);
    lv_obj_set_style_text_font(sec_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sec_label_, lv_color_hex(0xAAAA88), 0);
    lv_obj_align(sec_label_, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_label_set_text(sec_label_, ":00");

    // 左下角"小智"按钮（回到小智页面）
    back_btn_ = lv_btn_create(screen_);
    lv_obj_set_size(back_btn_, 50, 30);
    lv_obj_align(back_btn_, LV_ALIGN_BOTTOM_LEFT, 8, -8);
    lv_obj_set_style_bg_color(back_btn_, lv_color_hex(0x8B5CF6), 0);
    lv_obj_set_style_radius(back_btn_, 15, 0);
    lv_obj_set_style_border_width(back_btn_, 0, 0);

    lv_obj_t* btn_label = lv_label_create(back_btn_);
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(btn_label, LV_SYMBOL_HOME);
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(back_btn_, [](lv_event_t* e) {
        PageManager::GetInstance().SwitchTo(PageId::XIAOZHI);
    }, LV_EVENT_CLICKED, nullptr);

    // 初始化 SNTP 时间同步
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "ntp.aliyun.com");
    sntp_init();

    // 设置时区为北京时间
    setenv("TZ", "CST-8", 1);
    tzset();

    UpdateTime();
    ESP_LOGI(TAG, "Flip clock page created");
    return screen_;
}

void FlipClock::Start() {
    if (!timer_) {
        timer_ = lv_timer_create(TimerCallback, 1000, this);
        ESP_LOGI(TAG, "Flip clock timer started");
    }
}

void FlipClock::Stop() {
    if (timer_) {
        lv_timer_del(timer_);
        timer_ = nullptr;
    }
}

void FlipClock::TimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<FlipClock*>(lv_timer_get_user_data(timer));
    self->UpdateTime();
}

void FlipClock::SetDigit(lv_obj_t* label, int value) {
    char buf[2];
    snprintf(buf, sizeof(buf), "%d", value);
    const char* old_text = lv_label_get_text(label);
    if (old_text[0] != buf[0]) {
        // 数字变化时添加简单动画效果
        lv_obj_set_style_text_color(label, lv_color_hex(0xFF6B6B), 0);
        lv_label_set_text(label, buf);
        // 100ms 后恢复白色
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    }
}

void FlipClock::UpdateTime() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // 如果时间还没同步（年份异常），显示等待
    if (timeinfo.tm_year < 100) {
        lv_label_set_text(sec_label_, "syncing...");
        return;
    }

    int hour = timeinfo.tm_hour;
    int min = timeinfo.tm_min;
    int sec = timeinfo.tm_sec;

    SetDigit(hour_tens_, hour / 10);
    SetDigit(hour_ones_, hour % 10);
    SetDigit(min_tens_, min / 10);
    SetDigit(min_ones_, min % 10);

    // 秒数
    char sec_buf[8];
    snprintf(sec_buf, sizeof(sec_buf), ":%02d", sec);
    lv_label_set_text(sec_label_, sec_buf);

    // 日期
    char date_buf[32];
    const char* wdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d %s",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             wdays[timeinfo.tm_wday]);
    lv_label_set_text(date_label_, date_buf);

    // 冒号闪烁（每秒切换显隐）
    lv_obj_set_style_text_opa(colon1_, (sec % 2 == 0) ? LV_OPA_COVER : LV_OPA_30, 0);
}
