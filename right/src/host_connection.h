#ifndef __HOST_CONNECTION_H__
#define __HOST_CONNECTION_H__

// Includes:

    #include <stdint.h>
    #include <stdbool.h>
    #include "str_utils.h"

#ifdef __ZEPHYR__

    #include <zephyr/kernel.h>
    #include <zephyr/bluetooth/bluetooth.h>
    #include <zephyr/bluetooth/addr.h>

#else
    typedef struct {
        uint8_t val[6];
    } bt_addr_t;

    struct bt_addr_le {
        uint8_t      type;
        bt_addr_t    a;
    };

    typedef struct bt_addr_le bt_addr_le_t;
#endif

// Macros:

    #define HOST_CONNECTION_COUNT_MAX 22

    #define BLE_ADDRESS_LENGTH 6

// Typedefs:

    typedef enum {
        HostConnectionType_Empty,
        HostConnectionType_UsbRight,
        HostConnectionType_UsbLeft,
        HostConnectionType_Ble,
        HostConnectionType_Dongle,
        HostConnectionType_Count,
    } host_connection_type_t;

    typedef struct {
        host_connection_type_t type;
        bt_addr_le_t bleAddress;
        string_segment_t name;
        bool switchover;
    } ATTR_PACKED host_connection_t;

// Variables:

    extern host_connection_t HostConnections[HOST_CONNECTION_COUNT_MAX];

// Functions:

    bool HostConnections_IsKnownBleAddress(const bt_addr_le_t *address);

#endif
