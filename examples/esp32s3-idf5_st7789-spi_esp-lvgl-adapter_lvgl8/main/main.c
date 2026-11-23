#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_st7789v.h"

#include "driver/spi_master.h"

#include "esp_lv_adapter.h"
#include "lvgl.h"
#include "lv_demos.h"

static const char *TAG = "LVGL_ST7789";

// ==================== 你提供的引脚 ====================
#define EXAMPLE_PIN_NUM_LCD_CS      GPIO_NUM_12
#define EXAMPLE_PIN_NUM_LCD_DC      GPIO_NUM_13
#define EXAMPLE_PIN_NUM_LCD_SCK     GPIO_NUM_11   // SCLK
#define EXAMPLE_PIN_NUM_LCD_MOSI    GPIO_NUM_10
#define EXAMPLE_PIN_NUM_LCD_RST     GPIO_NUM_9
#define EXAMPLE_PIN_NUM_BK_LIGHT    GPIO_NUM_15

// ==================== 显示参数（请根据你的屏幕修改） ====================
#define EXAMPLE_LCD_HOST            SPI2_HOST
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ  (40 * 1000 * 1000)

#define EXAMPLE_LCD_H_RES           240
#define EXAMPLE_LCD_V_RES           320

#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL 0

static const st7789v_lcd_init_cmd_t vendor_specific_init_default[] = {
//  {cmd, { data }, data_size, delay_ms}
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0xB2, (uint8_t []){0x0C,0x0C,0x00,0x33,0x33}, 5, 0},
    // {0x35, (uint8_t []){0x00}, 1, 0},
    // {0x44, (uint8_t []){0x00,0x00}, 2, 0},
    {0x36, (uint8_t []){0x00}, 1, 0},
    {0x3A, (uint8_t []){0x05}, 1, 0},
    {0xB7, (uint8_t []){0x35}, 1, 0},
    {0xBB, (uint8_t []){0x34}, 1, 0},
    {0xC0, (uint8_t []){0x2C}, 1, 0},
    {0xC2, (uint8_t []){0x01}, 1, 0},
    {0xC3, (uint8_t []){0x13}, 1, 0},
    {0xC4, (uint8_t []){0x20}, 1, 0},
    {0xC6, (uint8_t []){0x0F}, 1, 0},
    {0xD0, (uint8_t []){0xA4,0xA1}, 2, 0},
    {0xD6, (uint8_t []){0xA1}, 1, 0},
    {0xE0, (uint8_t []){0xD0,0x0A,0x10,0x0C,0x0C,0x18,0x35,0x43,0x4D,0x39,0x13,0x13,0x2D,0x34}, 14, 0},
    {0xE1, (uint8_t []){0xD0,0x05,0x0B,0x06,0x05,0x02,0x35,0x43,0x4D,0x16,0x15,0x15,0x2E,0x32}, 14, 0},
    {0x21, (uint8_t []){0x00}, 0, 0},
    {0x29, (uint8_t []){0x00}, 0, 0},
};

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 + ST7789 + LVGL 开始初始化...");

    // 1. 背光引脚初始化
    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bk_gpio_config);
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);

    // 2. SPI 总线初始化
    spi_bus_config_t bus_config = {
        .sclk_io_num = EXAMPLE_PIN_NUM_LCD_SCK,
        .mosi_io_num = EXAMPLE_PIN_NUM_LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    // 3. LCD Panel IO（4线SPI）
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS,
        .dc_gpio_num = EXAMPLE_PIN_NUM_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_lcd_panel_io_handle_t panel_io = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_HOST,
                                             &io_config, &panel_io));

    // 4. ST7789 Panel 初始化
    const st7789v_vendor_config_t vendor_config = {
        .init_cmds = vendor_specific_init_default,
        .init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(vendor_specific_init_default[0]),
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_endian = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = (void *) &vendor_config,
    };
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "ST7789 显示初始化完成 (分辨率 %dx%d)", EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);

    // 5. esp_lvgl_adapter 初始化
    esp_lv_adapter_config_t adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_cfg));

    // 注册显示器
    esp_lv_adapter_display_config_t disp_cfg =
        ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(
            panel_handle,
            panel_io,
            EXAMPLE_LCD_H_RES,
            EXAMPLE_LCD_V_RES,
            ESP_LV_ADAPTER_ROTATE_0);

    lv_display_t *disp = esp_lv_adapter_register_display(&disp_cfg);
    assert(disp != NULL);

    ESP_ERROR_CHECK(esp_lv_adapter_start());
    ESP_LOGI(TAG, "esp_lvgl_adapter 启动成功！");

    // 6. 创建 LVGL UI（必须加锁）
    if (esp_lv_adapter_lock(portMAX_DELAY) == ESP_OK) {

        lv_demo_widgets();

        esp_lv_adapter_unlock();
    }

    ESP_LOGI(TAG, "LVGL UI 创建完成，系统正常运行...");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}