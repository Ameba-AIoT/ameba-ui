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

#include <stdlib.h>
#include <string.h>

#include "panel_manager.h"
#include "os_wrapper.h"
#include "ameba_soc.h"

#define LOG_TAG "PanelMgr"
#define MAX_REGISTERED_PANELS 20

static panel_desc_t *panel_database[MAX_REGISTERED_PANELS];
static uint8_t panel_count = 0;

bool panel_register(panel_desc_t *desc) {
    if (!desc || !desc->name) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Invalid panel descriptor\n");
        return false;
    }

    if (panel_count >= MAX_REGISTERED_PANELS) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Panel database is full\n");
        return false;
    }

    // check if already registered.
    for (int i = 0; i < panel_count; i++) {
        if (strcmp(panel_database[i]->name, desc->name) == 0) {
            RTK_LOGS(LOG_TAG, RTK_LOG_WARN, "Panel %s already registered, updating\n", desc->name);
            panel_database[i] = desc;
            return true;
        }
    }

    // register new panel.
    panel_database[panel_count++] = desc;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Registered panel: %s (%dx%d)\n",
             desc->name, desc->timing.width, desc->timing.height);

    return true;
}

bool panel_unregister(const char *name) {
    if (!name) {
        return false;
    }

    for (uint8_t i = 0; i < panel_count; i++) {
        if (strcmp(panel_database[i]->name, name) == 0) {
            // Move the following elements forward.
            for (uint8_t j = i; j < panel_count - 1; j++) {
                panel_database[j] = panel_database[j + 1];
            }
            panel_count--;

            RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Unregistered panel: %s\n", name);
            return true;
        }
    }

    return false;
}

panel_desc_t *panel_find_by_name(const char *name) {
    if (!name) {
        return NULL;
    }

    for (int i = 0; i < panel_count; i++) {
        if (strcmp(panel_database[i]->name, name) == 0) {
            return panel_database[i];
        }
    }

    RTK_LOGS(LOG_TAG, RTK_LOG_DEBUG, "Panel %s not found\n", name);
    return NULL;
}

panel_desc_t *panel_find_by_resolution(uint32_t width, uint32_t height) {
    for (int i = 0; i < panel_count; i++) {
        if (panel_database[i]->timing.width == width &&
            panel_database[i]->timing.height == height) {
            return panel_database[i];
        }
    }

    // Try to find a similar resolution.
    for (int i = 0; i < panel_count; i++) {
        if (panel_database[i]->timing.width >= width &&
            panel_database[i]->timing.height >= height) {
            RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Found compatible panel: %s\n",
                     panel_database[i]->name);
            return panel_database[i];
        }
    }

    RTK_LOGS(LOG_TAG, RTK_LOG_DEBUG, "No panel found for resolution %dx%d\n", width, height);
    return NULL;
}

panel_dev_t *panel_device_create(panel_desc_t *desc) {
    if (!desc) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Cannot create panel device: invalid descriptor\n");
        return NULL;
    }

    panel_dev_t *dev = (panel_dev_t *)malloc(sizeof(panel_dev_t));
    if (!dev) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Failed to allocate panel device\n");
        return NULL;
    }

    memset(dev, 0, sizeof(panel_dev_t));
    dev->desc = desc;
    dev->initialized = false;
    dev->powered_on = false;
    dev->backlight_on = false;
    dev->brightness = 0;
    dev->frame_callback = NULL;
    dev->user_data = NULL;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Created panel device: %s\n", desc->name);

    return dev;
}

bool panel_device_destroy(panel_dev_t *dev) {
    if (!dev) {
        return false;
    }

    // Exit if device is initialized.
    if (dev->initialized) {
        panel_device_exit(dev);
    }

    free(dev);

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Destroyed panel device\n");
    return true;
}

bool panel_device_init(panel_dev_t *dev) {
    if (!dev || !dev->desc) {
        return false;
    }

    if (dev->initialized) {
        RTK_LOGS(LOG_TAG, RTK_LOG_WARN, "Panel device already initialized\n");
        return true;
    }

    // call panel's init function.
    if (dev->desc->ops && dev->desc->ops->init) {
        if (!dev->desc->ops->init(dev)) {
            RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Panel init failed\n");
            return false;
        }
    }

    dev->initialized = true;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Panel device initialized: %s\n", dev->desc->name);

    return true;
}

bool panel_device_exit(panel_dev_t *dev) {
    if (!dev || !dev->initialized) {
        return false;
    }

    // call panel's exit function.
    if (dev->desc->ops && dev->desc->ops->exit) {
        if (!dev->desc->ops->exit(dev)) {
            RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Panel exit failed\n");
            return false;
        }
    }

    dev->initialized = false;
    dev->powered_on = false;
    dev->backlight_on = false;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Panel device exited: %s\n", dev->desc->name);

    return true;
}

bool panel_power_on(panel_dev_t *dev) {
    if (!dev || !dev->desc) {
        return false;
    }

    if (dev->powered_on) {
        RTK_LOGS(LOG_TAG, RTK_LOG_WARN, "Panel already powered on\n");
        return true;
    }

    bool success = true;

    // call panel's power on function.
    if (dev->desc->ops && dev->desc->ops->power_on) {
        success = dev->desc->ops->power_on(dev);
    }

    if (success) {
        dev->powered_on = true;
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Panel powered on\n");
    } else {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Failed to power on panel\n");
    }

    return success;
}

bool panel_power_off(panel_dev_t *dev) {
    if (!dev || !dev->desc) {
        return false;
    }

    if (!dev->powered_on) {
        RTK_LOGS(LOG_TAG, RTK_LOG_WARN, "Panel already powered off\n");
        return true;
    }

    bool success = true;

    // turn off back light.
    if (dev->backlight_on) {
        panel_set_backlight(dev, 0);
    }

    // call panel's power_off
    if (dev->desc->ops && dev->desc->ops->power_off) {
        success = dev->desc->ops->power_off(dev);
    }

    if (success) {
        dev->powered_on = false;
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Panel powered off\n");
    } else {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Failed to power off panel\n");
    }

    return success;
}

bool panel_reset(panel_dev_t *dev) {
    if (!dev || !dev->desc) {
        return false;
    }

    // call panel's reset function.
    if (dev->desc->ops && dev->desc->ops->reset) {
        return dev->desc->ops->reset(dev);
    }

    // using GPIO implement if there's no reset function in panel.
    if (dev->desc->gpio_config->reset_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(dev->desc->gpio_config->reset_pin, 1);
        rtos_time_delay_ms(1);
        GPIO_WriteBit(dev->desc->gpio_config->reset_pin, 0);
        rtos_time_delay_ms(10);
        GPIO_WriteBit(dev->desc->gpio_config->reset_pin, 1);
        rtos_time_delay_ms(120);
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Panel reset via GPIO\n");
        return true;
    }

    RTK_LOGS(LOG_TAG, RTK_LOG_WARN, "No reset method available for panel\n");
    return false;
}

bool panel_set_backlight(panel_dev_t *dev, uint8_t brightness) {
    if (!dev || !dev->desc) {
        return false;
    }

    if (!dev->powered_on) {
        RTK_LOGS(LOG_TAG, RTK_LOG_WARN, "Cannot set backlight: panel is powered off\n");
        return false;
    }

    bool success = true;

    if (brightness > 0) {
        // turn on back light.
        if (dev->desc->ops && dev->desc->ops->enable_backlight) {
            success = dev->desc->ops->enable_backlight(dev, brightness);
        } else {
            // using GPIO implement.
            if (dev->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
                GPIO_WriteBit(dev->desc->gpio_config->bl_pin, 1);
                dev->backlight_on = true;
                dev->brightness = brightness;
                RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Backlight on via GPIO, brightness: %d\n", brightness);
            } else {
                success = false;
            }
        }
    } else {
        // turn off back light.
        if (dev->desc->ops && dev->desc->ops->disable_backlight) {
            success = dev->desc->ops->disable_backlight(dev);
        } else {
            // using GPIO implement.
            if (dev->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
                GPIO_WriteBit(dev->desc->gpio_config->bl_pin, 0);
                dev->backlight_on = false;
                dev->brightness = 0;
                RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Backlight off via GPIO\n");
            } else {
                success = false;
            }
        }
    }

    return success;
}

bool panel_enable_display(panel_dev_t *dev) {
    if (!dev || !dev->desc) {
        return false;
    }

    if (!dev->powered_on) {
        RTK_LOGS(LOG_TAG, RTK_LOG_WARN, "Cannot enable display: panel is powered off\n");
        return false;
    }

    // call panel's enable_display fucntion.
    if (dev->desc->ops && dev->desc->ops->enable_display) {
        return dev->desc->ops->enable_display(dev);
    }

    if (dev->desc->interface == PANEL_IF_RGB) {
        RTK_LOGS(LOG_TAG, RTK_LOG_DEBUG, "RGB panel doesn't need explicit display enable\n");
        return true;
    }

    RTK_LOGS(LOG_TAG, RTK_LOG_WARN, "No display enable method available\n");
    return false;
}

bool panel_disable_display(panel_dev_t *dev) {
    if (!dev || !dev->desc) {
        return false;
    }

    // call panel's disable_display function.
    if (dev->desc->ops && dev->desc->ops->disable_display) {
        return dev->desc->ops->disable_display(dev);
    }

    RTK_LOGS(LOG_TAG, RTK_LOG_WARN, "No display disable method available\n");
    return false;
}

void panel_dump_info(panel_dev_t *dev) {
    if (!dev || !dev->desc) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Invalid panel device\n");
        return;
    }

    panel_desc_t *desc = dev->desc;

    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "=== Panel Information ===\n");
    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "Name: %s\n", desc->name);
    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "Manufacturer: %s\n", desc->manufacturer);
    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "Model: %s\n", desc->model);
    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "Interface: %d\n", desc->interface);
    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "Resolution: %dx%d\n", desc->timing.width, desc->timing.height);
    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "Clock: %d Hz\n", desc->timing.clock_frequency);
    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "Format: %d\n", desc->rgb_format);
    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "Initialized: %s\n", dev->initialized ? "Yes" : "No");
    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "Powered: %s\n", dev->powered_on ? "Yes" : "No");
    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "Backlight: %s (brightness: %d)\n",
             dev->backlight_on ? "On" : "Off", dev->brightness);
    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "=========================\n");
}

void panel_list_all(void) {
    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "=== Registered Panels (%d) ===\n", panel_count);

    for (int i = 0; i < panel_count; i++) {
        panel_desc_t *desc = panel_database[i];
        RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "%d. %s: %dx%d, %s, if=%d\n",
                 i + 1, desc->name, desc->timing.width, desc->timing.height,
                 desc->manufacturer, desc->interface);
    }

    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "==============================\n");
}

static bool panel_default_init(void *panel_data) {
    panel_dev_t *dev = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_DEBUG, "Default init for panel %s\n", dev->desc->name);

    panel_gpio_config_t *gpio = dev->desc->gpio_config;

    if (gpio->reset_pin != 0xFFFFFFFF) {
        // default high.
        GPIO_InitTypeDef gpio_init;
        gpio_init.GPIO_Pin = gpio->reset_pin;
        gpio_init.GPIO_Mode = GPIO_Mode_OUT;
        GPIO_Init(&gpio_init);
        GPIO_WriteBit(gpio->reset_pin, 1);
    }

    if (gpio->bl_pin != 0xFFFFFFFF) {
        // default turn off.
        GPIO_InitTypeDef gpio_init;
        gpio_init.GPIO_Pin = gpio->bl_pin;
        gpio_init.GPIO_Mode = GPIO_Mode_OUT;
        GPIO_Init(&gpio_init);
        GPIO_WriteBit(gpio->bl_pin, 0);
    }

    if (gpio->power_en_pin != 0xFFFFFFFF) {
        // default disable.
        GPIO_InitTypeDef gpio_init;
        gpio_init.GPIO_Pin = gpio->power_en_pin;
        gpio_init.GPIO_Mode = GPIO_Mode_OUT;
        GPIO_Init(&gpio_init);
        GPIO_WriteBit(gpio->power_en_pin, 0);
    }

    return true;
}

static panel_ops_t default_panel_ops = {
    .init = panel_default_init,
    .exit = NULL,
    .power_on = NULL,
    .power_off = NULL,
    .reset = NULL,
    .enable_backlight = NULL,
    .disable_backlight = NULL,
    .enable_display = NULL,
    .disable_display = NULL,
    .set_sleep_mode = NULL,
    .set_display_mode = NULL
};

// If panel provides no ops, using default.
panel_ops_t *panel_get_default_ops(void) {
    return &default_panel_ops;
}