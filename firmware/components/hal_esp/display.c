#include "display.h"

#include "board_pins.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_ili9486.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_xpt2046.h"
#include "esp_lvgl_port.h"

static const char *TAG = "display";

#define LCD_SPI_HOST    SPI2_HOST
#define DRAW_BUF_LINES  40

static esp_err_t init_bus(void)
{
    const spi_bus_config_t bus = {
        .sclk_io_num = PIN_LCD_SCLK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = PIN_LCD_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * DRAW_BUF_LINES * sizeof(uint16_t),
    };
    return spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO);
}

static esp_err_t init_panel(esp_lcd_panel_io_handle_t *out_io, esp_lcd_panel_handle_t *out_panel)
{
    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = PIN_LCD_CS,
        .dc_gpio_num = PIN_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = LCD_SPI_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 16,
        .lcd_param_bits = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, out_io),
                        TAG, "panel io failed");

    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ili9486(*out_io, &panel_cfg, out_panel), TAG, "panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(*out_panel), TAG, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*out_panel), TAG, "panel init failed");
    return esp_lcd_panel_disp_on_off(*out_panel, true);
}

static esp_err_t init_touch(esp_lcd_touch_handle_t *out_touch)
{
    esp_lcd_panel_io_handle_t tp_io;
    esp_lcd_panel_io_spi_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(PIN_TOUCH_CS);
    tp_io_cfg.pclk_hz = TOUCH_SPI_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &tp_io_cfg, &tp_io),
                        TAG, "touch io failed");

    // Touch axes follow the panel's native portrait orientation; rotated to landscape here.
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_V_RES,
        .y_max = LCD_H_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = PIN_TOUCH_IRQ,
        .flags = {
            .swap_xy = 1,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    return esp_lcd_touch_new_spi_xpt2046(tp_io, &tp_cfg, out_touch);
}

esp_err_t display_init(lv_display_t **out_disp)
{
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    esp_lcd_touch_handle_t touch;

    ESP_RETURN_ON_ERROR(init_bus(), TAG, "spi bus failed");
    ESP_RETURN_ON_ERROR(init_panel(&io, &panel), TAG, "panel init failed");
    ESP_RETURN_ON_ERROR(init_touch(&touch), TAG, "touch init failed");

    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "lvgl port failed");

    // Panel is natively 320x480 portrait; swap_xy turns it into 480x320 landscape.
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = LCD_H_RES * DRAW_BUF_LINES,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = true,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    ESP_RETURN_ON_FALSE(disp, ESP_FAIL, TAG, "lvgl display failed");

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = touch,
    };
    ESP_RETURN_ON_FALSE(lvgl_port_add_touch(&touch_cfg), ESP_FAIL, TAG, "lvgl touch failed");

    *out_disp = disp;
    return ESP_OK;
}
