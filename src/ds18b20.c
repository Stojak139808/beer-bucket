#include <zephyr/drivers/w1.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <errno.h>
#include "ds18b20.h"

#define DS18B20_FAMILY_CODE 0x28U

typedef enum ds18b20_message_id {
    DS18B20_CONVERT_T,
    DS18B20_READ_SCRATCHPAD,
    DS18B20_WRITE_SCRATCHPAD,
    DS18B20_COPY_SCRATCHPAD,
    DS18B20_RECALL_E_2,
    DS18B20_READ_POWER_SUPPLY,
    DS18B20_MESSAGE_COUNT
}ds18b20_message_id_t;

typedef struct ds18b20_context {
    bool is_discovered;
    bool was_T_issued;          /* start conversion command */
    uint8_t tx_buffer[10];
    uint8_t rx_buffer[10];
    int64_t trigger_time;       /* time of last convert T trigger */
    ds18b20_conversion_time_t conversion_time;

}ds18b20_context_t;

typedef struct ds18b20_command {
    uint8_t protocol;           /* command id */
    uint8_t tx_payload_len;     /* additional data length (without command id)*/
    uint8_t rx_len;             /* response length */
}ds18b20_command_t;

static void w1_search_callback(struct w1_rom rom, void *user_data);

static void ds18b20_prepare_buffers(ds18b20_message_id_t message_id, uint8_t *payload,
    uint8_t *tx_len, uint8_t *rx_len);
static int ds18b20_send_command(const struct device *w1_bus_dev, uint8_t *tx_len, uint8_t *rx_len);
static int16_t ds18b20_get_raw_temp(void);
static void ds18b20_temp_to_sensor_value(int16_t temperature, struct sensor_value *result);
static int ds18b20_get_curr_ticks_to_convert(void);

/* 
 * Has to be in the same order as ds18b20_message_id_t, which is used to access
 * this without any for loops!
 */
static const ds18b20_command_t ds18b20_messages[] = {
    {   /* Convert T */
        .protocol = 0x44U,
        .tx_payload_len = 0U,
        .rx_len = 1U
    },
    {   /* Read Scratchpad */
        .protocol = 0xBEU,
        .tx_payload_len = 0U,
        .rx_len = 9U
    },
    {   /* Write Scratchpad */
        .protocol = 0x4EU,
        .tx_payload_len = 3U,
        .rx_len = 0U
    },
    {   /* Copy Scratchpad */
        .protocol = 0x48U,
        .tx_payload_len = 0U,
        .rx_len = 0U
    },
    {   /* Recall E 2 */
        .protocol = 0xB8U,
        .tx_payload_len = 0U,
        .rx_len = 3U
    },
    {   /* Read Power Supply */
        .protocol = 0xB4U,
        .tx_payload_len = 0U,
        .rx_len = 1U
    }
};

/* DS18B20 config for zephyr,w1-gpio driver */
struct w1_slave_config ds18b20_w1_config = 
{
    .rom = W1_ROM_INIT_ZERO,
    .overdrive = 0U
};

static ds18b20_context_t ds18b20_context = {
    .is_discovered = false,
    .was_T_issued = false,
    .trigger_time = 0
};

int ds18b20_init(const struct device *w1_bus_dev){
    int result = 0U;
    uint8_t crc = 0x00U;
    uint8_t tx_length = 0;
    uint8_t rx_length = 0;

    if (false == device_is_ready(w1_bus_dev)) {
        return -ENOENT;
    }

    /* search for temperature sensor if we haven't found one already */
    if (false == ds18b20_context.is_discovered) {
        result = w1_search_rom(w1_bus_dev, w1_search_callback, NULL);
        if (result == 0){
            /* no devices found on w1 bus */
            return -EIO;
        }
        else if (result < 0){
            /* pass the error down */
            return result;
        }

        /* calculate CRC of ROM */
        crc = w1_crc8(&ds18b20_w1_config.rom, sizeof(struct w1_rom) - sizeof(uint8_t));
        if (crc == ds18b20_w1_config.rom.crc){
            /* device discovered and CRC verified */
            ds18b20_context.is_discovered = true;
        }
        else {
            printk("ds18b20 discovered, but returned ROM has bad CRC\n");
        }
    }

    if (true == ds18b20_context.is_discovered) {
        /* try to match rom with the discovered device */
        result = w1_match_rom(w1_bus_dev, &ds18b20_w1_config);
        if (result < 0) {
            /* error */
            return result;
        }

        ds18b20_prepare_buffers(DS18B20_READ_SCRATCHPAD, NULL, 
            &tx_length, &rx_length);

        result = w1_write_read(w1_bus_dev, &ds18b20_w1_config,
            ds18b20_context.tx_buffer, tx_length,
            ds18b20_context.rx_buffer, rx_length
        );

        if (result >= 0) {
            printk("write response:");
            for (int i = 0; i < rx_length; i++) {
                printk(" 0x%x", ds18b20_context.rx_buffer[i]);
            }
            printk("\n");
        }
        else {
            printk("reading scratchpad failed");
            return result;
        }
        
    }

    return 0;
}

int ds18b20_set_config(const struct device *w1_bus_dev, uint8_t TH, uint8_t TL,
    ds18b20_conversion_time_t resolution){

    int result = 0U;
    uint8_t payload[3] = {0};
    uint8_t tx_len = 0U;
    uint8_t rx_len = 0U;

    /* fill the payload */
    payload[0U] = TH;
    payload[1U] = TL;
    payload[2U] = ((resolution << 5U) | 0x1FU) & 0x7FU;

    ds18b20_prepare_buffers(DS18B20_WRITE_SCRATCHPAD, payload, &tx_len, &rx_len);

    result = ds18b20_send_command(w1_bus_dev, &tx_len, &rx_len);

    if (0U <= result) {
        ds18b20_context.conversion_time = resolution;
    }

    return result;

}

int ds18b20_trigger_conversion(const struct device *w1_bus_dev){
    int result = 0U;
    uint8_t tx_len = 0U;
    uint8_t rx_len = 0U;

    ds18b20_prepare_buffers(DS18B20_CONVERT_T, NULL, &tx_len, &rx_len);

    result = ds18b20_send_command(w1_bus_dev, &tx_len, &rx_len);

    if (0U <= result) {
        /* trigger successful */
        ds18b20_context.trigger_time = k_uptime_ticks();
        ds18b20_context.was_T_issued = true;
    }

    return result;

}

int ds18b20_get_temperature(const struct device *w1_bus_dev, struct sensor_value *new_value){

    int result = 0U;
    uint8_t tx_len = 0U;
    uint8_t rx_len = 0U;
    int16_t raw_temperature = 0x0000U;
    uint8_t crc = 0U;

    /* check if conversion was triggered */
    if (false == ds18b20_context.was_T_issued) {
        return -ENOEXEC;
    }

    /* did conversion time elapse? */
    if (ds18b20_context.trigger_time + ds18b20_get_curr_ticks_to_convert() > k_uptime_ticks()) {
        return -EBUSY;
    }

    ds18b20_prepare_buffers(DS18B20_READ_SCRATCHPAD, NULL, &tx_len, &rx_len);

    result = ds18b20_send_command(w1_bus_dev, &tx_len, &rx_len);

    if (0U > result) {
        return result;
    }

    /* check crc */
    crc = w1_crc8(&ds18b20_context.rx_buffer, rx_len - sizeof(uint8_t));
    if (crc != ds18b20_context.rx_buffer[rx_len - 1]){
        /* bad CRC */
        printk("bad crc when reading temperature\n");
        return -EBADMSG;
    }

    /* reset conversion triggered flag */
    ds18b20_context.was_T_issued = false;

    raw_temperature = ds18b20_get_raw_temp();
    ds18b20_temp_to_sensor_value(raw_temperature, new_value);

    return 0;
}

static int ds18b20_send_command(const struct device *w1_bus_dev, uint8_t *tx_len, uint8_t *rx_len){
    int result = 0U;

    result = w1_skip_rom(w1_bus_dev, &ds18b20_w1_config);

    if (-ENODEV == result) {
        /* try to match ROM before declaring failure */
        result = w1_match_rom(w1_bus_dev, &ds18b20_w1_config);
        if (0U > result) {
            goto exit;
        }
    }
    else if (0U > result) {
        goto exit;
    }

    result = w1_write_block(w1_bus_dev, ds18b20_context.tx_buffer, *tx_len);
    if (0U > result || 0U == *rx_len) {
        goto exit;
    }

    result = w1_read_block(w1_bus_dev, ds18b20_context.rx_buffer, *rx_len);

exit:
    return result;
}

static void w1_search_callback(struct w1_rom rom, void *user_data)
{
    if (DS18B20_FAMILY_CODE == rom.family) {
        /* Discovered device matches DS18B20 family code */
        memcpy((void*)&ds18b20_w1_config.rom, (void*)&rom, sizeof(struct w1_rom));

        /* set that the right sensor has been discovered */
        ds18b20_context.is_discovered = true;

        printk("DS18B20 registered with id: 0x%016llx\n", w1_rom_to_uint64(&rom));
    }

}

static void ds18b20_prepare_buffers(ds18b20_message_id_t message_id, uint8_t *payload,
    uint8_t *tx_len, uint8_t *rx_len){

    uint8_t *tx_buffer = ds18b20_context.tx_buffer;
    
    if (DS18B20_MESSAGE_COUNT <= message_id) {
        return;
    }
    
    *tx_len = ds18b20_messages[message_id].tx_payload_len;
    *rx_len = ds18b20_messages[message_id].rx_len;

    /* set command */
    tx_buffer[0U] = ds18b20_messages[message_id].protocol;

    /* set payload */
    if (NULL != payload && 0U < *tx_len) {
        memcpy((void*)&tx_buffer[1U], (void*)payload, *tx_len);
    }

    /* add protocol byte to total tx length */
    *tx_len += 1U;
}

static void ds18b20_temp_to_sensor_value(int16_t temperature, struct sensor_value *result){
    result->val1 = (temperature / 16);
    result->val2 = (((temperature % 16) * 1000000) >> 4U);
}

static int ds18b20_get_curr_ticks_to_convert(void){

    int conversion_time_us = 0;
    int conversion_ticks = 0;

    switch (ds18b20_context.conversion_time)
    {
    case DS18B20_RESOLUTION_9_BITS:
        /* 93.75ms */
        conversion_time_us = 93750;
        break;
    case DS18B20_RESOLUTION_10_BITS:
        /* 187.5ms */
        conversion_time_us = 187500;
        break;
    case DS18B20_RESOLUTION_11_BITS:
        /* 375ms */
        conversion_time_us = 375000;
        break;
    case DS18B20_RESOLUTION_12_BITS:
        /* 750ms */
        conversion_time_us = 750000;
        break;
    default:
        conversion_time_us = 0;
        break;
    }

    conversion_ticks = k_us_to_ticks_ceil32(conversion_time_us);

    return conversion_ticks;
}

static int16_t ds18b20_get_raw_temp(void){
    int16_t raw_temp = 0;
    uint8_t *scratch_pad = ds18b20_context.rx_buffer;

    raw_temp = scratch_pad[0U] | (scratch_pad[1U] << 8U);

    return raw_temp;
}