#pragma once

#include "esp_lcd_panel_dev.h"
#include "esp_lcd_panel_io.h"

// ILI9486 behind the 16-bit shift registers used by RPi 3.5" boards
// (Waveshare 3.5" A / LCDwiki MPI3501): every command and parameter is
// clocked as a 16-bit word, pixels as plain RGB565.
// The IO handle must be created with lcd_cmd_bits = 16, lcd_param_bits = 16.
esp_err_t esp_lcd_new_panel_ili9486(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *config,
                                    esp_lcd_panel_handle_t *ret_panel);
