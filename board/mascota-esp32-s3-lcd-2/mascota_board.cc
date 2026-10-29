// Mascota IA - placa Waveshare ESP32-S3-LCD-2 para el firmware xiaozhi-esp32.
// Pantalla = cara de la mascota, INMP441 = oído, MAX98357A = voz,
// QMI8658 = se "marea" si la agitas, botón externo = mantén pulsado para hablar.
// Sin batería: siempre conectada por USB-C, no se muestra icono de batería.

#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "system_reset.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdlib>

#define TAG "MascotaLCD2"

// ---------------------------------------------------------------------------
// Sensor de movimiento QMI8658 (solo acelerómetro, para detectar sacudidas)
// ---------------------------------------------------------------------------
namespace Motion {
struct AccelSample {
    int x = 0;
    int y = 0;
    int z = 0;
};  // en mili-g

static i2c_master_dev_handle_t device = nullptr;

static esp_err_t Read(uint8_t reg, uint8_t* data, size_t length) {
    return i2c_master_transmit_receive(device, &reg, 1, data, length, 50);
}

static esp_err_t Write(uint8_t reg, uint8_t value) {
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(device, data, sizeof(data), 50);
}

static int16_t Decode(const uint8_t* data) {
    return static_cast<int16_t>(static_cast<uint16_t>(data[0]) |
                                (static_cast<uint16_t>(data[1]) << 8));
}

static esp_err_t Initialize(i2c_master_bus_handle_t bus) {
    if (device != nullptr) {
        return ESP_OK;
    }
    esp_err_t added = ESP_ERR_NOT_FOUND;
    for (uint8_t address : {uint8_t{0x6B}, uint8_t{0x6A}}) {
        if (i2c_master_probe(bus, address, 50) != ESP_OK) {
            continue;
        }
        i2c_device_config_t config = {};
        config.device_address = address;
        config.scl_speed_hz = 400000;
        added = i2c_master_bus_add_device(bus, &config, &device);
        break;
    }
    if (added != ESP_OK) {
        return added;
    }
    uint8_t id = 0;
    esp_err_t ret = Read(0x00, &id, 1);
    if (ret == ESP_OK && id != 0x05) {
        ret = ESP_ERR_NOT_FOUND;
    }
    // Auto-incremento, +/-8 g a 250 Hz, acelerómetro activado.
    if (ret == ESP_OK) ret = Write(0x02, 0x60);
    if (ret == ESP_OK) ret = Write(0x03, 0x25);
    if (ret == ESP_OK) ret = Write(0x08, 0x01);
    if (ret != ESP_OK) {
        i2c_master_bus_rm_device(device);
        device = nullptr;
    }
    return ret;
}

static bool ReadAccel(AccelSample& sample) {
    if (device == nullptr) {
        return false;
    }
    uint8_t data[6] = {};
    if (Read(0x35, data, sizeof(data)) != ESP_OK) {
        return false;
    }
    sample.x = static_cast<int>(Decode(data)) * 1000 / 4096;
    sample.y = static_cast<int>(Decode(data + 2)) * 1000 / 4096;
    sample.z = static_cast<int>(Decode(data + 4)) * 1000 / 4096;
    return true;
}
}  // namespace Motion

// ---------------------------------------------------------------------------
// Placa
// ---------------------------------------------------------------------------
class MascotaLcd2Board : public WifiBoard {
private:
    Button boot_button_;
    Button talk_button_;
    LcdDisplay* display_ = nullptr;
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    esp_timer_handle_t emotion_reset_timer_ = nullptr;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {};
        i2c_bus_cfg.i2c_port = I2C_NUM_0;
        i2c_bus_cfg.sda_io_num = IMU_I2C_SDA_PIN;
        i2c_bus_cfg.scl_io_num = IMU_I2C_SCL_PIN;
        i2c_bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
        i2c_bus_cfg.glitch_ignore_cnt = 7;
        i2c_bus_cfg.flags.enable_internal_pullup = 1;
        if (i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_) != ESP_OK) {
            ESP_LOGW(TAG, "No se pudo crear el bus I2C; sin sensor de movimiento");
            i2c_bus_ = nullptr;
        }
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);

        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                     DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        // Botón BOOT de la placa: pulsación = empezar/terminar conversación.
        // Si se pulsa durante el arranque entra en modo configuración WiFi.
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        // Botón externo: mantén pulsado para hablar, suelta para que responda.
        talk_button_.OnPressDown([]() {
            Application::GetInstance().StartListening();
        });
        talk_button_.OnPressUp([]() {
            Application::GetInstance().StopListening();
        });
    }

    // --- Reacción al movimiento -------------------------------------------
    static void EmotionResetCallback(void* arg) {
        auto* self = static_cast<MascotaLcd2Board*>(arg);
        Application::GetInstance().Schedule([self]() {
            if (self->display_ != nullptr) {
                self->display_->SetEmotion("neutral");
            }
        });
    }

    void ShowTemporaryEmotion(const char* emotion, uint32_t duration_ms) {
        if (display_ == nullptr) {
            return;
        }
        display_->SetEmotion(emotion);
        if (emotion_reset_timer_ != nullptr) {
            esp_timer_stop(emotion_reset_timer_);
            esp_timer_start_once(emotion_reset_timer_,
                                 static_cast<uint64_t>(duration_ms) * 1000ULL);
        }
    }

    static void MotionTask(void* arg) {
        auto* self = static_cast<MascotaLcd2Board*>(arg);
        Motion::AccelSample prev;
        Motion::AccelSample cur;
        bool has_prev = false;
        int64_t last_shake_ms = 0;
        constexpr int kShakeDeltaThreshold = 2000;   // sube este número si reacciona demasiado
        constexpr int64_t kShakeCooldownMs = 2500;

        while (true) {
            if (Motion::ReadAccel(cur)) {
                if (has_prev) {
                    int score = abs(cur.x - prev.x) + abs(cur.y - prev.y) + abs(cur.z - prev.z);
                    int64_t now_ms = esp_timer_get_time() / 1000;
                    if (score > kShakeDeltaThreshold && (now_ms - last_shake_ms) > kShakeCooldownMs) {
                        last_shake_ms = now_ms;
                        auto& app = Application::GetInstance();
                        // Solo se "marea" si no está hablando ni escuchando.
                        if (app.GetDeviceState() == kDeviceStateIdle) {
                            app.Schedule([self]() { self->ShowTemporaryEmotion("confused", 2000); });
                        }
                    }
                }
                prev = cur;
                has_prev = true;
            }
            vTaskDelay(pdMS_TO_TICKS(80));
        }
    }

    void InitializeMotion() {
        if (i2c_bus_ == nullptr) {
            return;
        }
        esp_err_t ret = Motion::Initialize(i2c_bus_);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Sensor QMI8658 no encontrado: %s", esp_err_to_name(ret));
            return;
        }
        esp_timer_create_args_t timer_args = {};
        timer_args.callback = &MascotaLcd2Board::EmotionResetCallback;
        timer_args.arg = this;
        timer_args.dispatch_method = ESP_TIMER_TASK;
        timer_args.name = "emotion_reset";
        esp_timer_create(&timer_args, &emotion_reset_timer_);
        xTaskCreatePinnedToCore(MotionTask, "motion", 4096, this, 2, nullptr, 1);
    }

public:
    MascotaLcd2Board()
        : boot_button_(BOOT_BUTTON_GPIO),
          talk_button_(TALK_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeMotion();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(MascotaLcd2Board);
