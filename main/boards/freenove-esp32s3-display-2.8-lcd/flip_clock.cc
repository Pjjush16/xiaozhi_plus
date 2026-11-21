#include "flip_clock.h"

#include <esp_log.h>
#include <esp_sntp.h>
#include <time.h>

#define TAG "FlipClock"

// ========== 颜色方案 ==========
static constexpr uint32_t kBgTopColor     = 0x0D1117;
static constexpr uint32_t kBgBottomColor  = 0x161B22;
static constexpr uint32_t kCardBgColor    = 0x21262D;
static constexpr uint32_t kDigitColor     = 0xE6EDF3;
static constexpr uint32_t kColonColor     = 0x58A6FF;
static constexpr uint32_t kDateColor      = 0x8B949E;
static constexpr uint32_t kSecColor       = 0x7EE787;
static constexpr uint32_t kArrowColor     = 0x484F58;  // 低调灰色箭头

// ========== 字体选择 ==========
#if LV_FONT_MONTSERRAT_36
  #define DIGIT_FONT lv_font_montserrat_36
#elif LV_FONT_MONTSERRAT_24
  #define DIGIT_FONT lv_font_montserrat_24
#else
  #define DIGIT_FONT lv_font_montserrat_14
#endif

#if LV_FONT_MONTSERRAT_24
  #define COLON_FONT lv_font_montserrat_24
#else
  #define COLON_FONT lv_font_montserrat_14
#endif

#if LV_FONT_MONTSERRAT_48
  #define ARROW_FONT lv_font_montserrat_48
#elif LV_FONT_MONTSERRAT_36
  #define ARROW_FONT lv_font_montserrat_36
#else
  #define ARROW_FONT lv_font_montserrat_24
#endif

// ========== 布局参数 (320×240 横屏) ==========
static constexpr lv_coord_t kCardW     = 62;
static constexpr lv_coord_t kCardH     = 100;
static constexpr lv_coord_t kCardR     = 12;
static constexpr lv_coord_t kGapSmall  = 6;
static constexpr lv_coord_t kColonW    = 20;
static constexpr lv_coord_t kXStart    = 20;
static constexpr lv_coord_t kCardY     = 50;

lv_obj_t* FlipClock::CreateDigitCard(lv_obj_t* parent, lv_coord_t x, lv_coord_t y) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, kCardW, kCardH);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(kCardBgColor), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, kCardR, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 8, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_40, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* label = lv_label_create(card);
    lv_obj_set_style_text_font(label, &DIGIT_FONT, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(kDigitColor), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(label, "0");

    return label;
}

void FlipClock::Create() {
    if (overlay_) return;

    lv_obj_t* top = lv_layer_top();

    overlay_ = lv_obj_create(top);
    lv_obj_set_size(overlay_, 320, 240);
    lv_obj_set_pos(overlay_, 0, 0);
    lv_obj_set_style_bg_color(overlay_, lv_color_hex(kBgTopColor), 0);
    lv_obj_set_style_bg_grad_color(overlay_, lv_color_hex(kBgBottomColor), 0);
    lv_obj_set_style_bg_grad_dir(overlay_, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay_, 0, 0);
    lv_obj_set_style_radius(overlay_, 0, 0);
    lv_obj_set_style_pad_all(overlay_, 0, 0);
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLLABLE);

    // ── 日期标签（顶部居中）──
    date_label_ = lv_label_create(overlay_);
    lv_obj_set_style_text_font(date_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(date_label_, lv_color_hex(kDateColor), 0);
    lv_obj_align(date_label_, LV_ALIGN_TOP_MID, 0, 14);
    lv_label_set_text(date_label_, "----年--月--日 ---");

    // ── 数字卡片 ──
    lv_coord_t x = kXStart;
    hour_tens_label_ = CreateDigitCard(overlay_, x, kCardY);
    hour_tens_card_  = lv_obj_get_parent(hour_tens_label_);
    x += kCardW + kGapSmall;

    hour_ones_label_ = CreateDigitCard(overlay_, x, kCardY);
    hour_ones_card_  = lv_obj_get_parent(hour_ones_label_);
    x += kCardW;

    // 冒号
    colon_label_ = lv_label_create(overlay_);
    lv_obj_set_style_text_font(colon_label_, &COLON_FONT, 0);
    lv_obj_set_style_text_color(colon_label_, lv_color_hex(kColonColor), 0);
    lv_obj_set_pos(colon_label_, x, kCardY + (kCardH - 30) / 2);
    lv_label_set_text(colon_label_, ":");
    x += kColonW;

    min_tens_label_ = CreateDigitCard(overlay_, x, kCardY);
    min_tens_card_  = lv_obj_get_parent(min_tens_label_);
    x += kCardW + kGapSmall;

    min_ones_label_ = CreateDigitCard(overlay_, x, kCardY);
    min_ones_card_  = lv_obj_get_parent(min_ones_label_);

    // ── 秒数标签（底部偏右）──
    sec_label_ = lv_label_create(overlay_);
    lv_obj_set_style_text_font(sec_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sec_label_, lv_color_hex(kSecColor), 0);
    lv_obj_align(sec_label_, LV_ALIGN_BOTTOM_RIGHT, -16, -12);
    lv_label_set_text(sec_label_, ":00");

    // ── 方向箭头提示（纯视觉提示，不可点击）──
    // 左侧 ← 左滑去闹钟
    arrow_left_ = lv_label_create(overlay_);
    lv_obj_set_style_text_font(arrow_left_, &ARROW_FONT, 0);
    lv_obj_set_style_text_color(arrow_left_, lv_color_hex(kArrowColor), 0);
    lv_obj_set_style_text_opa(arrow_left_, LV_OPA_50, 0);
    lv_obj_align(arrow_left_, LV_ALIGN_LEFT_MID, 8, 0);
    lv_label_set_text(arrow_left_, LV_SYMBOL_LEFT);

    // 右侧 → 右滑去音乐
    arrow_right_ = lv_label_create(overlay_);
    lv_obj_set_style_text_font(arrow_right_, &ARROW_FONT, 0);
    lv_obj_set_style_text_color(arrow_right_, lv_color_hex(kArrowColor), 0);
    lv_obj_set_style_text_opa(arrow_right_, LV_OPA_50, 0);
    lv_obj_align(arrow_right_, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_label_set_text(arrow_right_, LV_SYMBOL_RIGHT);

    // 底部 ↓ 上滑去小智
    arrow_bottom_ = lv_label_create(overlay_);
    lv_obj_set_style_text_font(arrow_bottom_, &ARROW_FONT, 0);
    lv_obj_set_style_text_color(arrow_bottom_, lv_color_hex(kArrowColor), 0);
    lv_obj_set_style_text_opa(arrow_bottom_, LV_OPA_50, 0);
    lv_obj_align(arrow_bottom_, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_label_set_text(arrow_bottom_, LV_SYMBOL_DOWN);

    // ── 初始化 SNTP ──
    if (esp_sntp_enabled() == false) {
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "ntp.aliyun.com");
        esp_sntp_setservername(2, "cn.pool.ntp.org");
        esp_sntp_init();
    }
    setenv("TZ", "CST-8", 1);
    tzset();

    UpdateTime();
    ESP_LOGI(TAG, "Flip clock overlay created");
}

void FlipClock::Show() {
    if (!overlay_) Create();
    lv_obj_remove_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
    visible_ = true;
    Start();
    ESP_LOGI(TAG, "Flip clock shown");
}

void FlipClock::Hide() {
    if (overlay_) {
        lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
    }
    visible_ = false;
    Stop();
    ESP_LOGI(TAG, "Flip clock hidden");
}

void FlipClock::Start() {
    if (!timer_) {
        timer_ = lv_timer_create(TimerCallback, 1000, this);
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

void FlipClock::UpdateTime() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < 100) {
        lv_label_set_text(sec_label_, "同步中...");
        return;
    }

    int hour = timeinfo.tm_hour;
    int min  = timeinfo.tm_min;
    int sec  = timeinfo.tm_sec;

    auto UpdateDigit = [](lv_obj_t* label, int val) {
        char buf[4];
        buf[0] = '0' + (val % 10);
        buf[1] = '\0';
        const char* old = lv_label_get_text(label);
        if (old[0] != buf[0]) {
            lv_label_set_text(label, buf);
        }
    };

    UpdateDigit(hour_tens_label_, hour / 10);
    UpdateDigit(hour_ones_label_, hour % 10);
    UpdateDigit(min_tens_label_,  min / 10);
    UpdateDigit(min_ones_label_,  min % 10);

    char sec_buf[8];
    snprintf(sec_buf, sizeof(sec_buf), ":%02d", sec);
    lv_label_set_text(sec_label_, sec_buf);

    const char* wdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    char date_buf[48];
    snprintf(date_buf, sizeof(date_buf), "%04d年%02d月%02d日 %s",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             wdays[timeinfo.tm_wday]);
    lv_label_set_text(date_label_, date_buf);

    // 冒号闪烁
    lv_obj_set_style_text_opa(colon_label_,
        (sec % 2 == 0) ? LV_OPA_COVER : LV_OPA_30, 0);
}
