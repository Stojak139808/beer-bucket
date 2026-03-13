#ifndef __DS18B20_H__
#define __DS18B20_H__

#include <zephyr/drivers/sensor.h>

/* type for DS18B20 Configuration Register */
typedef enum ds18b20_conversion_time {      /* conversion time */
    DS18B20_RESOLUTION_9_BITS,              /* 93.75ms         */
    DS18B20_RESOLUTION_10_BITS,             /* 187.5ms         */
    DS18B20_RESOLUTION_11_BITS,             /* 375ms           */
    DS18B20_RESOLUTION_12_BITS              /* 750ms           */
}ds18b20_conversion_time_t;

int ds18b20_init(const struct device *w1_bus_dev);
int ds18b20_set_config(const struct device *w1_bus_dev, uint8_t TH, uint8_t TL,
    ds18b20_conversion_time_t resolution);

int ds18b20_trigger_conversion(const struct device *w1_bus_dev);
int ds18b20_get_temperature(const struct device *w1_bus_dev, struct sensor_value *new_value);

#endif /* __DS18B20_H__ */
