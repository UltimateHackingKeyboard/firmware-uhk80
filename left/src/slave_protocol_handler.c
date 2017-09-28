#include "slave_protocol_handler.h"
#include "test_led.h"
#include "main.h"
#include "i2c_addresses.h"
#include "i2c.h"
#include "led_pwm.h"
#include "slave_protocol.h"
#include "main.h"
#include "init_peripherals.h"
#include "bool_array_converter.h"
#include "bootloader.h"

i2c_message_t RxMessage;
i2c_message_t TxMessage;

void SetError(uint8_t error);
void SetGenericError(void);
void SetResponseByte(uint8_t response);

void SetError(uint8_t error) {
    TxMessage.data[0] = error;
}

void SetGenericError(void)
{
    SetError(PROTOCOL_RESPONSE_GENERIC_ERROR);
}

// Set a single byte as the response.
void SetResponseByte(uint8_t response)
{
    TxMessage.data[1] = response;
}

void SlaveRxHandler(void)
{
    uint8_t commandId = RxMessage.data[0];
    switch (commandId) {
        case SlaveCommand_SetTestLed:
            TxMessage.length = 0;
            bool isLedOn = RxMessage.data[1];
            TEST_LED_SET(isLedOn);
            break;
        case SlaveCommand_SetLedPwmBrightness:
            TxMessage.length = 0;
            uint8_t brightnessPercent = RxMessage.data[1];
            LedPwm_SetBrightness(brightnessPercent);
            break;
        case SlaveCommand_JumpToBootloader:
            JumpToBootloader();
            break;
    }
}

void SlaveTxHandler(void)
{
    uint8_t commandId = RxMessage.data[0];
    switch (commandId) {
        case SlaveCommand_RequestKeyStates:
            BoolBytesToBits(keyMatrix.keyStates, TxMessage.data, LEFT_KEYBOARD_HALF_KEY_COUNT);
            TxMessage.length = KEY_STATE_SIZE;
            CRC16_UpdateMessageChecksum(&TxMessage);
            break;
    }
}
