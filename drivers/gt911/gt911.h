#ifndef __CST328_H
#define __CST328_H

#include "lwip/sys.h"

typedef enum {
	TOUCH_PRESS = 1,
	TOUCH_RELEASE = 0,
} gt911_touch_state;

typedef struct {
	u8 state;
	u16 x;
	u16 y;
} gt911_touch_data_t;

typedef void (*gt911_touch_data_callback)(gt911_touch_data_t data);

void gt911_init(void);
void gt911_register_touch_data_callback(gt911_touch_data_callback cb);

#endif
