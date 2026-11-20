#include "page_manager.h"
#include "application.h"
#include <esp_log.h>

#define TAG "PageManager"

void PageManager::Init(lv_obj_t* xiaozhi_screen) {
    xiaozhi_screen_ = xiaozhi_screen;
    ESP_LOGI(TAG, "PageManager initialized with xiaozhi screen");
}

void PageManager::RegisterPage(PageId page, PageCreateFunc create, PageDestroyFunc destroy) {
    int idx = static_cast<int>(page);
    if (idx >= 0 && idx < static_cast<int>(PageId::PAGE_COUNT)) {
        pages_[idx].create = create;
        pages_[idx].destroy = destroy;
    }
}

void PageManager::SwitchTo(PageId page) {
    auto& app = Application::GetInstance();
    app.Schedule([this, page]() {
        DoSwitch(page);
    });
}

void PageManager::DoSwitch(PageId page) {
    if (page == current_page_) return;

    ESP_LOGI(TAG, "Switching to page: %s", GetPageName(page));

    if (page == PageId::XIAOZHI) {
        // 切回小智主页面
        if (xiaozhi_screen_) {
            lv_screen_load(xiaozhi_screen_);
        }
        // 不销毁自定义页面，下次切换时重用
        current_page_ = PageId::XIAOZHI;
        return;
    }

    int idx = static_cast<int>(page);
    auto& info = pages_[idx];

    if (!info.create) {
        ESP_LOGW(TAG, "No create function registered for page %s", GetPageName(page));
        return;
    }

    // 创建或复用页面
    if (!info.screen) {
        info.screen = info.create();
        if (!info.screen) {
            ESP_LOGE(TAG, "Failed to create page %s", GetPageName(page));
            return;
        }
    }

    // 切换到新页面
    lv_screen_load(info.screen);
    current_page_ = page;
}

void PageManager::OnWakeWordTriggered() {
    // 唤醒词触发时，如果不在小智页面，切回小智
    if (current_page_ != PageId::XIAOZHI) {
        SwitchTo(PageId::XIAOZHI);
    }
}

const char* PageManager::GetPageName(PageId page) {
    switch (page) {
        case PageId::XIAOZHI: return "xiaozhi";
        case PageId::FLIP_CLOCK: return "flip_clock";
        case PageId::MUSIC_PLAYER: return "music_player";
        default: return "unknown";
    }
}
