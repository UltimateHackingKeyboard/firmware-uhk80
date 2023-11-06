#ifndef __DEVICE_H__
#define __DEVICE_H__

// Includes:

    #include "autoconf.h"

// Macros:

    #define DEVICE_ID_UHK60V1_RIGHT 1
    #define DEVICE_ID_UHK60V2_RIGHT 2
    #define DEVICE_ID_UHK80_LEFT 3
    #define DEVICE_ID_UHK80_RIGHT 4

    #if CONFIG_DEVICE_ID == DEVICE_ID_UHK60V1_RIGHT
        #define DEVICE_NAME "UHK 60 v1"
        #define USB_DEVICE_PRODUCT_ID 0x6122
    #elif CONFIG_DEVICE_ID == DEVICE_ID_UHK60V2_RIGHT
        #define DEVICE_NAME "UHK 60 v2"
        #define USB_DEVICE_PRODUCT_ID 0x6124
    #elif CONFIG_DEVICE_ID == DEVICE_ID_UHK80_LEFT
        #define DEVICE_NAME "UHK 80 left half"
        #define USB_DEVICE_PRODUCT_ID 0xffff // TODO
    #elif CONFIG_DEVICE_ID == DEVICE_ID_UHK80_RIGHT
        #define DEVICE_NAME "UHK 80 right half"
        #define USB_DEVICE_PRODUCT_ID 0xffff // TODO
    #endif

    #if CONFIG_DEVICE_ID == DEVICE_ID_UHK80_LEFT
        #define DEVICE_HAS_MERGE_SENSE
    #endif

    #if CONFIG_DEVICE_ID == DEVICE_ID_UHK80_RIGHT
        #define DEVICE_HAS_OLED
    #endif

    #if CONFIG_DEVICE_ID == DEVICE_ID_UHK80_LEFT || CONFIG_DEVICE_ID == DEVICE_ID_UHK80_RIGHT
        #define DEVICE_HAS_NRF
        #define DEVICE_HAS_BATTERY
    #endif

#endif