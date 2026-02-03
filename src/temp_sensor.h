#ifndef __TEMP_SENSOR_H__
#define __TEMP_SENSOR_H__

#include <zephyr/drivers/sensor.h>

typedef enum temp_sensor_id {
    //MAIN_TEMPERATURE_SENSOR,
    AUX_TEMPERATURE_SENSOR,
    TEMP_SENSOR_COUNT
}temp_sensor_id_t;

void temp_sensor_init(void);
void temp_sensor_main(void);
bool get_temperature(temp_sensor_id_t id, struct sensor_value *temperature);

#endif /* __TEMP_SENSOR_H__ */
