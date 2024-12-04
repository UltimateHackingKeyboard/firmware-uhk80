#include <string.h>
#include "usb_commands/usb_command_apply_config.h"
#include "config_parser/config_globals.h"
#include "config_parser/parse_config.h"
#include "peripherals/reset_button.h"
#include "usb_protocol_handler.h"
#include "keymap.h"
#include "macro_events.h"
#include "macros/core.h"
#include "macros/status_buffer.h"
#include "debug.h"
#include "config_manager.h"
#include "led_manager.h"
#include "versioning.h"

#ifdef __ZEPHYR__
#include "state_sync.h"
#include "event_scheduler.h"
#include "main.h"
#endif

void updateUsbBuffer(uint8_t *GenericHidInBuffer, uint8_t usbStatusCode, uint16_t parserOffset, parser_stage_t parserStage)
{
    SetUsbTxBufferUint8(0, usbStatusCode);
    SetUsbTxBufferUint16(1, parserOffset);
    SetUsbTxBufferUint8(3, parserStage);
}

static uint8_t validateConfig(uint8_t *GenericHidInBuffer) {
    // Validate the staging configuration.
    ParserRunDry = true;
    StagingUserConfigBuffer.offset = 0;
    uint8_t parseConfigStatus = ParseConfig(&StagingUserConfigBuffer);
    if (GenericHidInBuffer) {
        updateUsbBuffer(GenericHidInBuffer, UsbStatusCode_Success, StagingUserConfigBuffer.offset, ParsingStage_Validate);
    }
    return parseConfigStatus;
}

void UsbCommand_ApplyConfigAsync(const uint8_t *GenericHidOutBuffer, uint8_t *GenericHidInBuffer) {
    if (validateConfig(GenericHidInBuffer) == UsbStatusCode_Success) {
        EventVector_Set(EventVector_ApplyConfig);
#ifdef __ZEPHYR__
        Main_Wake();
#endif
    }
}

void UsbCommand_ApplyFactory(const uint8_t *GenericHidOutBuffer, uint8_t *GenericHidInBuffer)
{
    EventVector_Unset(EventVector_ApplyConfig);

    DataModelVersion = userConfigVersion;

    Macros_ClearStatus();

    ConfigManager_ResetConfiguration(false);

#ifdef __ZEPHYR__
    StateSync_ResetConfig();
    StateSync_ResetRightLeftLink(true);
#endif

    LedManager_FullUpdate();
}

uint8_t UsbCommand_ApplyConfig(const uint8_t *GenericHidOutBuffer, uint8_t *GenericHidInBuffer)
{
    static bool isBoot = true;
    EventVector_Unset(EventVector_ApplyConfig);

    uint8_t parseConfigStatus = validateConfig(GenericHidInBuffer);

    if (parseConfigStatus != UsbStatusCode_Success) {
        return parseConfigStatus;
    }

    // Make the staging configuration the current one.
    char oldKeymapAbbreviation[KEYMAP_ABBREVIATION_LENGTH];
    uint8_t oldKeymapAbbreviationLen;
    memcpy(oldKeymapAbbreviation, AllKeymaps[CurrentKeymapIndex].abbreviation, KEYMAP_ABBREVIATION_LENGTH);
    oldKeymapAbbreviationLen = AllKeymaps[CurrentKeymapIndex].abbreviationLen;

    uint8_t *temp = ValidatedUserConfigBuffer.buffer;
    ValidatedUserConfigBuffer.buffer = StagingUserConfigBuffer.buffer;
    StagingUserConfigBuffer.buffer = temp;

    if (IsFactoryResetModeEnabled) {
        return UsbStatusCode_Success;
    }

    ConfigManager_ResetConfiguration(false);

    ParserRunDry = false;
    ValidatedUserConfigBuffer.offset = 0;
    parseConfigStatus = ParseConfig(&ValidatedUserConfigBuffer);

    if (parseConfigStatus != UsbStatusCode_Success) {
        Macros_ReportErrorPrintf(NULL, "UserConfig validated successfuly, but failed to parse. This wasn't supposed to happen.");
        return parseConfigStatus;
    }

    Macros_ClearStatus();

    if (!isBoot) {
        Macros_ValidateAllMacros();
    }

    MacroEvent_OnInit();

#ifdef __ZEPHYR__
    StateSync_ResetConfig();
#endif

    // Switch to the keymap of the updated configuration of the same name or the default keymap.
    if (SwitchKeymapByAbbreviation(oldKeymapAbbreviationLen, oldKeymapAbbreviation)) {
        return UsbStatusCode_Success;
    }

    SwitchKeymapById(DefaultKeymapIndex);
    isBoot = false;

    return UsbStatusCode_Success;
}
