#ifndef _ST7701S_RGB_H
#define _ST7701S_RGB_H

#include "ameba_soc.h"

typedef enum {
	RGB565,
} st7701s_rgb_image_format;

typedef struct {
	void (*VBlank)(void *user_data);
} ST7701S_RGBVBlankCallback;

void st7701s_rgb_init(int image_format);
void st7701s_rgb_get_info(int *width, int *height);
void st7701s_rgb_clean_invalidate_buffer(u8 *buffer);
void st7701s_rgb_register_callback(ST7701S_RGBVBlankCallback *callback, void *data);

#endif
