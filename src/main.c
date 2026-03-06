#include <zephyr/kernel.h>
#include <string.h>
#include "display.h"
#include "temp_sensor.h"


static struct k_sem temp_sensor_tick_sem;

static void temp_sensor_timer_handler(struct k_timer *dummy);
static void temp_sensor_thread(void *a, void *b, void *c);

static void temp_sensor_timer_handler(struct k_timer *dummy){
    k_sem_give(&temp_sensor_tick_sem);
}

K_TIMER_DEFINE(temp_sensor_timer, temp_sensor_timer_handler, NULL);

static void temp_sensor_thread(void *a, void *b, void *c){

    k_sem_init(&temp_sensor_tick_sem, 0, 1);

    temp_sensor_init();

    k_timer_start(&temp_sensor_timer, K_SECONDS(1), K_SECONDS(5));

    for(;;){
        k_sem_take(&temp_sensor_tick_sem, K_FOREVER);

        temp_sensor_main();
    }
}

int main(void)
{

    display_buffer_t screen_text;

    memcpy(screen_text.data,
        "hello                          ",
        sizeof(display_buffer_t));

    printk("start\n");


    display_init();

    display_set_text(&screen_text);
    display_set_brightness(0xFFU);

    for(;;){
        display_main();
        k_sleep(K_MSEC(500));
    }

}

K_THREAD_DEFINE(temp_sensor_thread_id, 1024, temp_sensor_thread,
                NULL, NULL, NULL, 3, 0, 0);