#include "i2c_api.h"
#include "i2c_ex_api.h"

#include "gpio_api.h"
#include "gpio_irq_api.h"

#include "ft6336u.h"

#define SCREEN_WIDTH        800
#define SCREEN_HEIGHT       480

#define RST_PIN             _PA_21
#define INT_PIN             _PB_14
#define SDA_PIN             _PA_20
#define SCL_PIN             _PB_13

#define XSIZE               720
#define YSIZE               720

#define I2C_ADDR            0X38
#define I2C_BUS_CLK         400000

static i2c_t g_obj;
static ft6336u_touch_data_t g_ft6336u_touch_data;
static ft6336u_touch_data_callback g_cb = NULL;

typedef enum {
    TOUCH_PRESS = 1,
    TOUCH_RELEASE = 0,
} ft6336u_touch_state;

bool ack = true;

static int FT6336U_read_reg(u8 reg, u8 *buf, u8 len)
{
    int ret = 0;
    ret = i2c_write(&g_obj, I2C_ADDR, (char *)&reg, 1, 1);

    if (ret != 1) {
        RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s: Slave no ACK before read:%d. \r\n", __func__, ret);
        ack = false;
        return -1;
    }

    return i2c_read(&g_obj, I2C_ADDR, (char *)buf, len, 1);
}

static void FT6336U_touch_report(void)
{
    g_ft6336u_touch_data.state = TOUCH_RELEASE;
    uint8_t buf[10] = {0};
    g_ft6336u_touch_data.x = XSIZE;
    g_ft6336u_touch_data.y = YSIZE;

    static uint16_t x_old = 0;
    static uint16_t y_old = 0;

    if (FT6336U_read_reg(0x03, buf, 6)) {
        g_ft6336u_touch_data.state = TOUCH_PRESS;
        g_ft6336u_touch_data.x = ((buf[0] & 0x0F) << 8) | buf[0 + 1];
        g_ft6336u_touch_data.y = ((buf[0 + 2] & 0x0F) << 8) | buf[0 + 3];

        x_old = g_ft6336u_touch_data.x;
        y_old = g_ft6336u_touch_data.y;
    } else {
        g_ft6336u_touch_data.x = x_old;
        g_ft6336u_touch_data.y = y_old;
    }
    RTK_LOGS(NOTAG, RTK_LOG_INFO, "%s: (x, y) = (%d, %d) \r\n", __func__, x_old, y_old);
}

static void gpio_touch_irq_handler(uint32_t id, uint32_t event)
{
    UNUSED(id);
    UNUSED(event);

    if (ack) {
        FT6336U_touch_report();
        if (g_cb) {
            g_cb(g_ft6336u_touch_data);
        }
    }
}

static void board_i2c_init(void)
{
    GPIO_WriteBit(INT_PIN, 0);
    GPIO_WriteBit(RST_PIN, 0);
    DelayMs(10);
    GPIO_WriteBit(INT_PIN, 1);
    DelayUs(100);
    GPIO_WriteBit(RST_PIN, 1);
    DelayMs(5);
    GPIO_WriteBit(INT_PIN, 0);
    DelayMs(50);
}

void ft6336u_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /*init RST pin*/
    GPIO_InitStructure.GPIO_Pin = RST_PIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(&GPIO_InitStructure);

    /*init INT pin*/
    GPIO_InitStructure.GPIO_Pin = INT_PIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(&GPIO_InitStructure);

    board_i2c_init();

    /*init INT pin*/
    GPIO_InitStructure.GPIO_Pin = INT_PIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_Init(&GPIO_InitStructure);

    /*init I2C*/
    g_obj.i2c_idx = 0;
    i2c_init(&g_obj, SDA_PIN, SCL_PIN);
    i2c_frequency(&g_obj, I2C_BUS_CLK);
    i2c_restart_disable(&g_obj);

    gpio_irq_t gpio_btn;
    gpio_irq_init(&gpio_btn, INT_PIN, gpio_touch_irq_handler, (uint32_t)(&gpio_btn));
    gpio_irq_set(&gpio_btn, IRQ_RISE, 1); // IRQ_FALL
    gpio_irq_enable(&gpio_btn);
}

void ft6336u_register_touch_data_callback(ft6336u_touch_data_callback cb)
{
    g_cb = cb;
}