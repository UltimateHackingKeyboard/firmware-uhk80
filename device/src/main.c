#include "bt_hid.h"
#include "keyboard/key_scanner.h"
#include "keyboard/leds.h"
#include "keyboard/oled.h"
#include "keyboard/charger.h"
#include "keyboard/spi.h"
#include "keyboard/uart.h"
#include "bt_central_uart.h"
#include "bt_peripheral_uart.h"
#include "keyboard/i2c.h"
#include "keyboard/merge_sensor.h"
#include "shell.h"
#include "device.h"
#include "usb/usb.hpp"
#include <zephyr/drivers/gpio.h>
#include "bt_conn.h"
#include "settings.h"

int main(void) {
    printk("----------\n" DEVICE_NAME " started\n");

#if CONFIG_DEVICE_ID != DEVICE_ID_UHK_DONGLE
    InitUart();
    InitI2c();
    InitSpi();

    #ifdef DEVICE_HAS_OLED
    InitOled();
    #endif // DEVICE_HAS_OLED

    InitLeds();
    InitCharger();
    InitMergeSensor();
    InitKeyScanner();
#endif // CONFIG_DEVICE_ID != DEVICE_ID_UHK_DONGLE
    usb_init(true);
    bt_init();
    InitSettings();

#if CONFIG_DEVICE_ID == DEVICE_ID_UHK80_LEFT
    InitPeripheralUart();
#elif CONFIG_DEVICE_ID == DEVICE_ID_UHK80_RIGHT
    bt_hid_init();
    InitCentralUart();
#endif

}
