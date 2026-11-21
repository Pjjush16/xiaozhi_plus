#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include <string>
#include <functional>
#include <lvgl.h>

enum class PageId {
    XIAOZHI = 0,    // 小智 AI 对话主界面
    FLIP_CLOCK,     // 翻页时钟（覆盖层）
    MUSIC_PLAYER,   // 音乐播放器（覆盖层）
    PAGE_COUNT
};

/**
 * 页面管理器 — 覆盖层架构
 *
 * 小智主界面始终在底层 screen 上，自定义页面（翻页时钟、
 * 音乐播放器）创建在 lv_layer_top() 覆盖层中，通过
 * Show / Hide 切换可见性，避免 lv_screen_load 销毁主界面。
 */
class PageManager {
public:
    static PageManager& GetInstance() {
        static PageManager instance;
        return instance;
    }

    /** 初始化（无需传入 screen 引用，覆盖层模式下不需要） */
    void Init();

    /** 切换到指定页面（线程安全，内部 Schedule 到主线程） */
    void SwitchTo(PageId page);

    /** 获取当前页面 */
    PageId GetCurrentPage() const { return current_page_; }

    /** 获取页面名称 */
    static const char* GetPageName(PageId page);

    /** 供唤醒词回调使用：当唤醒词触发时切回小智页面 */
    void OnWakeWordTriggered();

    /** 设置空闲超时（秒），0 表示禁用。超时后自动切到翻页时钟 */
    void SetIdleTimeout(uint32_t seconds);

    /** 重置空闲计时器（用户触摸、按键、AI 交互时调用） */
    void ResetIdleTimer();

private:
    PageManager() = default;

    PageId current_page_ = PageId::XIAOZHI;
    uint32_t idle_timeout_sec_ = 0;
    lv_timer_t* idle_timer_ = nullptr;

    static void IdleTimerCallback(lv_timer_t* timer);
};

#endif // PAGE_MANAGER_H
