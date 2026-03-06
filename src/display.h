#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <zephyr/device.h>

#define _LCD_NODE        DT_NODELABEL(lcd)
#define _LCD_COLUMNS     DT_PROP(_LCD_NODE, columns)
#define _LCD_ROWS        DT_PROP(_LCD_NODE, rows)

typedef enum display_control{
    DISP_ON,
    DISP_OFF
}display_control_t;

typedef struct display_buffer{
    uint8_t data[_LCD_COLUMNS * _LCD_ROWS];
}display_buffer_t;

void display_init(void);
void display_main(void);

void display_control(display_control_t onoff);

void display_set_text(const display_buffer_t *text);
void display_set_brightness(uint8_t brightness);

#endif /* __DISPLAY_H__ */