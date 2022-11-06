#include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>


#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <soc.h>
#include <assert.h>
#include <zephyr/spinlock.h>

#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/bluetooth/services/bas.h>
#include <bluetooth/services/hids.h>
#include <zephyr/bluetooth/services/dis.h>
#include <dk_buttons_and_leds.h>

static const uint8_t hid_keyboard_report_desc[] = HID_KEYBOARD_REPORT_DESC();
static const uint8_t hid_mouse_report_desc[] = HID_MOUSE_REPORT_DESC(2);

static enum usb_dc_status_code usb_status;

#define MOUSE_BTN_REPORT_POS	0
#define MOUSE_X_REPORT_POS	1
#define MOUSE_Y_REPORT_POS	2

#define MOUSE_BTN_LEFT		BIT(0)
#define MOUSE_BTN_RIGHT		BIT(1)
#define MOUSE_BTN_MIDDLE	BIT(2)

uint8_t mouse_report[4] = { 0x00 };
uint8_t keyboard_report[8] = { 0x00 };
const struct device *hid_keyboard_dev;
const struct device *hid_mouse_dev;

static void status_cb(enum usb_dc_status_code status, const uint8_t *param) {
	printk("USB status code change: %i\n", status);
	usb_status = status;
	if (status == USB_DC_CONFIGURED) {
		hid_int_ep_write(hid_mouse_dev, mouse_report, sizeof(mouse_report), NULL);
		hid_int_ep_write(hid_keyboard_dev, keyboard_report, sizeof(keyboard_report), NULL);
	}
}

/*
	if (IS_ENABLED(CONFIG_USB_DEVICE_REMOTE_WAKEUP)) {
		if (usb_status == USB_DC_SUSPEND) {
			usb_wakeup_request();
			return;
		}
	}
*/

void int_in_ready_mouse(const struct device *dev) {
	uint32_t buttons = dk_get_buttons();
	mouse_report[MOUSE_X_REPORT_POS] = buttons & DK_BTN1_MSK ? 5 : 0;
	mouse_report[MOUSE_Y_REPORT_POS] = buttons & DK_BTN2_MSK ? 5 : 0;
	mouse_report[MOUSE_BTN_REPORT_POS] = buttons & DK_BTN3_MSK ? MOUSE_BTN_LEFT : 0;
	hid_int_ep_write(hid_mouse_dev, mouse_report, sizeof(mouse_report), NULL);
}

void int_in_ready_keyboard(const struct device *dev) {
	uint32_t buttons = dk_get_buttons();
	keyboard_report[2] = buttons & DK_BTN4_MSK ? HID_KEY_A : 0;
	hid_int_ep_write(hid_keyboard_dev, keyboard_report, sizeof(keyboard_report), NULL);
}

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define BASE_USB_HID_SPEC_VERSION   0x0101

#define OUTPUT_REPORT_MAX_LEN            1
#define OUTPUT_REPORT_BIT_MASK_CAPS_LOCK 0x02
#define INPUT_REP_KEYS_REF_ID            1
#define OUTPUT_REP_KEYS_REF_ID           1
#define MODIFIER_KEY_POS                 0
#define SHIFT_KEY_CODE                   0x02
#define SCAN_CODE_POS                    2
#define KEYS_MAX_LEN                    (INPUT_REPORT_KEYS_MAX_LEN - SCAN_CODE_POS)

#define ADV_LED_BLINK_INTERVAL  1000

#define ADV_STATUS_LED DK_LED1
#define CON_STATUS_LED DK_LED2
#define LED_CAPS_LOCK  DK_LED3
#define KEY_TEXT_MASK  DK_BTN1_MSK

/* Key used to accept or reject passkey value */
#define KEY_PAIRING_ACCEPT DK_BTN1_MSK
#define KEY_PAIRING_REJECT DK_BTN2_MSK

/* HIDs queue elements. */
#define HIDS_QUEUE_SIZE 10

/* ********************* */
/* Buttons configuration */

/* Note: The configuration below is the same as BOOT mode configuration
 * This simplifies the code as the BOOT mode is the same as REPORT mode.
 * Changing this configuration would require separate implementation of
 * BOOT mode report generation.
 */
#define KEY_CTRL_CODE_MIN 224 /* Control key codes - required 8 of them */
#define KEY_CTRL_CODE_MAX 231 /* Control key codes - required 8 of them */
#define KEY_CODE_MIN      0   /* Normal key codes */
#define KEY_CODE_MAX      101 /* Normal key codes */
#define KEY_PRESS_MAX     6   /* Maximum number of non-control keys
			       * pressed simultaneously
			       */

/* Number of bytes in key report
 *
 * 1B - control keys
 * 1B - reserved
 * rest - non-control keys
 */
#define INPUT_REPORT_KEYS_MAX_LEN (1 + 1 + KEY_PRESS_MAX)

/* Current report map construction requires exactly 8 buttons */
BUILD_ASSERT((KEY_CTRL_CODE_MAX - KEY_CTRL_CODE_MIN) + 1 == 8);

/* OUT report internal indexes.
 *
 * This is a position in internal report table and is not related to
 * report ID.
 */
enum {
	OUTPUT_REP_KEYS_IDX = 0
};

/* INPUT report internal indexes.
 *
 * This is a position in internal report table and is not related to
 * report ID.
 */
enum {
	INPUT_REP_KEYS_IDX = 0
};

/* HIDS instance. */
BT_HIDS_DEF(hids_obj,
	    OUTPUT_REPORT_MAX_LEN,
	    INPUT_REPORT_KEYS_MAX_LEN);

static volatile bool is_adv;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
					  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static struct conn_mode {
	struct bt_conn *conn;
	bool in_boot_mode;
} conn_mode;

/* Current report status
 */
static struct keyboard_state {
	uint8_t ctrl_keys_state; /* Current keys state */
	uint8_t keys_state[KEY_PRESS_MAX];
} hid_keyboard_state;

static struct k_work pairing_work;
struct pairing_data_mitm {
	struct bt_conn *conn;
	unsigned int passkey;
};

K_MSGQ_DEFINE(mitm_queue, sizeof(struct pairing_data_mitm), CONFIG_BT_HIDS_MAX_CLIENT_COUNT, 4);

static void advertising_start(void) {
	struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM(
						BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME,
						BT_GAP_ADV_FAST_INT_MIN_2,
						BT_GAP_ADV_FAST_INT_MAX_2,
						NULL);

	int err = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), sd,
			      ARRAY_SIZE(sd));
	if (err) {
		if (err == -EALREADY) {
			printk("Advertising continued\n");
		} else {
			printk("Advertising failed to start (err %d)\n", err);
		}

		return;
	}

	is_adv = true;
	printk("Advertising successfully started\n");
}

static void pairing_process(struct k_work *work) {
	struct pairing_data_mitm pairing_data;

	char addr[BT_ADDR_LE_STR_LEN];

	int err = k_msgq_peek(&mitm_queue, &pairing_data);
	if (err) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(pairing_data.conn), addr, sizeof(addr));

	printk("Passkey for %s: %06u\n", addr, pairing_data.passkey);
	printk("Press Button 1 to confirm, Button 2 to reject.\n");
}

static void connected(struct bt_conn *conn, uint8_t err) {
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s (%u)\n", addr, err);
		return;
	}

	printk("Connected %s\n", addr);
	dk_set_led_on(CON_STATUS_LED);

	err = bt_hids_connected(&hids_obj, conn);

	if (err) {
		printk("Failed to notify HID service about connection\n");
		return;
	}

	if (!conn_mode.conn) {
		conn_mode.conn = conn;
		conn_mode.in_boot_mode = false;
		advertising_start();
	}

	is_adv = false;
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
	int err;
	bool is_any_dev_connected = false;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected from %s (reason %u)\n", addr, reason);

	err = bt_hids_disconnected(&hids_obj, conn);

	if (err) {
		printk("Failed to notify HID service about disconnection\n");
	}

	if (conn_mode.conn == conn) {
		conn_mode.conn = NULL;
	} else if (conn_mode.conn) {
		is_any_dev_connected = true;
	}

	if (!is_any_dev_connected) {
		dk_set_led_off(CON_STATUS_LED);
	}

	advertising_start();
}

static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err) {
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		printk("Security changed: %s level %u\n", addr, level);
	} else {
		printk("Security failed: %s level %u err %d\n", addr, level, err);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};

static void caps_lock_handler(const struct bt_hids_rep *rep) {
	uint8_t report_val = ((*rep->data) & OUTPUT_REPORT_BIT_MASK_CAPS_LOCK) ? 1 : 0;
	dk_set_led(LED_CAPS_LOCK, report_val);
}

static void hids_outp_rep_handler(struct bt_hids_rep *rep, struct bt_conn *conn, bool write) {
	char addr[BT_ADDR_LE_STR_LEN];

	if (!write) {
		printk("Output report read\n");
		return;
	};

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Output report has been received %s\n", addr);
	caps_lock_handler(rep);
}

static void hids_boot_kb_outp_rep_handler(struct bt_hids_rep *rep, struct bt_conn *conn, bool write) {
	char addr[BT_ADDR_LE_STR_LEN];

	if (!write) {
		printk("Output report read\n");
		return;
	};

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Boot Keyboard Output report has been received %s\n", addr);
	caps_lock_handler(rep);
}

static void hids_pm_evt_handler(enum bt_hids_pm_evt evt, struct bt_conn *conn) {
	char addr[BT_ADDR_LE_STR_LEN];

	if (conn_mode.conn != conn) {
		printk("Cannot find connection handle when processing PM");
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	switch (evt) {
	case BT_HIDS_PM_EVT_BOOT_MODE_ENTERED:
		printk("Boot mode entered %s\n", addr);
		conn_mode.in_boot_mode = true;
		break;
	case BT_HIDS_PM_EVT_REPORT_MODE_ENTERED:
		printk("Report mode entered %s\n", addr);
		conn_mode.in_boot_mode = false;
		break;
	default:
		break;
	}
}

static void hid_init(void) {
	int err;
	struct bt_hids_init_param    hids_init_obj = { 0 };
	struct bt_hids_inp_rep       *hids_inp_rep;
	struct bt_hids_outp_feat_rep *hids_outp_rep;

	static const uint8_t report_map[] = {
		0x05, 0x01,       /* Usage Page (Generic Desktop) */
		0x09, 0x06,       /* Usage (Keyboard) */
		0xA1, 0x01,       /* Collection (Application) */

		/* Keys */
#if INPUT_REP_KEYS_REF_ID
		0x85, INPUT_REP_KEYS_REF_ID,
#endif
		0x05, 0x07,       /* Usage Page (Key Codes) */
		0x19, 0xe0,       /* Usage Minimum (224) */
		0x29, 0xe7,       /* Usage Maximum (231) */
		0x15, 0x00,       /* Logical Minimum (0) */
		0x25, 0x01,       /* Logical Maximum (1) */
		0x75, 0x01,       /* Report Size (1) */
		0x95, 0x08,       /* Report Count (8) */
		0x81, 0x02,       /* Input (Data, Variable, Absolute) */

		0x95, 0x01,       /* Report Count (1) */
		0x75, 0x08,       /* Report Size (8) */
		0x81, 0x01,       /* Input (Constant) reserved byte(1) */

		0x95, 0x06,       /* Report Count (6) */
		0x75, 0x08,       /* Report Size (8) */
		0x15, 0x00,       /* Logical Minimum (0) */
		0x25, 0x65,       /* Logical Maximum (101) */
		0x05, 0x07,       /* Usage Page (Key codes) */
		0x19, 0x00,       /* Usage Minimum (0) */
		0x29, 0x65,       /* Usage Maximum (101) */
		0x81, 0x00,       /* Input (Data, Array) Key array(6 bytes) */

		/* LED */
#if OUTPUT_REP_KEYS_REF_ID
		0x85, OUTPUT_REP_KEYS_REF_ID,
#endif
		0x95, 0x05,       /* Report Count (5) */
		0x75, 0x01,       /* Report Size (1) */
		0x05, 0x08,       /* Usage Page (Page# for LEDs) */
		0x19, 0x01,       /* Usage Minimum (1) */
		0x29, 0x05,       /* Usage Maximum (5) */
		0x91, 0x02,       /* Output (Data, Variable, Absolute), */
				  /* Led report */
		0x95, 0x01,       /* Report Count (1) */
		0x75, 0x03,       /* Report Size (3) */
		0x91, 0x01,       /* Output (Data, Variable, Absolute), */
				  /* Led report padding */

		0xC0              /* End Collection (Application) */
	};

	hids_init_obj.rep_map.data = report_map;
	hids_init_obj.rep_map.size = sizeof(report_map);

	hids_init_obj.info.bcd_hid = BASE_USB_HID_SPEC_VERSION;
	hids_init_obj.info.b_country_code = 0x00;
	hids_init_obj.info.flags = (BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE);

	hids_inp_rep = &hids_init_obj.inp_rep_group_init.reports[INPUT_REP_KEYS_IDX];
	hids_inp_rep->size = INPUT_REPORT_KEYS_MAX_LEN;
	hids_inp_rep->id = INPUT_REP_KEYS_REF_ID;
	hids_init_obj.inp_rep_group_init.cnt++;

	hids_outp_rep = &hids_init_obj.outp_rep_group_init.reports[OUTPUT_REP_KEYS_IDX];
	hids_outp_rep->size = OUTPUT_REPORT_MAX_LEN;
	hids_outp_rep->id = OUTPUT_REP_KEYS_REF_ID;
	hids_outp_rep->handler = hids_outp_rep_handler;
	hids_init_obj.outp_rep_group_init.cnt++;

	hids_init_obj.is_kb = true;
	hids_init_obj.boot_kb_outp_rep_handler = hids_boot_kb_outp_rep_handler;
	hids_init_obj.pm_evt_handler = hids_pm_evt_handler;

	err = bt_hids_init(&hids_obj, &hids_init_obj);
	__ASSERT(err == 0, "HIDS initialization failed\n");
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey) {
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey) {
	struct pairing_data_mitm pairing_data;

	pairing_data.conn    = bt_conn_ref(conn);
	pairing_data.passkey = passkey;

	int err = k_msgq_put(&mitm_queue, &pairing_data, K_NO_WAIT);
	if (err) {
		printk("Pairing queue is full. Purge previous data.\n");
	}

	/* In the case of multiple pairing requests, trigger
	 * pairing confirmation which needed user interaction only
	 * once to avoid display information about all devices at
	 * the same time. Passkey confirmation for next devices will
	 * be proccess from queue after handling the earlier ones.
	 */
	if (k_msgq_num_used_get(&mitm_queue) == 1) {
		k_work_submit(&pairing_work);
	}
}

static void auth_cancel(struct bt_conn *conn) {
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Pairing cancelled: %s\n", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded) {
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason) {
	char addr[BT_ADDR_LE_STR_LEN];
	struct pairing_data_mitm pairing_data;

	if (k_msgq_peek(&mitm_queue, &pairing_data) != 0) {
		return;
	}

	if (pairing_data.conn == conn) {
		bt_conn_unref(pairing_data.conn);
		k_msgq_get(&mitm_queue, &pairing_data, K_NO_WAIT);
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Pairing failed conn: %s, reason %d\n", addr, reason);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = auth_passkey_display,
	.passkey_confirm = auth_passkey_confirm,
	.cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};

static int key_report_con_send(const struct keyboard_state *state, bool boot_mode, struct bt_conn *conn, bool down) {
	uint8_t chr = down ? HID_KEY_A : 0;
	uint8_t  data[INPUT_REPORT_KEYS_MAX_LEN] = {state->ctrl_keys_state, 0, chr, 0, 0, 0, 0, 0};

	int err = 0;
	if (boot_mode) {
		err = bt_hids_boot_kb_inp_rep_send(&hids_obj, conn, data, sizeof(data), NULL);
	} else {
		err = bt_hids_inp_rep_send(&hids_obj, conn, INPUT_REP_KEYS_IDX, data, sizeof(data), NULL);
	}
	return err;
}

static int key_report_send(bool down) {
	if (conn_mode.conn) {
		int err = key_report_con_send(&hid_keyboard_state, conn_mode.in_boot_mode, conn_mode.conn, down);
		if (err) {
			printk("Key report send error: %d\n", err);
			return err;
		}
	}
	return 0;
}

static void num_comp_reply(bool accept) {
	struct pairing_data_mitm pairing_data;
	struct bt_conn *conn;

	if (k_msgq_get(&mitm_queue, &pairing_data, K_NO_WAIT) != 0) {
		return;
	}

	conn = pairing_data.conn;

	if (accept) {
		bt_conn_auth_passkey_confirm(conn);
		printk("Numeric Match, conn %p\n", conn);
	} else {
		bt_conn_auth_cancel(conn);
		printk("Numeric Reject, conn %p\n", conn);
	}

	bt_conn_unref(pairing_data.conn);

	if (k_msgq_num_used_get(&mitm_queue)) {
		k_work_submit(&pairing_work);
	}
}

static void button_changed(uint32_t button_state, uint32_t has_changed) {
	static bool pairing_button_pressed;

	uint32_t buttons = button_state & has_changed;

	if (k_msgq_num_used_get(&mitm_queue)) {
		if (buttons & KEY_PAIRING_ACCEPT) {
			pairing_button_pressed = true;
			num_comp_reply(true);
			return;
		}

		if (buttons & KEY_PAIRING_REJECT) {
			pairing_button_pressed = true;
			num_comp_reply(false);
			return;
		}
	}

	/* Do not take any action if the pairing button is released. */
	if (pairing_button_pressed &&
	    (has_changed & (KEY_PAIRING_ACCEPT | KEY_PAIRING_REJECT))) {
		pairing_button_pressed = false;
		return;
	}

	if (has_changed & KEY_TEXT_MASK) {
		key_report_send((button_state & KEY_TEXT_MASK) != 0);
	}
}

static void configure_gpio(void) {
	dk_buttons_init(button_changed);
	dk_leds_init();
}

static void bas_notify(void) {
	uint8_t battery_level = bt_bas_get_battery_level();

	battery_level--;

	if (!battery_level) {
		battery_level = 100U;
	}

	bt_bas_set_battery_level(battery_level);
}

void main(void) {
	// USB

	struct hid_ops hidops_mouse = {
		.int_in_ready = int_in_ready_mouse,
	};

	struct hid_ops hidops_keyboard = {
		.int_in_ready = int_in_ready_keyboard,
	};

	hid_keyboard_dev = device_get_binding("HID_0");
	hid_mouse_dev = device_get_binding("HID_1");

	configure_gpio();

	usb_hid_register_device(hid_keyboard_dev, hid_keyboard_report_desc, sizeof(hid_keyboard_report_desc), &hidops_keyboard);
	usb_hid_register_device(hid_mouse_dev, hid_mouse_report_desc, sizeof(hid_mouse_report_desc), &hidops_mouse);

	usb_hid_init(hid_keyboard_dev);
	usb_hid_init(hid_mouse_dev);

	usb_enable(status_cb);

	// Bluetooth

	int blink_status = 0;

	printk("Starting Bluetooth Peripheral HIDS keyboard example\n");


	bt_conn_auth_cb_register(&conn_auth_callbacks);
	bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
	bt_enable(NULL);

	printk("Bluetooth initialized\n");

	hid_init();

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	advertising_start();

	k_work_init(&pairing_work, pairing_process);

	for (;;) {
		if (is_adv) {
			dk_set_led(ADV_STATUS_LED, (++blink_status) % 2);
		} else {
			dk_set_led_off(ADV_STATUS_LED);
		}
/*
		if (conn_mode.conn&& bt_conn_auth_passkey_confirm(conn_mode.conn) == 0) {
			uint32_t buttons = dk_get_buttons();
			hid_keyboard_state.keys_state[1] = buttons & DK_BTN4_MSK ? HID_KEY_A : 0;
			printk(".");
			int err = key_report_con_send(&hid_keyboard_state, conn_mode.in_boot_mode, conn_mode.conn);
			if (err) {
				printk("Key report send error: %d\n", err);
				return err;
			}
		}
*/
		k_sleep(K_MSEC(ADV_LED_BLINK_INTERVAL));
		/* Battery level simulation */
		bas_notify();
	}
}
