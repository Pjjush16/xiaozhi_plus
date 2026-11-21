#include "flip_clock.h"
#include "page_manager.h"

#include <esp_log.h>
#include <esp_sntp.h>
#include <time.h>

#define TAG "FlipClock"

// ========== 颜色方案 ==========
// 深蓝渐变背景 + 暗色卡片 + 亮色数字
static constexpr uint32_t kBgTopColor     = 0x0D1117;
static constexpr uint32_t kBgBottomColor  = 0x161B22;
static constexpr uint32_t kCardBgColor    = 0x21262D;
static constexpr uint32_t kDigitColor     = 0xE6EDF3;
static constexpr uint32_t kColonColor     = 0x58A6FF;
static constexpr uint32_t kDateColor      = 0x8B949E;
static constexpr uint32_t kSecColor       = 0x7EE787;
static constexpr uint32_t kBtnColor       = 0x8957E5;

// ========== 字体选择 ==========
// 优先使用 montserrat_36，编译时需开启 CONFIG_LV_FONT_MONTSERRAT_36
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

// ========== 布局参数 (320×240 横屏) ==========
static constexpr lv_coord_t kCardW     = 62;
static constexpr lv_coord_t kCardH     = 100;
static constexpr lv_coord_t kCardR     = 12;
static constexpr lv_coord_t kGapSmall  = 6;
static constexpr lv_coord_t kColonW    = 20;
// 总宽: 4*62 + 2*6 + 20 = 280, 居中偏移 (320-280)/2 = 20
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

    // 创建在 top layer 上，不会被 lv_screen_load 影响
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

    // ── "小智" 按钮（左下角）──
    xiaozhi_btn_ = lv_btn_create(overlay_);
    lv_obj_set_size(xiaozhi_btn_, 56, 32);
    lv_obj_align(xiaozhi_btn_, LV_ALIGN_BOTTOM_LEFT, 12, -10);
    lv_obj_set_style_bg_color(xiaozhi_btn_, lv_color_hex(kBtnColor), 0);
    lv_obj_set_style_radius(xiaozhi_btn_, 16, 0);
    lv_obj_set_style_border_width(xiaozhi_btn_, 0, 0);
    lv_obj_set_style_shadow_width(xiaozhi_btn_, 6, 0);
    lv_obj_set_style_shadow_color(xiaozhi_btn_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(xiaozhi_btn_, LV_OPA_30, 0);

    lv_obj_t* btn_label = lv_label_create(xiaozhi_btn_);
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(btn_label, "小智");
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(xiaozhi_btn_, [](lv_event_t* e) {
        PageManager::GetInstance().SwitchTo(PageId::XIAOZHI);
    }, LV_EVENT_CLICKED, nullptr);

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

    // SNTP 未同步时显示等待提示
    if (timeinfo.tm_year < 100) {
        lv_label_set_text(sec_label_, "同步中...");
        return;
    }

    int hour = timeinfo.tm_hour;
    int min  = timeinfo.tm_min;
    int sec  = timeinfo.tm_sec;

    // 更新各位数字（仅在变化时刷新，减少重绘）
    auto UpdateDigit = [](lv_obj_t* label, int val) {
        char buf[2];
        snprintf(buf, sizeof(buf), "%d", val);
        const char* old = lv_label_get_text(label);
        if (old[0] != buf[0]) {
            lv_label_set_text(label, buf);
        }
    };

    UpdateDigit(hour_tens_label_, hour / 10);
    UpdateDigit(hour_ones_label_, hour % 10);
    UpdateDigit(min_tens_label_,  min / 10);
    UpdateDigit(min_ones_label_,  min % 10);

    // 秒数
    char sec_buf[8];
    snprintf(sec_buf, sizeof(sec_buf), ":%02d", sec);
    lv_label_set_text(sec_label_, sec_buf);

    // 日期（中文星期）
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
