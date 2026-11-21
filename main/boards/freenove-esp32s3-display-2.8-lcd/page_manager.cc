#include "page_manager.h"
#include "flip_clock.h"
#include "music_player.h"
#include "application.h"

#include <esp_log.h>

#define TAG "PageManager"

void PageManager::Init() {
    // 覆盖层模式无需保存 screen 引用
    ESP_LOGI(TAG, "PageManager initialized (overlay mode)");
}

void PageManager::SwitchTo(PageId page) {
    auto& app = Application::GetInstance();
    app.Schedule([this, page]() {
        if (page == current_page_) return;

        ESP_LOGI(TAG, "Switching: %s -> %s",
                 GetPageName(current_page_), GetPageName(page));

        // 隐藏当前自定义页面
        if (current_page_ == PageId::FLIP_CLOCK) {
            FlipClock::GetInstance().Hide();
        } else if (current_page_ == PageId::MUSIC_PLAYER) {
            MusicPlayer::GetInstance().Hide();
        }

        // 显示目标页面
        if (page == PageId::XIAOZHI) {
            // 什么都不做 — 隐藏覆盖层后小智界面自然露出
        } else if (page == PageId::FLIP_CLOCK) {
            FlipClock::GetInstance().Show();
        } else if (page == PageId::MUSIC_PLAYER) {
            MusicPlayer::GetInstance().Show();
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
        // 每秒检查一次空闲状态
        idle_timer_ = lv_timer_create(IdleTimerCallback, 1000, this);
        ESP_LOGI(TAG, "Idle timeout set to %u seconds", seconds);
    }
}

void PageManager::ResetIdleTimer() {
    if (idle_timer_) {
        // 重置计时器 — 通过删除重建来实现
        lv_timer_reset(idle_timer_);
    }
}

void PageManager::IdleTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<PageManager*>(lv_timer_get_user_data(timer));
    if (self->idle_timeout_sec_ == 0) return;

    // lv_timer 的 period 是 1000ms, 用 repeat_count 计数
    // 简单方案：检查 lv_display 的 last_activity_time
    uint32_t idle_ms = lv_tick_elaps(lv_disp_get_default()->last_activity_time);
    if (idle_ms >= self->idle_timeout_sec_ * 1000) {
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
        default:                   return "unknown";
    }
}
