#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <string.h>
#include "display.h"
#include "temp_sensor.h"
#include "sys.h"

/* entrypoint and system configuration for the application */

#define TEMP_SENSOR_THREAD_PERIOD_MS    5000
#define DISPLAY_THREAD_PERIOD_MS        200
#define SYS_THREAD_PERIOD_MS            200

static struct k_sem temp_sensor_tick_sem;
static struct k_sem display_tick_sem;
static struct k_sem sys_tick_sem;

static void temp_sensor_timer_handler(struct k_timer *dummy);
static void temp_sensor_thread(void *a, void *b, void *c);

static void display_timer_handler(struct k_timer *dummy);
static void display_thread(void *a, void *b, void *c);

static void sys_timer_handler(struct k_timer *dummy);
static void sys_thread(void *a, void *b, void *c);

static void temp_sensor_timer_handler(struct k_timer *dummy){
    (void)dummy;
    k_sem_give(&temp_sensor_tick_sem);
}
K_TIMER_DEFINE(temp_sensor_timer, temp_sensor_timer_handler, NULL);

static void display_timer_handler(struct k_timer *dummy){
    (void)dummy;
    k_sem_give(&display_tick_sem);
}
K_TIMER_DEFINE(display_timer, display_timer_handler, NULL);

static void sys_timer_handler(struct k_timer *dummy){
    (void)dummy;
    k_sem_give(&sys_tick_sem);
}
K_TIMER_DEFINE(sys_timer, sys_timer_handler, NULL);

static void temp_sensor_thread(void *a, void *b, void *c){

    k_sem_init(&temp_sensor_tick_sem, 0, 1);

    temp_sensor_init();

    k_timer_start(&temp_sensor_timer, K_NO_WAIT, K_MSEC(TEMP_SENSOR_THREAD_PERIOD_MS));

    for (;;) {
        k_sem_take(&temp_sensor_tick_sem, K_FOREVER);

        temp_sensor_main();
    }
}

static void display_thread(void *a, void *b, void *c){

    k_sem_init(&display_tick_sem, 0, 1);

    display_init();

    k_timer_start(&display_timer, K_NO_WAIT, K_MSEC(DISPLAY_THREAD_PERIOD_MS));

    for (;;) {
        k_sem_take(&display_tick_sem, K_FOREVER);
        display_main();
    }

}

static void sys_thread(void *a, void *b, void *c){

    k_sem_init(&sys_tick_sem, 0, 1);

    sys_init(SYS_THREAD_PERIOD_MS);

    k_timer_start(&sys_timer, K_NO_WAIT, K_MSEC(SYS_THREAD_PERIOD_MS));

    for (;;) {
        k_sem_take(&sys_tick_sem, K_FOREVER);
        sys_main();
    }

}

int main(void){

}

K_THREAD_DEFINE(temp_sensor_thread_id, 1024, temp_sensor_thread,
                NULL, NULL, NULL, 3, 0, 0);

K_THREAD_DEFINE(display_thread_id, 1024, display_thread,
                NULL, NULL, NULL, 4, 0, 0);

K_THREAD_DEFINE(sys_thread_id, 1024, sys_thread,
                NULL, NULL, NULL, 4, 0, 0);
