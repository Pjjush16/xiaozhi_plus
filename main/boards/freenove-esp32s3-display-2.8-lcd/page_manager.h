#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include <string>
#include <functional>
#include <lvgl.h>

enum class PageId {
    XIAOZHI = 0,    // 小智 AI 对话主界面（底层 screen）
    FLIP_CLOCK,     // 翻页时钟（默认待机页，覆盖层）
    MUSIC_PLAYER,   // 音乐播放器（覆盖层，右滑进入）
    ALARM,          // 闹钟（覆盖层，左滑进入）
    PAGE_COUNT
};

/**
 * 页面管理器 — 覆盖层 + 手势导航架构
 *
 * 翻页时钟是默认页面，通过滑动手势切换：
 *   左滑 → 闹钟    右滑 → 音乐    上滑 → 小智
 * 小智主界面始终在底层 screen 上，其他页面在 lv_layer_top() 覆盖层。
 */
class PageManager {
public:
    static PageManager& GetInstance() {
        static PageManager instance;
        return instance;
    }

    void Init();
    void SwitchTo(PageId page);
    PageId GetCurrentPage() const { return current_page_; }
    static const char* GetPageName(PageId page);

    void OnWakeWordTriggered();
    void SetIdleTimeout(uint32_t seconds);
    void ResetIdleTimer();

private:
    PageManager() = default;

    PageId current_page_ = PageId::XIAOZHI;
    uint32_t idle_timeout_sec_ = 0;
    lv_timer_t* idle_timer_ = nullptr;

    // 闹钟覆盖层
    lv_obj_t* alarm_overlay_ = nullptr;
    lv_obj_t* alarm_time_label_ = nullptr;
    lv_obj_t* alarm_hint_label_ = nullptr;
    void CreateAlarmPage();

    static void IdleTimerCallback(lv_timer_t* timer);
};

#endif // PAGE_MANAGER_H
