#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#include <wifi_station.h>
#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "adc_battery_monitor.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

#include "led/single_led.h"
#include "system_reset.h"
#include "esp_lcd_ili9341.h"

// 自定义页面
#include "page_manager.h"
#include "flip_clock.h"
#include "music_player.h"

#define TAG "FreenoveESP32S3Display"

class TouchDriver {
public:
    TouchDriver() : dev_(nullptr) {}

    bool Init(i2c_master_bus_handle_t bus, uint8_t addr) {
        i2c_device_config_t cfg = {
            .device_address = addr,
            .scl_speed_hz = 400000,
            .scl_wait_us = 0,
        };
        return i2c_master_bus_add_device(bus, &cfg, &dev_) == ESP_OK;
    }

    bool Read(bool &touched, uint16_t &x, uint16_t &y) {
        touched = false;
        x = y = 0;
        if (!dev_) return false;

        uint8_t reg = 0x02;
        uint8_t buf[5];
        if (i2c_master_transmit_receive(dev_, &reg, 1, buf, 5, 50) != ESP_OK) return false;

        uint8_t points = buf[0] & 0x0F;
        if (points == 0) return true;

        touched = true;
        x = ((buf[1] & 0x0F) << 8) | buf[2];
        y = ((buf[3] & 0x0F) << 8) | buf[4];
        return true;
    }

private:
    i2c_master_dev_handle_t dev_;
};

class FreenoveESP32S3Display : public WifiBoard {
private:
    Button boot_button_;
    LcdDisplay *display_;
    i2c_master_bus_handle_t codec_i2c_bus_;
    TouchDriver touch_;
    AdcBatteryMonitor* adc_battery_monitor_;
    bool clock_active_ = false;

    void InitializeBatteryMonitor() {
        adc_battery_monitor_ = new AdcBatteryMonitor(ADC_UNIT_1, ADC_CHANNEL_8, 200000, 200000, GPIO_NUM_NC);
    }

    static void TouchTask(void *arg) {
        auto *self = static_cast<FreenoveESP32S3Display*>(arg);
        auto &app = Application::GetInstance();

        uint32_t last_tap = 0;
        uint32_t down_start = 0;
        bool down = false;

        while (true) {
            bool t;
            uint16_t x, y;
            self->touch_.Read(t, x, y);

            uint32_t now = esp_timer_get_time() / 1000;

            if (t) {
                if (!down) {
                    down = true;
                    down_start = now;
                }
            }

            if (!t && down) {
                down = false;
                uint32_t press = now - down_start;

                if (press > 3000) {
                    self->EnterWifiConfigMode();
                } else {
                    // 如果当前在自定义页面，双击切回小智
                    if (PageManager::GetInstance().GetCurrentPage() != PageId::XIAOZHI) {
                        if (now - last_tap < 250) {
                            PageManager::GetInstance().SwitchTo(PageId::XIAOZHI);
                            last_tap = 0;
                        } else {
                            last_tap = now;
                        }
                    } else {
                        // 在小智页面，正常处理
                        if (now - last_tap < 250) {
                            app.StartListening();
                            last_tap = 0;
                        } else {
                            app.ToggleChatState();
                            last_tap = now;
                        }
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    void InitializeTouch() {
        if (!touch_.Init(codec_i2c_bus_, 0x38)) return;
        xTaskCreatePinnedToCore(TouchTask, "touch_task", 4096, this, 5, nullptr, 0);
    }

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = AUDIO_CODEC_I2C_NUM,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = DISPLAY_MIS0_PIN;
        buscfg.sclk_io_num = DISPLAY_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto &app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
            }
            app.ToggleChatState();
        });
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, &panel_io));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
        ESP_LOGI(TAG, "Install LCD driver ILI9341");
        esp_lcd_panel_reset(panel);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new SpiLcdDisplay(panel_io, panel,
            DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeCustomPages() {
        // 等 LVGL 初始化完成后获取小智主屏幕
        // 使用延迟任务确保 LVGL 已就绪
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                auto* self = static_cast<FreenoveESP32S3Display*>(arg);
                self->SetupCustomPages();
            },
            .arg = this,
            .name = "page_init"
        };
        esp_timer_handle_t timer;
        esp_timer_create(&timer_args, &timer);
        esp_timer_start_once(timer, 3000000); // 3秒后初始化（等LVGL就绪）
    }

    void SetupCustomPages() {
        // 覆盖层模式：只需确认 LVGL 已就绪
        if (!lv_screen_active()) {
            ESP_LOGW(TAG, "No active screen, retry in 2s");
            esp_timer_create_args_t retry = {
                .callback = [](void* arg) {
                    auto* self = static_cast<FreenoveESP32S3Display*>(arg);
                    self->SetupCustomPages();
                },
                .arg = this,
                .name = "page_retry"
            };
            esp_timer_handle_t t;
            esp_timer_create(&retry, &t);
            esp_timer_start_once(t, 2000000);
            return;
        }

        // 初始化页面管理器（覆盖层模式，无需 screen 引用）
        PageManager::GetInstance().Init();

        // 预创建覆盖层 UI（创建时隐藏）
        FlipClock::GetInstance().Create();
        MusicPlayer::GetInstance().Create();

        // 设置空闲 30 秒后自动跳转到翻页时钟
        PageManager::GetInstance().SetIdleTimeout(30);

        ESP_LOGI(TAG, "Custom pages initialized: flip_clock, music_player (overlay mode)");
    }

    void InitializeTools() {
        auto& mcp = McpServer::GetInstance();

        // MCP 工具：切换页面
        mcp.AddTool("self.screen.switch_page",
            "切换设备显示页面。可选页面: xiaozhi(小智AI对话), flip_clock(翻页时钟), music_player(音乐播放器)",
            PropertyList({Property("page", kPropertyTypeString, "xiaozhi")}),
            [](const PropertyList& props) -> ReturnValue {
                std::string page = props["page"].value<std::string>();
                if (page == "xiaozhi" || page == "小智") {
                    PageManager::GetInstance().SwitchTo(PageId::XIAOZHI);
                    return std::string("已切换到小智页面");
                } else if (page == "flip_clock" || page == "clock" || page == "时钟") {
                    PageManager::GetInstance().SwitchTo(PageId::FLIP_CLOCK);
                    return std::string("已切换到翻页时钟");
                } else if (page == "music_player" || page == "music" || page == "音乐") {
                    PageManager::GetInstance().SwitchTo(PageId::MUSIC_PLAYER);
                    return std::string("已切换到音乐播放器");
                }
                return std::string("未知页面: ") + page + "。可选: xiaozhi, flip_clock, music_player";
            });

        // MCP 工具：音乐播放控制
        mcp.AddTool("self.music.control",
            "控制音乐播放。操作: play(播放), pause(暂停), next(下一首), prev(上一首), volume(设置音量0-100)",
            PropertyList({
                Property("action", kPropertyTypeString, "play"),
                Property("value", kPropertyTypeInteger, 0, 0, 100)
            }),
            [](const PropertyList& props) -> ReturnValue {
                std::string action = props["action"].value<std::string>();
                auto& player = MusicPlayer::GetInstance();
                if (action == "play") {
                    player.Play();
                    // 自动切到音乐页面
                    PageManager::GetInstance().SwitchTo(PageId::MUSIC_PLAYER);
                    return std::string("正在播放: ") + player.GetCurrentSong().title;
                } else if (action == "pause") {
                    player.Pause();
                    return std::string("已暂停");
                } else if (action == "next") {
                    player.NextSong();
                    return std::string("下一首: ") + player.GetCurrentSong().title;
                } else if (action == "prev") {
                    player.PrevSong();
                    return std::string("上一首: ") + player.GetCurrentSong().title;
                } else if (action == "volume") {
                    int vol = props["value"].value<int>();
                    player.SetVolume(vol);
                    return std::string("音量设为: ") + std::to_string(vol);
                }
                return std::string("未知操作: ") + action;
            });

        // MCP 工具：获取当前页面信息
        mcp.AddTool("self.screen.get_current_page",
            "获取当前显示的页面名称",
            PropertyList(),
            [](const PropertyList& props) -> ReturnValue {
                auto page = PageManager::GetInstance().GetCurrentPage();
                return std::string("当前页面: ") + PageManager::GetInstance().GetPageName(page);
            });

        ESP_LOGI(TAG, "MCP tools registered: switch_page, music.control, get_current_page");
    }

    // 注册唤醒词回调：唤醒时切回小智页面
    void RegisterWakeWordCallback() {
        auto& app = Application::GetInstance();
        app.RegisterMcpBroadcastCallback([](const std::string& payload) {
            // 当收到 MCP 广播消息时（包括唤醒），切回小智页面
            // 这里简单处理：如果有 "wake" 相关内容就切回
            if (payload.find("wake") != std::string::npos ||
                payload.find("listen") != std::string::npos) {
                PageManager::GetInstance().OnWakeWordTriggered();
            }
        });
    }

public:
    FreenoveESP32S3Display(): boot_button_(BOOT_BUTTON_GPIO)
    {
        InitializeI2c();
        InitializeBatteryMonitor();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeTouch();
        InitializeButtons();
        InitializeCustomPages();
        InitializeTools();
        RegisterWakeWordCallback();
        GetBacklight()->SetBrightness(100);
    }

    virtual Led *GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, AUDIO_CODEC_I2C_NUM,
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR, true, true);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override { return display_; }

    virtual Backlight *GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        charging = adc_battery_monitor_->IsCharging();
        discharging = adc_battery_monitor_->IsDischarging();
        level = adc_battery_monitor_->GetBatteryLevel();
        return true;
    }
};

DECLARE_BOARD(FreenoveESP32S3Display);
