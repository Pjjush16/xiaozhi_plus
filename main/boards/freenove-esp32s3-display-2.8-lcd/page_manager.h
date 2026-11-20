#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include <string>
#include <functional>
#include <lvgl.h>

enum class PageId {
    XIAOZHI = 0,
    FLIP_CLOCK,
    MUSIC_PLAYER,
    PAGE_COUNT
};

class PageManager {
public:
    static PageManager& GetInstance() {
        static PageManager instance;
        return instance;
    }

    // 初始化页面管理器，保存小智主屏幕引用
    void Init(lv_obj_t* xiaozhi_screen);

    // 切换到指定页面（线程安全，内部会 Schedule 到主线程）
    void SwitchTo(PageId page);

    // 获取当前页面
    PageId GetCurrentPage() const { return current_page_; }

    // 获取页面名称
    static const char* GetPageName(PageId page);

    // 注册页面创建/销毁回调
    using PageCreateFunc = std::function<lv_obj_t*()>;
    using PageDestroyFunc = std::function<void(lv_obj_t*)>;

    void RegisterPage(PageId page, PageCreateFunc create, PageDestroyFunc destroy = nullptr);

    // 供唤醒词回调使用：当唤醒词触发时切回小智页面
    void OnWakeWordTriggered();

private:
    PageManager() = default;

    lv_obj_t* xiaozhi_screen_ = nullptr;
    lv_obj_t* current_custom_screen_ = nullptr;
    PageId current_page_ = PageId::XIAOZHI;

    struct PageInfo {
        PageCreateFunc create;
        PageDestroyFunc destroy;
        lv_obj_t* screen = nullptr;
    };

    PageInfo pages_[static_cast<int>(PageId::PAGE_COUNT)] = {};

    void DoSwitch(PageId page);
};

#endif // PAGE_MANAGER_H
