#ifndef _ST7262_H
#define _ST7262_H

#include "ameba_soc.h"

typedef enum {
	RGB565,
	RGB888,
	ARGB8888,
} st7262_image_format;

typedef struct {
	void (*VBlank)(void *user_data);
} ST7262VBlankCallback;
void st7262_init(int image_format);
void st7262_get_info(int *width, int *height);
void st7262_clean_invalidate_buffer(u8 *buffer);
void st7262_register_callback(ST7262VBlankCallback *callback, void *data);

#endif
