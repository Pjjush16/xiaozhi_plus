#include "music_player.h"
#include "page_manager.h"
#include "application.h"
#include "board.h"

#include <esp_log.h>
#include <esp_http_client.h>
#include <cJSON.h>
#include <cstring>

#define TAG "MusicPlayer"

// music.yaonet.de5.net API 基地址
const char* MusicPlayer::API_BASE = "https://music.yaonet.de5.net";

lv_obj_t* MusicPlayer::Create() {
    if (screen_) return screen_;

    screen_ = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x0F0F1A), 0);

    // 标题
    lv_obj_t* header = lv_label_create(screen_);
    lv_obj_set_style_text_font(header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(header, lv_color_hex(0x8B5CF6), 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 8);
    lv_label_set_text(header, LV_SYMBOL_AUDIO " YAONET Music");

    // 歌曲名
    title_label_ = lv_label_create(screen_);
    lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(title_label_, 280);
    lv_label_set_long_mode(title_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(title_label_, LV_ALIGN_TOP_MID, 0, 36);
    lv_label_set_text(title_label_, "Loading...");

    // 艺术家
    artist_label_ = lv_label_create(screen_);
    lv_obj_set_style_text_font(artist_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(artist_label_, lv_color_hex(0x888888), 0);
    lv_obj_set_width(artist_label_, 280);
    lv_label_set_long_mode(artist_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(artist_label_, LV_ALIGN_TOP_MID, 0, 56);
    lv_label_set_text(artist_label_, "");

    // 控制按钮区域 (y=80)
    int btn_y = 82;
    int btn_size = 40;

    // 上一首
    prev_btn_ = lv_btn_create(screen_);
    lv_obj_set_size(prev_btn_, btn_size, btn_size);
    lv_obj_set_pos(prev_btn_, 60, btn_y);
    lv_obj_set_style_bg_color(prev_btn_, lv_color_hex(0x333344), 0);
    lv_obj_set_style_radius(prev_btn_, 20, 0);
    lv_obj_t* prev_lbl = lv_label_create(prev_btn_);
    lv_label_set_text(prev_lbl, LV_SYMBOL_PREV);
    lv_obj_center(prev_lbl);
    lv_obj_add_event_cb(prev_btn_, OnPrevBtn, LV_EVENT_CLICKED, this);

    // 播放/暂停
    play_btn_ = lv_btn_create(screen_);
    lv_obj_set_size(play_btn_, btn_size + 10, btn_size + 10);
    lv_obj_set_pos(play_btn_, 130, btn_y - 5);
    lv_obj_set_style_bg_color(play_btn_, lv_color_hex(0x8B5CF6), 0);
    lv_obj_set_style_radius(play_btn_, 25, 0);
    lv_obj_t* play_lbl = lv_label_create(play_btn_);
    lv_obj_set_style_text_font(play_lbl, &lv_font_montserrat_24, 0);
    lv_label_set_text(play_lbl, LV_SYMBOL_PLAY);
    lv_obj_center(play_lbl);
    lv_obj_add_event_cb(play_btn_, OnPlayBtn, LV_EVENT_CLICKED, this);

    // 下一首
    next_btn_ = lv_btn_create(screen_);
    lv_obj_set_size(next_btn_, btn_size, btn_size);
    lv_obj_set_pos(next_btn_, 220, btn_y);
    lv_obj_set_style_bg_color(next_btn_, lv_color_hex(0x333344), 0);
    lv_obj_set_style_radius(next_btn_, 20, 0);
    lv_obj_t* next_lbl = lv_label_create(next_btn_);
    lv_label_set_text(next_lbl, LV_SYMBOL_NEXT);
    lv_obj_center(next_lbl);
    lv_obj_add_event_cb(next_btn_, OnNextBtn, LV_EVENT_CLICKED, this);

    // 音量滑块
    volume_label_ = lv_label_create(screen_);
    lv_obj_set_style_text_font(volume_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(volume_label_, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(volume_label_, LV_ALIGN_BOTTOM_LEFT, 8, -48);
    lv_label_set_text(volume_label_, LV_SYMBOL_VOLUME_MAX " 50");

    volume_slider_ = lv_slider_create(screen_);
    lv_obj_set_width(volume_slider_, 200);
    lv_obj_align(volume_slider_, LV_ALIGN_BOTTOM_LEFT, 60, -44);
    lv_slider_set_range(volume_slider_, 0, 100);
    lv_slider_set_value(volume_slider_, volume_, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(0x8B5CF6), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_add_event_cb(volume_slider_, OnVolumeChanged, LV_EVENT_VALUE_CHANGED, this);

    // 播放列表区域
    list_container_ = lv_obj_create(screen_);
    lv_obj_set_size(list_container_, 300, 60);
    lv_obj_align(list_container_, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(list_container_, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(list_container_, 0, 0);
    lv_obj_set_style_radius(list_container_, 8, 0);
    lv_obj_set_style_pad_all(list_container_, 2, 0);
    lv_obj_set_scroll_dir(list_container_, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(list_container_, LV_SCROLLBAR_MODE_OFF);

    // 状态标签
    status_label_ = lv_label_create(screen_);
    lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_hex(0x666666), 0);
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_RIGHT, -8, -68);
    lv_label_set_text(status_label_, "");

    // 左下角返回按钮
    back_btn_ = lv_btn_create(screen_);
    lv_obj_set_size(back_btn_, 50, 30);
    lv_obj_align(back_btn_, LV_ALIGN_BOTTOM_LEFT, 8, -8);
    lv_obj_set_style_bg_color(back_btn_, lv_color_hex(0x444466), 0);
    lv_obj_set_style_radius(back_btn_, 15, 0);
    lv_obj_t* back_lbl = lv_label_create(back_btn_);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_12, 0);
    lv_label_set_text(back_lbl, LV_SYMBOL_HOME);
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn_, [](lv_event_t* e) {
        PageManager::GetInstance().SwitchTo(PageId::XIAOZHI);
    }, LV_EVENT_CLICKED, nullptr);

    // 加载播放列表
    LoadPlaylist();
    UpdateUI();

    ESP_LOGI(TAG, "Music player page created, %d songs loaded", (int)playlist_.size());
    return screen_;
}

void MusicPlayer::LoadPlaylist() {
    // 从 music.yaonet.de5.net 获取歌曲列表
    // 先加载一些示例歌曲（后续对接真实API）
    playlist_.clear();
    playlist_.push_back({"1", "Higher", "Tobu", ""});
    playlist_.push_back({"2", "Candyland", "Tobu", ""});
    playlist_.push_back({"3", "Hope", "Tobu", ""});
    playlist_.push_back({"4", "Cloud 9", "Tobu & Itro", ""});
    playlist_.push_back({"5", "Sunburst", "Tobu & Itro", ""});
    playlist_.push_back({"6", "Life", "Tobu", ""});
    playlist_.push_back({"7", "Colors", "Tobu", ""});
    playlist_.push_back({"8", "Infectious", "Tobu", ""});

    // 更新播放列表 UI
    if (list_container_) {
        lv_obj_clean(list_container_);
        for (int i = 0; i < (int)playlist_.size(); i++) {
            lv_obj_t* item = lv_btn_create(list_container_);
            lv_obj_set_size(item, 80, 50);
            lv_obj_set_style_bg_color(item,
                (i == current_index_) ? lv_color_hex(0x8B5CF6) : lv_color_hex(0x2A2A3A), 0);
            lv_obj_set_style_radius(item, 8, 0);
            lv_obj_set_style_border_width(item, 0, 0);

            lv_obj_t* lbl = lv_label_create(item);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
            lv_obj_set_width(lbl, 72);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
            lv_label_set_text(lbl, playlist_[i].title.c_str());
            lv_obj_center(lbl);

            lv_obj_add_event_cb(item, OnSongClick, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        }
    }
}

void MusicPlayer::UpdateUI() {
    if (!title_label_) return;

    if (playlist_.empty()) {
        lv_label_set_text(title_label_, "No songs");
        lv_label_set_text(artist_label_, "");
        return;
    }

    const auto& song = playlist_[current_index_];
    lv_label_set_text(title_label_, song.title.c_str());
    lv_label_set_text(artist_label_, song.artist.c_str());

    // 更新播放按钮图标
    lv_obj_t* play_lbl = lv_obj_get_child(play_btn_, 0);
    lv_label_set_text(play_lbl, is_playing_ ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);

    // 更新状态
    if (status_label_) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d/%d", current_index_ + 1, (int)playlist_.size());
        lv_label_set_text(status_label_, buf);
    }

    // 更新播放列表高亮
    if (list_container_) {
        int count = lv_obj_get_child_count(list_container_);
        for (int i = 0; i < count; i++) {
            lv_obj_t* item = lv_obj_get_child(list_container_, i);
            lv_obj_set_style_bg_color(item,
                (i == current_index_) ? lv_color_hex(0x8B5CF6) : lv_color_hex(0x2A2A3A), 0);
        }
    }
}

void MusicPlayer::OnPlayBtn(lv_event_t* e) {
    auto* self = static_cast<MusicPlayer*>(lv_event_get_user_data(e));
    self->TogglePlayPause();
}

void MusicPlayer::OnPrevBtn(lv_event_t* e) {
    auto* self = static_cast<MusicPlayer*>(lv_event_get_user_data(e));
    self->PrevSong();
}

void MusicPlayer::OnNextBtn(lv_event_t* e) {
    auto* self = static_cast<MusicPlayer*>(lv_event_get_user_data(e));
    self->NextSong();
}

void MusicPlayer::OnVolumeChanged(lv_event_t* e) {
    auto* self = static_cast<MusicPlayer*>(lv_event_get_user_data(e));
    self->volume_ = lv_slider_get_value(self->volume_slider_);
    char buf[16];
    snprintf(buf, sizeof(buf), LV_SYMBOL_VOLUME_MAX " %d", self->volume_);
    lv_label_set_text(self->volume_label_, buf);

    // 同步到硬件音量
    auto& board = Board::GetInstance();
    auto codec = board.GetAudioCodec();
    if (codec) codec->SetOutputVolume(self->volume_);
}

void MusicPlayer::OnSongClick(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    auto& self = MusicPlayer::GetInstance();
    if (idx >= 0 && idx < (int)self.playlist_.size()) {
        self.current_index_ = idx;
        self.is_playing_ = false;
        self.UpdateUI();
        self.Play();
    }
}

void MusicPlayer::Play() {
    if (playlist_.empty()) return;
    is_playing_ = true;
    UpdateUI();
    ESP_LOGI(TAG, "Playing: %s - %s", playlist_[current_index_].title.c_str(),
             playlist_[current_index_].artist.c_str());
}

void MusicPlayer::Pause() {
    is_playing_ = false;
    UpdateUI();
    ESP_LOGI(TAG, "Paused");
}

void MusicPlayer::TogglePlayPause() {
    if (is_playing_) Pause();
    else Play();
}

void MusicPlayer::NextSong() {
    if (playlist_.empty()) return;
    current_index_ = (current_index_ + 1) % playlist_.size();
    is_playing_ = true;
    UpdateUI();
    Play();
}

void MusicPlayer::PrevSong() {
    if (playlist_.empty()) return;
    current_index_ = (current_index_ - 1 + playlist_.size()) % playlist_.size();
    is_playing_ = true;
    UpdateUI();
    Play();
}

void MusicPlayer::SetVolume(int vol) {
    volume_ = vol;
    if (volume_slider_) lv_slider_set_value(volume_slider_, vol, LV_ANIM_ON);
    char buf[16];
    snprintf(buf, sizeof(buf), LV_SYMBOL_VOLUME_MAX " %d", vol);
    if (volume_label_) lv_label_set_text(volume_label_, buf);
}

void MusicPlayer::Start() {
    ESP_LOGI(TAG, "Music player started");
}

void MusicPlayer::Stop() {
    is_playing_ = false;
    ESP_LOGI(TAG, "Music player stopped");
}

void MusicPlayer::StartPlayback() {}
void MusicPlayer::StopPlayback() {}
