#include "esp_lcd_ili9486.h"

#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ili9486";

#define MAX_PARAMS 15

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t madctl;
} ili9486_panel_t;

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t data[MAX_PARAMS];
    uint16_t delay_ms;
} init_cmd_t;

// Init sequence from the Linux fbtft "waveshare35a" overlay.
static const init_cmd_t init_cmds[] = {
    {0xB0, 1, {0x00}, 0},                     // interface mode control
    {LCD_CMD_SLPOUT, 0, {0}, 250},
    {LCD_CMD_COLMOD, 1, {0x55}, 0},           // RGB565
    {0xC2, 1, {0x44}, 0},                     // power control 3
    {0xC5, 4, {0x00, 0x00, 0x00, 0x00}, 0},   // VCOM
    {0xE0, 15, {0x0F, 0x1F, 0x1C, 0x0C, 0x0F, 0x08, 0x48, 0x98,
                0x37, 0x0A, 0x13, 0x04, 0x11, 0x0D, 0x00}, 0},
    {0xE1, 15, {0x0F, 0x32, 0x2E, 0x0B, 0x0D, 0x05, 0x47, 0x75,
                0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00}, 0},
    {0xE2, 15, {0x0F, 0x32, 0x2E, 0x0B, 0x0D, 0x05, 0x47, 0x75,
                0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00}, 0},
    {LCD_CMD_SLPOUT, 0, {0}, 120},
};

// Commands and parameters go out as 16-bit words; esp_lcd byte-swaps the
// parameter buffer in place, so always pass a writable copy.
static esp_err_t tx_param(ili9486_panel_t *p, uint8_t cmd, const uint8_t *data, size_t len)
{
    uint16_t words[MAX_PARAMS];
    assert(len <= MAX_PARAMS);
    for (size_t i = 0; i < len; i++) {
        words[i] = data[i];
    }
    return esp_lcd_panel_io_tx_param(p->io, cmd, len ? words : NULL, len * sizeof(uint16_t));
}

static esp_err_t panel_del(esp_lcd_panel_t *panel)
{
    ili9486_panel_t *p = __containerof(panel, ili9486_panel_t, base);
    if (p->reset_gpio >= 0) {
        gpio_reset_pin(p->reset_gpio);
    }
    free(p);
    return ESP_OK;
}

static esp_err_t panel_reset(esp_lcd_panel_t *panel)
{
    ili9486_panel_t *p = __containerof(panel, ili9486_panel_t, base);
    if (p->reset_gpio >= 0) {
        gpio_set_level(p->reset_gpio, p->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(p->reset_gpio, !p->reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else {
        ESP_RETURN_ON_ERROR(tx_param(p, LCD_CMD_SWRESET, NULL, 0), TAG, "swreset failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    return ESP_OK;
}

static esp_err_t panel_init(esp_lcd_panel_t *panel)
{
    ili9486_panel_t *p = __containerof(panel, ili9486_panel_t, base);
    for (size_t i = 0; i < sizeof(init_cmds) / sizeof(init_cmds[0]); i++) {
        const init_cmd_t *c = &init_cmds[i];
        ESP_RETURN_ON_ERROR(tx_param(p, c->cmd, c->data, c->len), TAG, "init cmd 0x%02x failed", c->cmd);
        if (c->delay_ms) {
            vTaskDelay(pdMS_TO_TICKS(c->delay_ms));
        }
    }
    return tx_param(p, LCD_CMD_MADCTL, &p->madctl, 1);
}

static esp_err_t panel_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                   const void *color_data)
{
    ili9486_panel_t *p = __containerof(panel, ili9486_panel_t, base);
    x_start += p->x_gap;
    x_end += p->x_gap;
    y_start += p->y_gap;
    y_end += p->y_gap;

    const uint8_t caset[] = {x_start >> 8, x_start & 0xFF, (x_end - 1) >> 8, (x_end - 1) & 0xFF};
    const uint8_t raset[] = {y_start >> 8, y_start & 0xFF, (y_end - 1) >> 8, (y_end - 1) & 0xFF};
    ESP_RETURN_ON_ERROR(tx_param(p, LCD_CMD_CASET, caset, sizeof(caset)), TAG, "caset failed");
    ESP_RETURN_ON_ERROR(tx_param(p, LCD_CMD_RASET, raset, sizeof(raset)), TAG, "raset failed");

    size_t len = (size_t)(x_end - x_start) * (y_end - y_start) * sizeof(uint16_t);
    return esp_lcd_panel_io_tx_color(p->io, LCD_CMD_RAMWR, color_data, len);
}

static esp_err_t panel_invert_color(esp_lcd_panel_t *panel, bool invert)
{
    ili9486_panel_t *p = __containerof(panel, ili9486_panel_t, base);
    return tx_param(p, invert ? LCD_CMD_INVON : LCD_CMD_INVOFF, NULL, 0);
}

static esp_err_t panel_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    ili9486_panel_t *p = __containerof(panel, ili9486_panel_t, base);
    p->madctl = mirror_x ? (p->madctl | LCD_CMD_MX_BIT) : (p->madctl & ~LCD_CMD_MX_BIT);
    p->madctl = mirror_y ? (p->madctl | LCD_CMD_MY_BIT) : (p->madctl & ~LCD_CMD_MY_BIT);
    return tx_param(p, LCD_CMD_MADCTL, &p->madctl, 1);
}

static esp_err_t panel_swap_xy(esp_lcd_panel_t *panel, bool swap)
{
    ili9486_panel_t *p = __containerof(panel, ili9486_panel_t, base);
    p->madctl = swap ? (p->madctl | LCD_CMD_MV_BIT) : (p->madctl & ~LCD_CMD_MV_BIT);
    return tx_param(p, LCD_CMD_MADCTL, &p->madctl, 1);
}

static esp_err_t panel_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    ili9486_panel_t *p = __containerof(panel, ili9486_panel_t, base);
    p->x_gap = x_gap;
    p->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_disp_on_off(esp_lcd_panel_t *panel, bool on)
{
    ili9486_panel_t *p = __containerof(panel, ili9486_panel_t, base);
    return tx_param(p, on ? LCD_CMD_DISPON : LCD_CMD_DISPOFF, NULL, 0);
}

esp_err_t esp_lcd_new_panel_ili9486(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(config->bits_per_pixel == 16, ESP_ERR_NOT_SUPPORTED, TAG, "only RGB565 supported");

    ili9486_panel_t *p = calloc(1, sizeof(*p));
    ESP_RETURN_ON_FALSE(p, ESP_ERR_NO_MEM, TAG, "no mem for panel");

    if (config->reset_gpio_num >= 0) {
        gpio_config_t cfg = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << config->reset_gpio_num,
        };
        esp_err_t err = gpio_config(&cfg);
        if (err != ESP_OK) {
            free(p);
            return err;
        }
    }

    p->io = io;
    p->reset_gpio = config->reset_gpio_num;
    p->reset_level = config->flags.reset_active_high;
    p->madctl = (config->rgb_ele_order == LCD_RGB_ELEMENT_ORDER_BGR) ? LCD_CMD_BGR_BIT : 0;

    p->base.del = panel_del;
    p->base.reset = panel_reset;
    p->base.init = panel_init;
    p->base.draw_bitmap = panel_draw_bitmap;
    p->base.invert_color = panel_invert_color;
    p->base.mirror = panel_mirror;
    p->base.swap_xy = panel_swap_xy;
    p->base.set_gap = panel_set_gap;
    p->base.disp_on_off = panel_disp_on_off;

    *ret_panel = &p->base;
    return ESP_OK;
}
