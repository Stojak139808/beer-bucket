#include <zephyr/drivers/w1.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/atomic.h>
#include "temp_sensor.h"

/* Number of maximum consecutive read errors for auxilary temperature sensor */
#define AUX_SENSOR_ERR_MAX 5

typedef struct temp_sensor_attr {
    enum sensor_channel channel;
    enum sensor_attribute attribute;
    struct sensor_value value;
}temp_sensor_attr_t;

typedef void (*sensor_state_handle_t)(void);

typedef enum sensor_state {
    STATE_INITIALIZING,
    STATE_RUNNING,
    STATE_ERROR,
    STATE_COUNT
}sensor_state_t;

typedef struct sensor_state_table {
    sensor_state_t name;
    sensor_state_handle_t handle;
}sensor_state_table_t;

typedef struct sensor_context {
    sensor_state_t current_state;

    /* double buffer to avoid partial read by other thread*/
    struct sensor_value current_temperature_buf[2]; 
    /* either 1 or 0 for current_temperature_buf */
    atomic_t current_temperature_id;

    int err_read_cnt;
    bool is_reading_valid;
}sensor_context_t;

typedef struct sensor {
    const temp_sensor_id_t id;
    sensor_context_t *const context;
    const sensor_state_table_t *const state_table;
    const struct device *const dev;
}sensor_t;

static bool sensor_init_attributes(const struct device *dev, temp_sensor_attr_t *attributes, int n_attr);
static void temp_sensor_set_state(sensor_context_t *context, sensor_state_t new_state);
static void execute_sensor_state(const sensor_t *sensor);

/* auxilary sensor configuration */
static void aux_temp_sensor_init(void);
static void aux_temp_sensor_running(void);
static void aux_temp_sensor_error(void);

const sensor_state_table_t aux_sensor_states[] = {
    {STATE_INITIALIZING,    aux_temp_sensor_init    },
    {STATE_RUNNING,         aux_temp_sensor_running },
    {STATE_ERROR,           aux_temp_sensor_error   }
};

static temp_sensor_attr_t aux_temp_sensor_attr[] = {
    {SENSOR_CHAN_AMBIENT_TEMP, SENSOR_ATTR_FULL_SCALE,          {128, 0}},
    //{SENSOR_CHAN_AMBIENT_TEMP, SENSOR_ATTR_SAMPLING_FREQUENCY,  {8,   0}}
};

const struct device *const aux_temp_dev = DEVICE_DT_GET_ANY(ti_tmp112);
static sensor_context_t aux_temp_sensor_context;

/* main sensor configuration */
const struct device *const main_temp_dev = DEVICE_DT_GET(DT_NODELABEL(w1));
static sensor_context_t main_temp_sensor_context;


/* combined struct for functions that are common for all sensors */
static sensor_t sensors[] = {
    // auxilary temperature config
    {
        .id = AUX_TEMPERATURE_SENSOR,
        .dev = aux_temp_dev,
        .context = &aux_temp_sensor_context,
        .state_table = aux_sensor_states
    },
    {
        .id = 0xff,
        .dev = main_temp_dev,
        .context = NULL,
        .state_table = NULL
    }
};


void temp_sensor_init(){

    int i = 0;

    /* initialize context for all sensors */
    for (i = 0; i < TEMP_SENSOR_COUNT; i++){
        *sensors[i].context = (sensor_context_t){
                .current_state = STATE_INITIALIZING,
                .current_temperature_buf[0] = (struct sensor_value){0,0},
                .current_temperature_buf[1] = (struct sensor_value){0,0},
                .current_temperature_id = ATOMIC_INIT(0),
                .err_read_cnt = 0,
                .is_reading_valid = false
            };
    }
}

void temp_sensor_main(){
    int i = 0;
    for (i = 0; i < TEMP_SENSOR_COUNT; i++){
        execute_sensor_state(&sensors[i]);
    }
}

bool get_temperature(temp_sensor_id_t id, struct sensor_value *temperature){

    sensor_context_t *context = NULL;
    int buff_id = 0;
    bool reading_validity = false;

    int i = 0;
    for (i = 0; i < TEMP_SENSOR_COUNT; i++) {
        if (sensors[i].id == id){
            context = sensors[i].context;
            buff_id = atomic_get(&context->current_temperature_id);
            if (context->is_reading_valid){
                *temperature = context->current_temperature_buf[buff_id];
                reading_validity = true;
            }
        }
    }
    return reading_validity;
}

static void execute_sensor_state(const sensor_t *sensor){

    const sensor_state_table_t *states = sensor->state_table;
    sensor_state_t current_state = sensor->context->current_state;
    int i = 0;

    for (i = 0; i < STATE_COUNT; i++){
        if (current_state == states[i].name){
            states[i].handle();
        }
    }
}

static void temp_sensor_set_state(sensor_context_t *context, sensor_state_t new_state){
    context->current_state = new_state;
}

static void aux_temp_sensor_init(){

    bool ret = false;

    if (device_is_ready(aux_temp_dev)) {
        ret = sensor_init_attributes(aux_temp_dev, aux_temp_sensor_attr,
            sizeof(aux_temp_sensor_attr)/sizeof(aux_temp_sensor_attr[0]));
    }

    if (true == ret) {
        temp_sensor_set_state(&aux_temp_sensor_context, STATE_RUNNING);
        printk("Auxilary sensor init done\n");
    }

}

static void aux_temp_sensor_running(){
    int ret = 0;
    int current_buf_id = atomic_get(&aux_temp_sensor_context.current_temperature_id);
    int next_buf_id = (current_buf_id & 0x01) ^ 0x01; // toggle ID

    struct sensor_value new_value = {0U, 0U};

    ret = sensor_sample_fetch(aux_temp_dev);
    if (0 != ret) {
        aux_temp_sensor_context.err_read_cnt++;
        printk("sensor_sample_fetch failed ret %d\n", ret);
        goto err;
    }

    ret = sensor_channel_get(aux_temp_dev, SENSOR_CHAN_AMBIENT_TEMP, &new_value);
    if (0 == ret) {
        aux_temp_sensor_context.current_temperature_buf[next_buf_id] = new_value;
        aux_temp_sensor_context.is_reading_valid = true;
        aux_temp_sensor_context.err_read_cnt = 0;
        
        /* publish the new value */
        atomic_set(&aux_temp_sensor_context.current_temperature_id, next_buf_id);
    }
    else {
        aux_temp_sensor_context.err_read_cnt++;
        printk("sensor_channel_get failed ret %d\n", ret);
        goto err;
    }
    
    printk("temp is %d (%d micro)\n", new_value.val1,
        new_value.val2);
    return;

err:
    if (AUX_SENSOR_ERR_MAX < aux_temp_sensor_context.err_read_cnt) {
        aux_temp_sensor_context.is_reading_valid = false;
        temp_sensor_set_state(&aux_temp_sensor_context, STATE_ERROR);
    }
    return;
}

void aux_temp_sensor_error(){

}

static bool sensor_init_attributes(const struct device *dev, temp_sensor_attr_t *attributes, int n_attr){

    int i = 0;
    int result = 0;
    temp_sensor_attr_t *attr = {0};

    for (i = 0; i < n_attr; i++){
        attr = &attributes[i];
        result = sensor_attr_set(dev, attr->channel,
                attr->attribute, &attr->value);

        if (result != 0){
            printk("Failed to set attributes for %s, attr n: %d, errn: %d\n",
                dev->name, i, result);
            return false;
        }
    }

    return true;
}

