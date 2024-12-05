#include <stdint.h>
#include <string.h>
#ifndef __ZEPHYR__
#include "fsl_i2c.h"
#include "i2c_watchdog.h"
#include "right_key_matrix.h"
#endif
#include "usb_command_get_debug_buffer.h"
#include "usb_protocol_handler.h"
#include "slave_scheduler.h"
#include "buffer.h"
#include "timer.h"
#include "usb_report_updater.h"
#include "usb_interfaces/usb_interface_basic_keyboard.h"
#include "usb_interfaces/usb_interface_media_keyboard.h"
#include "usb_interfaces/usb_interface_system_keyboard.h"
#include "usb_interfaces/usb_interface_mouse.h"
#include "usb_interfaces/usb_interface_gamepad.h"

uint8_t DebugBuffer[USB_GENERIC_HID_IN_BUFFER_LENGTH];

void UsbCommand_GetDebugBuffer(const uint8_t *GenericHidOutBuffer, uint8_t *GenericHidInBuffer)
{
#ifndef __ZEPHYR__
    SetDebugBufferUint32(1, I2C_Watchdog);
    SetDebugBufferUint32(5, I2cSlaveScheduler_Counter);
    SetDebugBufferUint32(9, I2cWatchdog_WatchCounter);
    SetDebugBufferUint32(13, I2cWatchdog_RecoveryCounter);
    SetDebugBufferUint32(17, MatrixScanCounter);
    SetDebugBufferUint32(21, UsbReportUpdateCounter);
#endif
    SetDebugBufferUint32(25, CurrentTime);
#ifndef __ZEPHYR__
    SetDebugBufferUint32(29, UsbGenericHidActionCounter);
    SetDebugBufferUint32(33, UsbBasicKeyboardActionCounter);
    SetDebugBufferUint32(37, UsbMediaKeyboardActionCounter);
    SetDebugBufferUint32(41, UsbSystemKeyboardActionCounter);
    SetDebugBufferUint32(45, UsbMouseActionCounter);
    SetDebugBufferUint32(49, UsbGamepadActionCounter);
#endif

    memcpy(GenericHidInBuffer, DebugBuffer, USB_GENERIC_HID_IN_BUFFER_LENGTH);
}

void SetDebugBufferUint8(uint32_t offset, uint8_t value)
{
    SetBufferUint8(DebugBuffer, offset, value);
}

void SetDebugBufferUint16(uint32_t offset, uint16_t value)
{
    SetBufferUint16(DebugBuffer, offset, value);
}

void SetDebugBufferUint32(uint32_t offset, uint32_t value)
{
    SetBufferUint32(DebugBuffer, offset, value);
}

void SetDebugBufferInt8(uint32_t offset, int8_t value)
{
    SetBufferInt8(DebugBuffer, offset, value);
}

void SetDebugBufferInt16(uint32_t offset, int16_t value)
{
    SetBufferInt16(DebugBuffer, offset, value);
}

void SetDebugBufferInt32(uint32_t offset, int32_t value)
{
    SetBufferInt32(DebugBuffer, offset, value);
}

void SetDebugBufferFloat(uint32_t offset, float value)
{
    *(float*)(DebugBuffer + offset) = value;
}
