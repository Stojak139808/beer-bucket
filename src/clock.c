#include <zephyr/device.h>
#include "clock.h"

typedef struct clock_context {
    bool is_ready;
}clock_context_t;

const struct device *const rtc_dev = DEVICE_DT_GET(DT_NODELABEL(rtc_ext));

static clock_context_t clock_context;

void clock_init(){

    int result = 0;

    clock_context = (clock_context_t){
        .is_ready = false
    };

    result = device_is_ready(rtc_dev);
    if (0 > result) {
        return;
    }

    clock_context.is_ready = true;

}

int clock_get_time(struct rtc_time *time_ptr)
{
    int result = 0;

    result = rtc_get_time(rtc_dev, time_ptr);
    if (result < 0) {
        printk("Cannot read date time: %d\n", result);
        return result;
    }

    printk("RTC date and time: %04d-%02d-%02d %02d:%02d:%02d\n", time_ptr->tm_year + 1900,
           time_ptr->tm_mon + 1, time_ptr->tm_mday, time_ptr->tm_hour,
           time_ptr->tm_min, time_ptr->tm_sec);

    return result;
}

bool clock_is_ready(void){
    return clock_context.is_ready;
}
