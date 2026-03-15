#ifndef __RELAY_H__
#define __RELAY_H__

#include <zephyr/drivers/sensor.h>

typedef enum relay_state {
    RELAY_ON,
    RELAY_OFF,
    RELAY_ERR
}relay_state_t;

/*
 *          ------------------ ON
 *          |           |
 *          |           |
 *          A           B
 *          |           |
 *          |           |
 * OFF ------------------
 */
typedef struct relay_hysteresis {
    struct sensor_value A;
    struct sensor_value B;
}relay_hysteresis_t;

int relay_init(void);
void relay_set(relay_state_t state);
relay_state_t relay_get(void);

relay_hysteresis_t relay_define_hysteresis(struct sensor_value A, struct sensor_value B);
relay_state_t relay_get_hysteresis_output(relay_hysteresis_t *hysteresis,
    struct sensor_value *input, relay_state_t curr_state);

bool relay_is_ready(void);

#endif /* __RELAY_H__ */
