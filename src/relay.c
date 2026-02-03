#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include "relay.h"

#define GPIO_ON     1U
#define GPIO_OFF    0U

#define RELAY_NODE DT_NODELABEL(relay_main)

typedef struct relay_context {
    bool is_ready;
}relay_context_t;

static const struct gpio_dt_spec relay_gpio = GPIO_DT_SPEC_GET(RELAY_NODE, gpios);
static relay_context_t relay_context = {
    .is_ready = false
};

int relay_init(){

    int result = 0U;

    if (false == gpio_is_ready_dt(&relay_gpio)) {
        return result;
    }

    // should be off at startup
    result = gpio_pin_configure_dt(&relay_gpio, GPIO_OUTPUT_INACTIVE);
    if (0 != result) {
        return result;
    }

    relay_context.is_ready = true;

    return 0U;
}

void relay_set(relay_state_t state){
    if (true == relay_context.is_ready) {
        switch (state) {
        case RELAY_ON:
            (void)gpio_pin_set_dt(&relay_gpio, GPIO_ON);
            break;
        case RELAY_OFF:
            (void)gpio_pin_set_dt(&relay_gpio, GPIO_OFF);
            break;
        case RELAY_ERR:
        default:
            /* do nothing */
            break;
        }
    }
}

relay_state_t relay_get(){
    int status = gpio_pin_get_dt(&relay_gpio);
    relay_state_t ret = RELAY_ERR;
    switch (status) {
    case GPIO_ON:
        ret = RELAY_ON;
        break;
    case GPIO_OFF:
        ret = RELAY_OFF;
        break;
    };
    return ret;
}
