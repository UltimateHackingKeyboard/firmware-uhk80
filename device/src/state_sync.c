#include "state_sync.h"
#include "device.h"
#include "legacy/module.h"
#include "state_sync.h"
#include <stdint.h>
#include <zephyr/kernel.h>
#include "legacy/str_utils.h"
#include "legacy/debug.h"
#include "legacy/keymap.h"
#include "legacy/ledmap.h"
#include "legacy/config_manager.h"
#include "legacy/led_manager.h"
#include "keyboard/oled/widgets/widgets.h"
#include "device_state.h"
#include "messenger.h"
#include "usb/usb_compatibility.h"


#define THREAD_STACK_SIZE 1000
#define THREAD_PRIORITY 5
static K_THREAD_STACK_DEFINE(stack_area, THREAD_STACK_SIZE);
static struct k_thread thread_data;
static k_tid_t stateSyncThreadId = 0;

sync_generic_half_state_t SyncLeftHalfState;
sync_generic_half_state_t SyncRightHalfState;

static void receiveProperty(device_id_t src, state_sync_prop_id_t property, const uint8_t* data, uint8_t len);

#define DEFAULT_LAYER_PROP(NAME) [StateSyncPropertyId_##NAME] = { \
    .leftData = NULL, \
    .rightData = NULL, \
    .dongleData = NULL, \
    .len = 0, \
    .direction = SyncDirection_MasterToSlave, \
    .dirtyState = DirtyState_Clean, \
    .defaultDirty = DirtyState_Clean, \
    .name = #NAME \
}

#define SIMPLE_MASTER_TO_SLAVE(NAME, DATA, DEFAULT_DIRTY) [StateSyncPropertyId_##NAME] = { \
    .leftData = &DATA, \
    .rightData = &DATA, \
    .dongleData = &DATA, \
    .len = sizeof(DATA), \
    .direction = SyncDirection_MasterToSlave, \
    .dirtyState = DEFAULT_DIRTY, \
    .defaultDirty = DEFAULT_DIRTY, \
    .name = #NAME \
}

#define SIMPLE_DONGLE_TO_MASTER(NAME, DATA, DEFAULT_DIRTY) [StateSyncPropertyId_##NAME] = { \
    .leftData = &DATA, \
    .rightData = &DATA, \
    .dongleData = &DATA, \
    .len = sizeof(DATA), \
    .direction = SyncDirection_DongleToMaster, \
    .dirtyState = DEFAULT_DIRTY, \
    .defaultDirty = DEFAULT_DIRTY, \
    .name = #NAME \
}


#define CUSTOM_MASTER_TO_SLAVE(NAME, DEFAULT_DIRTY) [ StateSyncPropertyId_##NAME] = { \
    .leftData = NULL, \
    .rightData = NULL, \
    .dongleData = NULL, \
    .len = 0, \
    .direction = SyncDirection_MasterToSlave, \
    .dirtyState = DEFAULT_DIRTY, \
    .defaultDirty = DEFAULT_DIRTY, \
    .name = #NAME \
}

#define DUAL_SLAVE_TO_MASTER(NAME, LEFT, RIGHT, DEFAULT_DIRTY) [ StateSyncPropertyId_##NAME] = { \
    .leftData = &LEFT, \
    .rightData = &RIGHT, \
    .dongleData = NULL, \
    .len = sizeof(LEFT), \
    .direction = SyncDirection_SlaveToMaster, \
    .dirtyState = DEFAULT_DIRTY, \
    .defaultDirty = DEFAULT_DIRTY, \
    .name = #NAME \
}

#define EMPTY_MASTER_TO_SLAVE(NAME, DEFAULT_DIRTY) [ StateSyncPropertyId_##NAME] = { \
    .leftData = NULL, \
    .rightData = NULL, \
    .dongleData = NULL, \
    .len = 0, \
    .direction = SyncDirection_MasterToSlave, \
    .dirtyState = DEFAULT_DIRTY, \
    .defaultDirty = DEFAULT_DIRTY, \
    .name = #NAME \
}

static state_sync_prop_t stateSyncProps[StateSyncPropertyId_Count] = {
    DEFAULT_LAYER_PROP(LayerActionsLayer1),
    DEFAULT_LAYER_PROP(LayerActionsLayer2),
    DEFAULT_LAYER_PROP(LayerActionsLayer3),
    DEFAULT_LAYER_PROP(LayerActionsLayer4),
    DEFAULT_LAYER_PROP(LayerActionsLayer5),
    DEFAULT_LAYER_PROP(LayerActionsLayer6),
    DEFAULT_LAYER_PROP(LayerActionsLayer7),
    DEFAULT_LAYER_PROP(LayerActionsLayer8),
    DEFAULT_LAYER_PROP(LayerActionsLayer9),
    DEFAULT_LAYER_PROP(LayerActionsLayer10),
    DEFAULT_LAYER_PROP(LayerActionsLayer11),
    DEFAULT_LAYER_PROP(LayerActionsLayer12),
    DEFAULT_LAYER_PROP(LayerActionsClear),
    SIMPLE_MASTER_TO_SLAVE(ActiveLayer, ActiveLayer, DirtyState_Clean),
    CUSTOM_MASTER_TO_SLAVE(Backlight, DirtyState_Clean),
    SIMPLE_MASTER_TO_SLAVE(ActiveKeymap, CurrentKeymapIndex, DirtyState_Clean),
    SIMPLE_DONGLE_TO_MASTER(KeyboardLedsState, KeyboardLedsState, DirtyState_Clean),
    DUAL_SLAVE_TO_MASTER(Battery, SyncLeftHalfState.battery, SyncRightHalfState.battery, DirtyState_Clean),
    EMPTY_MASTER_TO_SLAVE(StateReset, DirtyState_Clean),
};

static void invalidateProperty(state_sync_prop_id_t propId) {
    STATE_SYNC_LOG("Invalidating property %s\n", stateSyncProps[propId].name);
    stateSyncProps[propId].dirtyState = DirtyState_NeedsUpdate;
    k_wakeup(stateSyncThreadId);
}

void StateSync_UpdateProperty(state_sync_prop_id_t propId, void* data) {
    state_sync_prop_t* prop = &stateSyncProps[propId];

    if (DEVICE_ID == DeviceId_Uhk80_Left && prop->leftData && data) {
        memcpy(prop->leftData, data, prop->len);
    }

    if (DEVICE_ID == DeviceId_Uhk80_Right && prop->rightData && data) {
        memcpy(prop->rightData, data, prop->len);
    }

    if (DEVICE_ID == DeviceId_Uhk_Dongle && prop->dongleData && data) {
        memcpy(prop->dongleData, data, prop->len);
    }

    switch (prop->direction) {
        case SyncDirection_LeftToRight:
        case SyncDirection_SlaveToMaster:
            if (DEVICE_ID == DeviceId_Uhk80_Left) {
                invalidateProperty(propId);
            }
            if (DEVICE_ID == DeviceId_Uhk80_Right) {
                receiveProperty(DEVICE_ID, propId, NULL, 0);
            }
            break;
        case SyncDirection_RightToLeft:
        case SyncDirection_MasterToSlave:
            if (DEVICE_ID == DeviceId_Uhk80_Left) {
                receiveProperty(DEVICE_ID, propId, NULL, 0);
            }
            if (DEVICE_ID == DeviceId_Uhk80_Right) {
                invalidateProperty(propId);
            }
            break;
        case SyncDirection_DongleToMaster:
            if (DEVICE_ID == DeviceId_Uhk80_Right) {
                receiveProperty(DEVICE_ID, propId, NULL, 0);
            }
            if (DEVICE_ID == DeviceId_Uhk_Dongle) {
                invalidateProperty(propId);
            }
            break;
    }
}

void receiveLayerActionsClear(layer_id_t layerId) {
    memset(&CurrentKeymap[layerId][SlotId_LeftKeyboardHalf][0], 0, sizeof(key_action_t)*MAX_KEY_COUNT_PER_MODULE);
}

void receiveLayerActions(sync_command_layer_t* buffer) {
    layer_id_t layerId = buffer->layerId;
    sync_command_action_t* actions = buffer->actions;
    for (uint8_t keyId = buffer->startOffset; keyId < buffer->startOffset + buffer->actionCount; keyId++) {
        key_action_t* action = &CurrentKeymap[layerId][SlotId_LeftKeyboardHalf][keyId];
        action->color = actions[keyId].color;
        action->colorOverridden = actions[keyId].colorOverriden;
        action->type = actions[keyId].type;
        action->keystroke.modifiers = actions[keyId].modifierPresent ? 0xff : 0;
        action->keystroke.scancode = actions[keyId].scancode;
    }
}

void receiveBacklight(sync_command_backlight_t* buffer) {
    Cfg.BacklightingMode = buffer->BacklightingMode;
    KeyBacklightBrightness = buffer->KeyBacklightBrightness;
    DisplayBrightness = buffer->DisplayBacklightBrightness;
    Cfg.LedMap_ConstantRGB = buffer->LedMap_ConstantRGB;
}

static void receiveProperty(device_id_t src, state_sync_prop_id_t propId, const uint8_t* data, uint8_t len) {
    state_sync_prop_t* prop = &stateSyncProps[propId];
    bool isLocalUpdate = src == DEVICE_ID;

    if (isLocalUpdate) {
        STATE_SYNC_LOG("Updating local property %s\n", prop->name);
    } else {
        STATE_SYNC_LOG("Received remote property %s from %s\n", prop->name, Utils_DeviceIdToString(src));
    }

    if (src == DeviceId_Uhk80_Left && prop->leftData && data) {
        memcpy(prop->leftData, data, prop->len);
    }

    if (src == DeviceId_Uhk80_Right && prop->rightData && data) {
        memcpy(prop->rightData, data, prop->len);
    }

    if (src == DeviceId_Uhk_Dongle && prop->dongleData && data) {
        memcpy(prop->dongleData, data, prop->len);
    }

    switch (propId) {
        case StateSyncPropertyId_LayerActionsClear:
            receiveLayerActionsClear(*((layer_id_t*)data));
            if (*((layer_id_t*)data) == ActiveLayer) {
                Ledmap_UpdateBacklightLeds();
            }
            break;
        case StateSyncPropertyId_LayerActionFirst ... StateSyncPropertyId_LayerActionLast:
            receiveLayerActions((sync_command_layer_t*)data);
            if ((propId - StateSyncPropertyId_LayerActionFirst) == ActiveLayer) {
                Ledmap_UpdateBacklightLeds();
            }
            break;
        case StateSyncPropertyId_ActiveLayer:
            Ledmap_UpdateBacklightLeds();
            break;
        case StateSyncPropertyId_Backlight:
            receiveBacklight((sync_command_backlight_t*)data);
            Ledmap_UpdateBacklightLeds();
            break;
        case StateSyncPropertyId_Battery:
            Widget_Refresh(&StatusWidget);
            {
                bool newRunningOnBattery = !SyncLeftHalfState.battery.powered || !SyncRightHalfState.battery.powered;
                if (RunningOnBattery != newRunningOnBattery) {
                    RunningOnBattery = newRunningOnBattery;
                    LedManager_FullUpdate();
                }
            }
            break;
        case StateSyncPropertyId_ActiveKeymap:
            // TODO
            break;
        case StateSyncPropertyId_KeyboardLedsState:
            Widget_Refresh(&StatusWidget);
            break;
        case StateSyncPropertyId_StateReset:
            if (!isLocalUpdate) {
                StateSync_Reset();
            }
            break;
        default:
            printk("Property %i ('%s') has no receive handler. If this is correct, please add a separate empty case...\n", propId, prop->name);
            break;
    }
}

void StateSync_ReceiveStateUpdate(device_id_t src, const uint8_t* data, uint8_t len) {
    ATTR_UNUSED message_id_t messageId = *(data++);
    ATTR_UNUSED state_sync_prop_id_t propId = *(data++);

    receiveProperty(src, propId, data, len - 2);
}

static void submitPreparedData(device_id_t dst, state_sync_prop_id_t propId, const uint8_t* data, uint8_t len) {
    STATE_SYNC_LOG("Sending %s data to %s\n", stateSyncProps[propId].name, Utils_DeviceIdToString(dst));
    Messenger_Send2(dst, MessageId_StateSync, propId, data, len);
}

static void prepareLayer(layer_id_t layerId, uint8_t offset, uint8_t count, sync_command_layer_t* buffer) {
    buffer->layerId = layerId;
    buffer->startOffset = offset;
    buffer->actionCount = count;
    for (uint8_t dstKeyId = 0; dstKeyId < count; dstKeyId++) {
        key_action_t* action = &CurrentKeymap[layerId][SlotId_LeftKeyboardHalf][offset+dstKeyId];
        buffer->actions[dstKeyId].type = action->type;
        buffer->actions[dstKeyId].scancode = action->keystroke.scancode;
        buffer->actions[dstKeyId].color = action->color;
        buffer->actions[dstKeyId].colorOverriden = action->colorOverridden;
        buffer->actions[dstKeyId].modifierPresent = action->keystroke.modifiers != 0;
    }
}

static void prepareAndSubmitLayer(device_id_t dst, state_sync_prop_id_t propId, layer_id_t layerId, uint8_t offset, uint8_t count) {
    sync_command_layer_t buffer;
    prepareLayer(layerId, offset, count, &buffer);
    submitPreparedData(dst, propId, (const uint8_t*)&buffer, sizeof(buffer));
}

static void prepareBacklight(sync_command_backlight_t* buffer) {
    buffer->BacklightingMode = Cfg.BacklightingMode;
    buffer->KeyBacklightBrightness = KeyBacklightBrightness;
    buffer->DisplayBacklightBrightness = DisplayBrightness;
    buffer->LedMap_ConstantRGB = Cfg.LedMap_ConstantRGB;
}

static void prepareData(device_id_t dst, const uint8_t* propDataPtr, state_sync_prop_id_t propId) {
    state_sync_prop_t* prop = &stateSyncProps[propId];
    const uint8_t* data = propDataPtr;

    uint8_t len = prop->len;

    STATE_SYNC_LOG("Preparing %s data for %s\n", prop->name, Utils_DeviceIdToString(dst));

    prop->dirtyState = DirtyState_Clean;

    switch (propId) {
        case StateSyncPropertyId_LayerActionFirst ... StateSyncPropertyId_LayerActionLast:
            {
                layer_id_t layerId = propId - StateSyncPropertyId_LayerActionFirst + LayerId_First;

                if (prop->dirtyState == DirtyState_NeedsClearing) {
                    submitPreparedData(dst, StateSyncPropertyId_LayerActionsClear, &layerId, sizeof(layerId));
                    return;
                } else {
                    prepareAndSubmitLayer(dst, propId, layerId, 0, MAX_KEY_COUNT_PER_UPDATE);
                    prepareAndSubmitLayer(dst, propId, layerId, MAX_KEY_COUNT_PER_UPDATE, MAX_KEY_COUNT_PER_MODULE - MAX_KEY_COUNT_PER_UPDATE);
                    return;
                }
            }
            break;
        case StateSyncPropertyId_Backlight:
            {
                sync_command_backlight_t buffer;
                prepareBacklight(&buffer);
                submitPreparedData(dst, propId, (const uint8_t*)&buffer, sizeof(buffer));
                return;
            }
            break;
        default:
            break;
    }

    submitPreparedData(dst, propId, data, len);
}

static void updateProperty(state_sync_prop_id_t propId) {
    device_id_t dst;
    const uint8_t* dataPtr;
    state_sync_prop_t* prop = &stateSyncProps[propId];


    switch (prop->direction) {
        case SyncDirection_LeftToRight:
        case SyncDirection_SlaveToMaster:
            dst = DeviceId_Uhk80_Right;
            dataPtr = prop->leftData;
            break;
        case SyncDirection_RightToLeft:
        case SyncDirection_MasterToSlave:
            dst = DeviceId_Uhk80_Left;
            dataPtr = prop->rightData;
            break;
        case SyncDirection_DongleToMaster:
            dst = DeviceId_Uhk80_Right;
            dataPtr = prop->dongleData;
            break;
        default:
            printk("Invalid sync direction for property %s\n", prop->name);
            return;
    }

    prepareData(dst, dataPtr, propId);
}

#define UPDATE_AND_RETURN_IF_DIRTY(propId) \
    if (stateSyncProps[propId].dirtyState != DirtyState_Clean) { \
        updateProperty(propId); \
        return false; \
    }

static bool handlePropertyUpdateMaster() {
    UPDATE_AND_RETURN_IF_DIRTY(StateSyncPropertyId_StateReset);

    if (KeyBacklightBrightness != 0 && Cfg.BacklightingMode != BacklightingMode_ConstantRGB) {
        state_sync_prop_id_t first = StateSyncPropertyId_LayerActionFirst;
        state_sync_prop_id_t last = StateSyncPropertyId_LayerActionLast;
        for (state_sync_prop_id_t propId = first; propId <= last; propId++) {
            UPDATE_AND_RETURN_IF_DIRTY(propId);
        }

        UPDATE_AND_RETURN_IF_DIRTY(StateSyncPropertyId_ActiveKeymap);
        UPDATE_AND_RETURN_IF_DIRTY(StateSyncPropertyId_ActiveLayer);
    }

    UPDATE_AND_RETURN_IF_DIRTY(StateSyncPropertyId_Backlight);

    return true;
}

static bool handlePropertyUpdateSlave() {
    UPDATE_AND_RETURN_IF_DIRTY(StateSyncPropertyId_Battery);

    return true;
}

static bool handlePropertyUpdateDongle() {
    UPDATE_AND_RETURN_IF_DIRTY(StateSyncPropertyId_KeyboardLedsState);

    return true;
}

static void updateLoop() {
    if (DEVICE_ID == DeviceId_Uhk80_Left) {
        while(true) {
            bool isConnected = DeviceState_IsDeviceConnected(DeviceId_Uhk80_Right);
            STATE_SYNC_LOG("Left update loop, connected: %i\n", isConnected);
            if (!isConnected || handlePropertyUpdateSlave()) {
                k_sleep(K_FOREVER);
            }
        }
    }

    if (DEVICE_ID == DeviceId_Uhk80_Right) {
        while(true) {
            bool isConnected = DeviceState_IsDeviceConnected(DeviceId_Uhk80_Left);
            STATE_SYNC_LOG("Right update loop, connected: %i\n", isConnected);
            if (!isConnected || handlePropertyUpdateMaster()) {
                k_sleep(K_FOREVER);
            }
        }
    }

    if (DEVICE_ID == DeviceId_Uhk_Dongle) {
        while(true) {
            bool isConnected = DeviceState_IsDeviceConnected(DeviceId_Uhk80_Right);
            STATE_SYNC_LOG("Dongle update loop, connected: %i\n", isConnected);
            if (!isConnected || handlePropertyUpdateDongle()) {
                k_sleep(K_FOREVER);
            }
        }
    }
}

void StateSync_UpdateLayer(layer_id_t layerId, bool fullUpdate) {
    state_sync_prop_id_t propId = StateSyncPropertyId_LayerActionFirst + layerId - LayerId_First;

    stateSyncProps[propId].dirtyState = fullUpdate ? DirtyState_NeedsUpdate : DirtyState_NeedsClearing;

    k_wakeup(stateSyncThreadId);
}

void StateSync_Init() {
    stateSyncThreadId = k_thread_create(
            &thread_data, stack_area,
            K_THREAD_STACK_SIZEOF(stack_area),
            updateLoop,
            NULL, NULL, NULL,
            THREAD_PRIORITY, 0, K_NO_WAIT
            );
    k_thread_name_set(&thread_data, "state_sync");
}

void StateSync_Reset() {
    if (DEVICE_ID == DeviceId_Uhk80_Right) {
        invalidateProperty(StateSyncPropertyId_ActiveLayer);
        invalidateProperty(StateSyncPropertyId_Backlight);
        invalidateProperty(StateSyncPropertyId_StateReset);
    }
    if (DEVICE_ID == DeviceId_Uhk80_Left) {
        invalidateProperty(StateSyncPropertyId_Battery);
    }
    if (DEVICE_ID == DeviceId_Uhk_Dongle) {
        invalidateProperty(StateSyncPropertyId_KeyboardLedsState);
    }
}




