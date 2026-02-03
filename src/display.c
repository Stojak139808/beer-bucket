#include <zephyr/device.h>
#include <zephyr/drivers/auxdisplay.h>

static const struct device *lcd_dev = DEVICE_DT_GET(DT_NODELABEL(lcd));

void display_init(){

    if (!device_is_ready(lcd_dev)) {
        printk("LCD not ready\n");
    }
}

void display_main(){
    int rc;
    uint8_t data[64];

    if (!device_is_ready(lcd_dev)) {
        printk("Auxdisplay device is not ready.");
        return;
    }

    auxdisplay_backlight_set(lcd_dev, 0xffU);

    rc = auxdisplay_display_on(lcd_dev);
    if (rc != 0) {
        printk("Failed to turn display on: %d", rc);
        return;
    }

    rc = auxdisplay_cursor_set_enabled(lcd_dev, true);

    if (rc != 0) {
        printk("Failed to enable cursor: %d", rc);
    }

    snprintk(data, sizeof(data), "Kocham Kociare <3");
    rc = auxdisplay_write(lcd_dev, data, strlen(data));

    if (rc != 0) {
        printk("Failed to write data: %d", rc);
    }
    return;
}