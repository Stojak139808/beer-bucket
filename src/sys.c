#include <zephyr/kernel.h>
#include "sys.h"
#include "display.h"
#include "relay.h"
#include "temp_sensor.h"
#include "clock.h"

#define SYS_RELAY_HYSTERESIS_A (struct sensor_value){-1, 0}
#define SYS_RELAY_HYSTERESIS_B (struct sensor_value){1, 0}

#define SYS_DISPLAY_DEGREES_UNIT_S "[" DISPLAY_DEGREES_S "C]"

#define SYS_DISPLAY_TASK_PERIOD_MS          1000
#define SYS_TEMP_CONTROL_TASK_PERIOD_MS     2000
#define SYS_DISPLAY_UPDATE_PERIOD_MS        5000

typedef bool (*module_init_function_t)(void);
typedef void (*sys_state_handle_t)(void);
typedef void (*sys_task_handle_t)(void);

typedef enum sys_state {
    SYS_STATE_INIT,
    SYS_STATE_RUNNING,
    SYS_STATE_ERROR,
    SYS_STATE_COUNT
}sys_state_t;

typedef enum sys_display_values {
    SYS_DISPLAY_MAIN_TEMP,
    SYS_DISPLAY_AMBIENT_TEMP,
    SYS_DISPLAY_SET_TEMP,
    SYS_DISPLAY_COUNT
}sys_display_values_t;

typedef struct sys_context {
    sys_state_t current_state;
    uint32_t call_period_ms;

    struct sensor_value planned_temperature;
    struct sensor_value temperature_error;
    relay_hysteresis_t output_hysteresis;
    sys_display_values_t currently_displayed;

    int32_t display_update_countdown_ms;

}sys_context_t;

typedef struct sys_init_entry {
    char module_name[16];
    module_init_function_t is_ready_check;  /* function to get module status */
    bool is_required;                       /* is the module required for basic functionality? */
}sys_init_entry_t;

typedef struct sys_state_table {
    sys_state_t name;
    sys_state_handle_t handle;
}sys_state_table_t;

typedef struct sys_task_entry {
    sys_task_handle_t task_handle;
    uint32_t period;                        /* period in ms */
    int32_t period_countdown;               /* current countdown in ms */
}sys_task_entry_t;

static void sys_state_init(void);
static void sys_state_running(void);
static void sys_state_error(void);
static void sys_set_state(sys_state_t new_state);

static void sys_display_control_task(void);
static void sys_temp_control_task(void);

static void sys_sensor_value_to_str(struct sensor_value *val, char *buff, int decimals);

static sys_context_t sys_context = {
    .current_state = SYS_STATE_INIT,
    .planned_temperature = {24, 00},
    .temperature_error = {0U, 0U},
    .currently_displayed = SYS_DISPLAY_MAIN_TEMP,
    .display_update_countdown_ms = 0
};

const sys_init_entry_t sys_init_checklist[] = {
    {
        .module_name = "relay",
        .is_ready_check = relay_is_ready,
        .is_required = true
    },
    {
        .module_name = "main sensor",
        .is_ready_check = temp_sensor_main_is_ready,
        .is_required = true
    },
    {
        .module_name = "ambient sensor",
        .is_ready_check = temp_sensor_aux_is_ready,
        .is_required = false
    },
    {
        .module_name = "display",
        .is_ready_check = display_is_ready,
        .is_required = false
    }
};

static sys_task_entry_t sys_task_list[] = {
    {
        .task_handle = sys_display_control_task,
        .period = SYS_DISPLAY_TASK_PERIOD_MS,
        .period_countdown = 0
    },
    {
        .task_handle = sys_temp_control_task,
        .period = SYS_TEMP_CONTROL_TASK_PERIOD_MS,
        .period_countdown = 0
    }
};

static sys_state_table_t sys_state_handles[] = {
    {SYS_STATE_INIT,         sys_state_init        },
    {SYS_STATE_RUNNING,      sys_state_running     },
    {SYS_STATE_ERROR,        sys_state_error       }
};



void sys_init(uint32_t sys_call_period_ms){

    sys_context.output_hysteresis = relay_define_hysteresis(SYS_RELAY_HYSTERESIS_A,
                                        SYS_RELAY_HYSTERESIS_B);

    sys_context.call_period_ms = sys_call_period_ms;

}

void sys_main(void){
    int i = 0;
    for (i = 0; i < SYS_STATE_COUNT; i++){
        if (sys_context.current_state == sys_state_handles[i].name){
            sys_state_handles[i].handle();
        }
    }
}

static void sys_state_init(void){

    int i = 0;
    bool are_modules_ready = true;
    bool is_module_ready = false;

    (void)relay_init();
    clock_init();

    /* check if all required modules are ready */
    for (i = 0; i < sizeof(sys_init_checklist)/sizeof(sys_init_entry_t); i++) {
        is_module_ready = sys_init_checklist[i].is_ready_check();
        if (false == is_module_ready && true == sys_init_checklist[i].is_required) {
            are_modules_ready = false;
        }
    }

    if (false == are_modules_ready) {
        /* not all modules are ready, wait */
        return;
    }

    /* reset display update timer */
    sys_context.display_update_countdown_ms = SYS_DISPLAY_UPDATE_PERIOD_MS;
    sys_set_state(SYS_STATE_RUNNING);

}

static void sys_state_running(void){
    int i = 0;
    for (i = 0; i < sizeof(sys_task_list)/sizeof(sys_task_entry_t); i++) {
        if (0U >= sys_task_list[i].period_countdown) {
            /* call task function */
            sys_task_list[i].task_handle();

            /* reset countdown to period */
            sys_task_list[i].period_countdown = sys_task_list[i].period;
        }
        else {
            /* tick down */
            sys_task_list[i].period_countdown -= sys_context.call_period_ms;
        }
    }
}

static void sys_state_error(void){

}

static void sys_display_control_task(void){

    struct sensor_value temperature = {0U,0U};
    display_buffer_t screen_text = {
        .data = {' '}
    };

    switch (sys_context.currently_displayed) {
        case SYS_DISPLAY_MAIN_TEMP:
            /* fetch main temperature */
            (void)temp_sensor_get_temperature(MAIN_TEMPERATURE_SENSOR, &temperature);
            snprintk(screen_text.data, sizeof(display_buffer_t),
            "main t" SYS_DISPLAY_DEGREES_UNIT_S ":");

            sys_sensor_value_to_str(&temperature, screen_text.data + LCD_COLUMNS, 2U);
            break;

        case SYS_DISPLAY_AMBIENT_TEMP:
            /* fetch ambient temperature from auxilary sensor */
            (void)temp_sensor_get_temperature(AUX_TEMPERATURE_SENSOR, &temperature);
            snprintk(screen_text.data, sizeof(display_buffer_t),
            "ambient t" SYS_DISPLAY_DEGREES_UNIT_S ":");

            sys_sensor_value_to_str(&temperature, screen_text.data + LCD_COLUMNS, 2U);
            break;

        case SYS_DISPLAY_SET_TEMP:
            snprintk(screen_text.data, sizeof(display_buffer_t),
            "target t" SYS_DISPLAY_DEGREES_UNIT_S ":");

            sys_sensor_value_to_str(&sys_context.planned_temperature,
                screen_text.data + LCD_COLUMNS, 2U);
            break;

        default:
            break;
    }

    display_set_brightness(0xFFU);
    display_set_text(&screen_text);

    /* tick down displayed value counter */
    sys_context.display_update_countdown_ms -= SYS_DISPLAY_TASK_PERIOD_MS;
    if (0 >= sys_context.display_update_countdown_ms) {
        sys_context.currently_displayed = (sys_context.currently_displayed + 1) % SYS_DISPLAY_COUNT;
        sys_context.display_update_countdown_ms = SYS_DISPLAY_UPDATE_PERIOD_MS;
    }
    printk("display_timer: %d\n", sys_context.display_update_countdown_ms);

}

static void sys_temp_control_task(void){
    bool temperature_validity = false;
    struct sensor_value current_temperature = {0};
    int64_t temperature_error_mili = 0; /* in mili degrees Celsius */
    relay_state_t current_relay_state = relay_get();
    relay_state_t new_relay_state = RELAY_OFF;

    temperature_validity = temp_sensor_get_temperature(MAIN_TEMPERATURE_SENSOR,
                                &current_temperature);

    if (false == temperature_validity) {
        return;
    }

    /* error = input - measurement */
    temperature_error_mili = sensor_value_to_milli(&sys_context.planned_temperature) -
                             sensor_value_to_milli(&current_temperature);

    sensor_value_from_micro(&sys_context.temperature_error, temperature_error_mili);

    new_relay_state = relay_get_hysteresis_output(&sys_context.output_hysteresis,
        &sys_context.temperature_error, current_relay_state);

    if ((RELAY_ON != new_relay_state) || (RELAY_OFF != new_relay_state)) {
        return;
    }

    relay_set(new_relay_state);

}

static void sys_sensor_value_to_str(struct sensor_value *val, char *buff, int decimals){

    int fraction = val->val2;
    int scale = 1000000; // 10^6, from sensor_value definition
    int i = 0;

    if (6 < decimals){
        // 10^-6 is the smallest possible decimal point
        return;
    }

    if (0 > fraction) {
        fraction = -fraction;
    }

    for (i = 0; decimals >= i; i++) {
        scale /= 10;
    }
    fraction /= scale;

    snprintk(buff, 6, "%d.%d", val->val1, fraction);
}

static void sys_set_state(sys_state_t new_state){
    if (SYS_STATE_COUNT > new_state){
        printk("SYS chagning from %d to %d\n", sys_context.current_state, new_state);
        sys_context.current_state = new_state;
    }
}
