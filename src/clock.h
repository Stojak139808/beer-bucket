#ifndef __CLOCK_H__
#define __CLOCK_H__

#include <zephyr/drivers/rtc.h>

void clock_init(void);
bool clock_is_ready(void);
int clock_get_time(struct rtc_time *time_ptr);

#endif /* __CLOCK_H__ */
