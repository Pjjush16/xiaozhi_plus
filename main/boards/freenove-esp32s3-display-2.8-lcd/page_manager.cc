#include "page_manager.h"
#include "flip_clock.h"
#include "music_player.h"
#include "application.h"

#include <esp_log.h>
#include <time.h>

#define TAG "PageManager"

static uint32_t s_idle_elapsed_ms = 0;

void PageManager::Init() {
    ESP_LOGI(TAG, "PageManager initialized (overlay + gesture mode)");
}

void PageManager::CreateAlarmPage() {
    if (alarm_overlay_) return;

    lv_obj_t* top = lv_layer_top();

    alarm_overlay_ = lv_obj_create(top);
    lv_obj_set_size(alarm_overlay_, 320, 240);
    lv_obj_set_pos(alarm_overlay_, 0, 0);
    lv_obj_set_style_bg_color(alarm_overlay_, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(alarm_overlay_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(alarm_overlay_, 0, 0);
    lv_obj_set_style_radius(alarm_overlay_, 0, 0);
    lv_obj_set_style_pad_all(alarm_overlay_, 0, 0);
    lv_obj_clear_flag(alarm_overlay_, LV_OBJ_FLAG_SCROLLABLE);

    // 标题
    lv_obj_t* title = lv_label_create(alarm_overlay_);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x8B949E), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    lv_label_set_text(title, LV_SYMBOL_BELL " 闹钟");

    // 当前时间（大字）
    alarm_time_label_ = lv_label_create(alarm_overlay_);
    lv_obj_set_style_text_font(alarm_time_label_, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(alarm_time_label_, lv_color_hex(0xE6EDF3), 0);
    lv_obj_align(alarm_time_label_, LV_ALIGN_CENTER, 0, -20);

    // 提示文字
    alarm_hint_label_ = lv_label_create(alarm_overlay_);
    lv_obj_set_style_text_font(alarm_hint_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(alarm_hint_label_, lv_color_hex(0x484F58), 0);
    lv_obj_align(alarm_hint_label_, LV_ALIGN_CENTER, 0, 30);
    lv_label_set_text(alarm_hint_label_, "暂未设置闹钟");

    // 右滑返回提示
    lv_obj_t* back_hint = lv_label_create(alarm_overlay_);
    lv_obj_set_style_text_font(back_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_hint, lv_color_hex(0x484F58), 0);
    lv_obj_set_style_text_opa(back_hint, LV_OPA_50, 0);
    lv_obj_align(back_hint, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_label_set_text(back_hint, LV_SYMBOL_RIGHT " 右滑返回时钟");

    lv_obj_add_flag(alarm_overlay_, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Alarm page created");
}

void PageManager::SwitchTo(PageId page) {
    auto& app = Application::GetInstance();
    app.Schedule([this, page]() {
        if (page == current_page_) return;

        ESP_LOGI(TAG, "Switching: %s -> %s",
                 GetPageName(current_page_), GetPageName(page));

        // 隐藏当前页面
        switch (current_page_) {
            case PageId::FLIP_CLOCK:
                FlipClock::GetInstance().Hide();
                break;
            case PageId::MUSIC_PLAYER:
                MusicPlayer::GetInstance().Hide();
                break;
            case PageId::ALARM:
                if (alarm_overlay_) {
                    lv_obj_add_flag(alarm_overlay_, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(alarm_overlay_, LV_OBJ_FLAG_CLICKABLE);
                }
                break;
            default:
                break;
        }

        // 显示目标页面
        switch (page) {
            case PageId::XIAOZHI:
                // 隐藏所有覆盖层，小智界面自然露出
                break;
            case PageId::FLIP_CLOCK:
                FlipClock::GetInstance().Show();
                break;
            case PageId::MUSIC_PLAYER:
                MusicPlayer::GetInstance().Show();
                break;
            case PageId::ALARM:
                if (!alarm_overlay_) CreateAlarmPage();
                lv_obj_remove_flag(alarm_overlay_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(alarm_overlay_, LV_OBJ_FLAG_CLICKABLE);
                // 更新闹钟页面上的当前时间
                if (alarm_time_label_) {
                    time_t now;
                    struct tm timeinfo;
                    time(&now);
                    localtime_r(&now, &timeinfo);
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
                    lv_label_set_text(alarm_time_label_, buf);
                }
                break;
            default:
                break;
        }

        current_page_ = page;
        ResetIdleTimer();
    });
}

void PageManager::OnWakeWordTriggered() {
    if (current_page_ != PageId::XIAOZHI) {
        SwitchTo(PageId::XIAOZHI);
    }
    ResetIdleTimer();
}

void PageManager::SetIdleTimeout(uint32_t seconds) {
    idle_timeout_sec_ = seconds;
    if (idle_timer_) {
        lv_timer_del(idle_timer_);
        idle_timer_ = nullptr;
    }
    if (seconds > 0) {
        idle_timer_ = lv_timer_create(IdleTimerCallback, 1000, this);
        ESP_LOGI(TAG, "Idle timeout set to %u seconds", seconds);
    }
}

void PageManager::ResetIdleTimer() {
    s_idle_elapsed_ms = 0;
    if (idle_timer_) {
        lv_timer_reset(idle_timer_);
    }
}

void PageManager::IdleTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<PageManager*>(lv_timer_get_user_data(timer));
    if (self->idle_timeout_sec_ == 0) return;

    s_idle_elapsed_ms += 1000;
    if (s_idle_elapsed_ms >= self->idle_timeout_sec_ * 1000) {
        if (self->current_page_ == PageId::XIAOZHI) {
            ESP_LOGI(TAG, "Idle timeout (%us), switching to flip clock",
                     self->idle_timeout_sec_);
            self->SwitchTo(PageId::FLIP_CLOCK);
        }
    }
}

const char* PageManager::GetPageName(PageId page) {
    switch (page) {
        case PageId::XIAOZHI:      return "xiaozhi";
        case PageId::FLIP_CLOCK:   return "flip_clock";
        case PageId::MUSIC_PLAYER: return "music_player";
        case PageId::ALARM:        return "alarm";
        default:                   return "unknown";
    }
}
