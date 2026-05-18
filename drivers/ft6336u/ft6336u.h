#ifndef __FT6336U_H
#define __FT6336U_H

#include "lwip/sys.h"

typedef struct {
    u8 state;
    u16 x;
    u16 y;
} ft6336u_touch_data_t;

typedef void (*ft6336u_touch_data_callback)(ft6336u_touch_data_t data);

void ft6336u_init(void);
void ft6336u_register_touch_data_callback(ft6336u_touch_data_callback cb);

#endif