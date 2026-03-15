#include <zephyr/kernel.h>
#include "sys.h"
#include "display.h"
#include "relay.h"
#include "temp_sensor.h"

typedef bool (*module_init_function_t)(void);
typedef void (*sys_state_handle_t)(void);
typedef void (*sys_task_handle_t)(void);

typedef enum sys_state {
    SYS_STATE_INIT,
    SYS_STATE_RUNNING,
    SYS_STATE_ERROR,
    SYS_STATE_COUNT
}sys_state_t;

typedef struct sys_context {
    sys_state_t current_state;
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
    uint32_t period;                        /* period in number of sys_main calls */
    uint32_t period_countdown;              /* current countdown in sys_main calls */
}sys_task_entry_t;

static void sys_state_init(void);
static void sys_state_running(void);
static void sys_state_error(void);

static void sys_display_control_task(void);
static void sys_temp_control_task(void);
static void sys_actuator_control_task(void);

static sys_context_t sys_context = {
    .current_state = SYS_STATE_INIT
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
        .period = 5,
        .period_countdown = 0
    },
    {
        .task_handle = sys_temp_control_task,
        .period = 5,
        .period_countdown = 0
    },
    {
        .task_handle = sys_actuator_control_task,
        .period = 5,
        .period_countdown = 0
    }
};

static sys_state_table_t sys_state_handles[] = {
    {SYS_STATE_INIT,         sys_state_init        },
    {SYS_STATE_RUNNING,      sys_state_running     },
    {SYS_STATE_ERROR,        sys_state_error       }
};

void sys_init(void){

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
            sys_task_list[i].period_countdown--;
        }
    }
}

static void sys_state_error(void){

}

static void sys_display_control_task(void){

}

static void sys_temp_control_task(void){

}

static void sys_actuator_control_task(void){

}
