/*
 * Copyright (c) 2026 Realtek Semiconductor Corp.
 * All rights reserved.
 *
 * Licensed under the Realtek License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License from Realtek
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AMEBA_UI_DISPLAY_PANEL_MANAGER_H
#define AMEBA_UI_DISPLAY_PANEL_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    PANEL_IF_RGB,
    PANEL_IF_MIPI_DSI,
    PANEL_IF_SPI,
    PANEL_IF_I2C
} panel_interface_t;

typedef enum {
    PANEL_RGB_FORMAT_RGB565,
    PANEL_RGB_FORMAT_RGB666,
    PANEL_RGB_FORMAT_RGB888,
    PANEL_RGB_FORMAT_RGB101010
} panel_rgb_format_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t hsync_front_porch;
    uint32_t hsync_back_porch;
    uint32_t hsync_pulse_width;
    uint32_t vsync_front_porch;
    uint32_t vsync_back_porch;
    uint32_t vsync_pulse_width;
    uint32_t clock_frequency;
    bool hsync_active_low;
    bool vsync_active_low;
    bool de_active_high;
    bool dclk_falling_edge;
    bool use_de;
} panel_timing_t;

typedef struct {
    bool status;
    uint32_t reset_pin;
    uint32_t cs_pin;
    uint32_t dc_pin;
    uint32_t bl_pin;
    uint32_t power_en_pin;
} panel_gpio_config_t;

typedef struct {
    bool status;
    uint32_t spi_index;
    uint32_t cs_pin;
    uint32_t mosi_pin;
    uint32_t miso_pin;
    uint32_t sclk_pin;
} panel_spi_config_t;

typedef struct {
    bool status;
    uint32_t data_pins[24];
} panel_rgb_data_config_t;

typedef struct {
    bool status;
    uint32_t hsync_pin;
    uint32_t vsync_pin;
    uint32_t dclk_pin;
    uint32_t de_pin;
} panel_rgb_ctrl_config_t;

typedef struct panel_operations {
    // init function.
    bool (*init)(void *panel_data);
    bool (*exit)(void *panel_data);

    // power management.
    bool (*power_on)(void *panel_data);
    bool (*power_off)(void *panel_data);
    bool (*reset)(void *panel_data);

    // backlight control.
    bool (*enable_backlight)(void *panel_data, uint8_t brightness);
    bool (*disable_backlight)(void *panel_data);

    // display control.
    bool (*enable_display)(void *panel_data);
    bool (*disable_display)(void *panel_data);

    // mode control.
    bool (*set_sleep_mode)(void *panel_data, bool sleep);
    bool (*set_display_mode)(void *panel_data, uint32_t mode);
} panel_ops_t;

typedef struct panel_descriptor {
    const char *name;
    const char *manufacturer;
    const char *model;

    panel_interface_t interface;
    panel_rgb_format_t rgb_format;

    panel_timing_t timing;
    panel_gpio_config_t *gpio_config;

    panel_spi_config_t *spi_config;

    uint32_t init_cmd_count;
    const uint8_t (*init_cmds)[3];  // initial cmds [cmd, param1, param2]

    panel_ops_t *ops;

    void *private_data;
} panel_desc_t;

typedef struct panel_device {
    panel_desc_t *desc;
    bool initialized;
    bool powered_on;
    bool backlight_on;
    uint8_t brightness;
    void (*frame_callback)(void *user_data);
    void *user_data;
} panel_dev_t;

bool panel_register(panel_desc_t *desc);
bool panel_unregister(const char *name);
panel_desc_t *panel_find_by_name(const char *name);
panel_desc_t *panel_find_by_resolution(uint32_t width, uint32_t height);

// Panel device management.
panel_dev_t *panel_device_create(panel_desc_t *desc);
bool panel_device_destroy(panel_dev_t *dev);
bool panel_device_init(panel_dev_t *dev);
bool panel_device_exit(panel_dev_t *dev);

// Panel control.
bool panel_power_on(panel_dev_t *dev);
bool panel_power_off(panel_dev_t *dev);
bool panel_reset(panel_dev_t *dev);
bool panel_set_backlight(panel_dev_t *dev, uint8_t brightness);
bool panel_enable_display(panel_dev_t *dev);
bool panel_disable_display(panel_dev_t *dev);

void panel_list_all(void);

#endif // AMEBA_UI_DISPLAY_PANEL_MANAGER_H