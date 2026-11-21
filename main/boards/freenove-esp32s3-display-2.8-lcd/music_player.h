#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

#include <lvgl.h>
#include <string>
#include <vector>
#include <esp_http_client.h>

struct SongInfo {
    std::string id;
    std::string title;
    std::string artist;
    std::string url;
};

class MusicPlayer {
public:
    static MusicPlayer& GetInstance() {
        static MusicPlayer instance;
        return instance;
    }

    /** 创建覆盖层 UI（幂等） */
    void Create();

    /** 显示音乐播放器覆盖层 */
    void Show();

    /** 隐藏音乐播放器覆盖层 */
    void Hide();

    /** 启动播放器 */
    void Start();
    void Stop();

    /** 当前是否正在显示 */
    bool IsVisible() const { return visible_; }

    // 播放控制
    void Play();
    void Pause();
    void TogglePlayPause();
    void NextSong();
    void PrevSong();
    void SetVolume(int vol);

    // 获取状态
    bool IsPlaying() const { return is_playing_; }
    const SongInfo& GetCurrentSong() const { return playlist_[current_index_]; }

private:
    MusicPlayer() = default;

    // 覆盖层根面板（在 lv_layer_top 上）
    lv_obj_t* overlay_ = nullptr;
    bool visible_ = false;

    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* artist_label_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* play_btn_ = nullptr;
    lv_obj_t* prev_btn_ = nullptr;
    lv_obj_t* next_btn_ = nullptr;
    lv_obj_t* volume_slider_ = nullptr;
    lv_obj_t* volume_label_ = nullptr;
    lv_obj_t* back_btn_ = nullptr;
    lv_obj_t* list_container_ = nullptr;

    std::vector<SongInfo> playlist_;
    int current_index_ = 0;
    bool is_playing_ = false;
    int volume_ = 50;

    esp_http_client_handle_t http_client_ = nullptr;
    TaskHandle_t play_task_ = nullptr;

    static const char* API_BASE; // music.yaonet.de5.net

    void LoadPlaylist();
    void UpdateUI();
    void StartPlayback();
    void StopPlayback();

    // LVGL 事件回调
    static void OnPlayBtn(lv_event_t* e);
    static void OnPrevBtn(lv_event_t* e);
    static void OnNextBtn(lv_event_t* e);
    static void OnVolumeChanged(lv_event_t* e);
    static void OnSongClick(lv_event_t* e);

    // HTTP 播放任务
    static void PlayTask(void* arg);
};

#endif // MUSIC_PLAYER_H
