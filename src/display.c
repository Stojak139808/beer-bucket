#include <zephyr/drivers/auxdisplay.h>
#include <string.h>
#include "display.h"

/*
sensor value aux:           5 characters    -> 123.45 | -12.34 | ERROR
sensor value main:          5 characters    -> 123.45 | -12.34 | ERROR
target temp:                5 characters    -> 123.45 | -12.34 | ERROR
connection status:          1 character     -> (write wifi icon to display SRAM)
progress in hours:          9 characters    -> 1234/5678
number of notifications:    3 characters    -> 12 (bell icon)
*/



/* display update period in multiples of 10ms */
#define LCD_REFRESH_PERIOD 5

typedef enum display_state {
    DISP_STATE_INIT,          /* initial display configuration */
    DISP_STATE_ON,            /* display ready to display data */
    DISP_STATE_OFF,           /* waiting for event to wake up */
    DISP_STATE_ERROR,         /* display encountered errors, trying to recover */
    DISP_STATE_COUNT
}display_state_t;

typedef void (*display_state_handle_t)(void);

typedef struct display_context {
    display_state_t current_state;
    display_buffer_t screen_data;        /* whats requested on the display*/
    display_buffer_t screen_data_shadow; /* whats on the display */
    uint8_t display_brightness;
    display_control_t requested_status;
}display_context_t;

typedef struct sensor_state_table {
    display_state_t name;
    display_state_handle_t handle;
}sensor_state_table_t;

/* declaration of a handle for each state */
static void display_state_init(void);
static void display_state_on(void);
static void display_state_off(void);
static void display_state_error(void);

static void display_set_state(display_state_t new_state);
static display_state_t display_get_state(void);

static void display_update_screen(void);

static const struct device *lcd_dev = DEVICE_DT_GET(_LCD_NODE);
static display_context_t display_context;

static sensor_state_table_t display_state_handles[] = {
    {DISP_STATE_INIT,         display_state_init          },
    {DISP_STATE_ON,           display_state_on            },
    {DISP_STATE_OFF,          display_state_off           },
    {DISP_STATE_ERROR,        display_state_error         }
};

void display_init(){
    display_context = (display_context_t){
        .current_state = DISP_STATE_INIT,
        .display_brightness = 0x00U
    };

    // clear buffers
    memset(display_context.screen_data.data, 0U, sizeof(display_buffer_t));
    memset(display_context.screen_data_shadow.data, 0U, sizeof(display_buffer_t));
}

void display_main(){
    int i = 0;
    for (i = 0; i < DISP_STATE_COUNT; i++){
        if (display_context.current_state == display_state_handles[i].name){
            display_state_handles[i].handle();
        }
    }
}

void display_control(display_control_t onoff){
    switch (onoff) {
    case DISP_ON:
        display_context.requested_status = DISP_ON;
        break;
    case DISP_OFF:
        display_context.requested_status = DISP_OFF;
        break;
    default:
        printk("Invalid disp on/off request: %d", onoff);
        break;
    }
}

void display_set_text(const display_buffer_t *text){
    // copy the text to buffer
    memcpy((void*)display_context.screen_data.data, text, sizeof(display_buffer_t));
}

void display_set_brightness(uint8_t brightness){
    display_context.display_brightness = brightness;
}

static void display_state_init(void){

    int result = 0;

    if (false == device_is_ready(lcd_dev)) {
        printk("Auxdisplay device is not ready.");
        return;
    }

    // enable communication
    result = auxdisplay_cursor_set_enabled(lcd_dev, false);
    if (0 != result) {
        printk("Failed to disable cursor: %d", result);
        return;
    }

    display_set_state(DISP_STATE_OFF);
}

static void display_state_on(void){

    int result = 0;
    uint8_t current_backlight = 0x00U;

    if (DISP_OFF == display_context.requested_status){

        result = auxdisplay_display_off(lcd_dev);
        if (result != 0) {
            printk("Failed to turn display off: %d", result);
            return;
        }

        // clear shadow buffer
        memset(display_context.screen_data_shadow.data, 0U, sizeof(display_buffer_t));
        display_set_state(DISP_STATE_OFF);

        return;
    }

    // update screen data
    display_update_screen();

    // update brightness if needed
    result = auxdisplay_backlight_get(lcd_dev, &current_backlight);
    if (display_context.display_brightness != current_backlight || 0 > result) {
        // not checking result, if it fails, it will be triggered again
        result = auxdisplay_backlight_set(lcd_dev, display_context.display_brightness);

    }

}

static void display_state_off(void){

    int result = 0;
    uint8_t current_backlight = 0xFFU;

    // was there a display ON request
    if (DISP_ON == display_context.requested_status){
        result = auxdisplay_display_on(lcd_dev);
        if (result != 0) {
            printk("Failed to turn display on: %d", result);
            return;
        }
        display_set_state(DISP_STATE_ON);
        return;
    }

    // make sure the backlight is off
    result = auxdisplay_backlight_get(lcd_dev, &current_backlight);
    if (0x00U != current_backlight || 0 > result) {
        // not checking result, if it fails, it will be triggered again
        (void)auxdisplay_backlight_set(lcd_dev, 0x00U);
    }
}

static void display_state_error(void){

}

static void display_set_state(display_state_t new_state){
    if (DISP_STATE_COUNT > new_state){
        display_context.current_state = new_state;
    }
}

static display_state_t display_get_state(){
    return display_context.current_state;
}

static void display_update_screen(){

    int i = 0;
    int result = 0;

    display_buffer_t *requested_buff = &display_context.screen_data;
    display_buffer_t *shadow_buff = &display_context.screen_data_shadow;

    for (i = 0; i < sizeof(display_buffer_t); i++){
        if (shadow_buff->data[i] != requested_buff->data[i]){

            // set cursor
            result = auxdisplay_cursor_position_set(lcd_dev,
                        AUXDISPLAY_POSITION_ABSOLUTE,
                        i % _LCD_COLUMNS,
                        i / _LCD_COLUMNS
                    );
            if (0 > result){
                continue;
            }

            // write the character
            result = auxdisplay_write(lcd_dev, &requested_buff->data[i], 1);
            if (0 > result){
                continue;
            }

            // update shadow register
            shadow_buff->data[i] = requested_buff->data[i];
        }
    }
}
