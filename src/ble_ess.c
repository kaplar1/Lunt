/*
 Bluetooth LE peripheral exposing the standard Environmental Sensing
 Service (ESS, 0x181A) with Temperature, Humidity and Pressure.
 Any generic BLE app (nRF Connect for Mobile) decodes these.

 The MCUmgr SMP service for firmware updates is added automatically by
 CONFIG_NCS_SAMPLE_MCUMGR_BT_OTA_DFU; we only need to advertise.

 - Static GATT service definition, read callbacks, notifications (CCC)
 - Advertising data vs scan response
 - Restarting advertising after a disconnect (the "recycled" callback)
 - Protecting data shared between threads with a spinlock
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/bluettoth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "ble_ess.h"

LOG_MODULE_REGISTER(ble_ess, CONFIG_APP_LOG_LEVEL);

/*
 ESS value formats (Bluetooth GATT Specification Supplement):
 Temperature 0x2A6E: sint16, 0.01 degC
 Humidity    0x2A6F: uint16, 0.01 %
 Pressure    0x2A6D: uint32, 0.1 Pa
 Stored little-endian, ready to send.
*/

static struct {
	int16_t temp_le;
	uint16_t hum_le;
	uint32_t press_le;
} values;

static struct k_spinlock values_lock;

static ssize_t read_value(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			  uint16_t len, uint16_t offset)
{
	/* user_data points at the field to return; copy it under the lock
	 * because the main thread may be updating it right now. */
	uint8_t tmp[4];
	size_t size = (attr->user_data == &values.press_le) ? 4 : 2;
	k_spinlock_key_t key = k_spin_lock(&values_lock);

	memcpy(tmp, attr->user_data, size);
	k_spin_unlock(&values_lock, key);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, tmp, size);
}

static void ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	LOG_INF("Notifications %s", value == BT_GATT_CCC_NOTIFY ? "enabled" : "disabled");
}

/* Attribute table. Index:
 * 0 service | 1 temp decl | 2 temp value | 3 temp CCC
 * 4 hum decl | 5 hum value | 6 hum CCC | 7 press decl | 8 press value | 9 press CCC */
BT_GATT_SERVICE_DEFINE(
	ess_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),

	BT_GATT_CHARACTERISTIC(BT_UUID_TEMPERATURE, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_value, NULL, &values.temp_le),
	BT_GATT_CCC(ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_HUMIDITY, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_value, NULL, &values.hum_le),
	BT_GATT_CCC(ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_PRESSURE, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_value, NULL, &values.press_le),
	BT_GATT_CCC(ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

#define ATTR_TEMP  (&ess_svc.attrs[2])
#define ATTR_HUM   (&ess_svc.attrs[5])
#define ATTR_PRESS (&ess_svc.attrs[8])

/* Advertising packet: flags + "I offer ESS". Name goes in the scan response. */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_ESS_VAL)),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static int advertise(void)
{
	int err;

	/* About 100-150 ms between advertising events: quick to find. */
	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err == -EALREADY) {
		return 0;
	}
	if (err) {
		LOG_ERR("Advertising failed to start (%d)", err);
	} else {
		LOG_INF("Advertising as \"%s\"", CONFIG_BT_DEVICE_NAME);
	}
	return err;
}

/* Advertising cannot restart inside the disconnected callback, because the
 * connection object is not free yet. "recycled" fires once it is. We hop to
 * the system workqueue rather than doing work inside the BT callback. */
static void adv_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	(void)advertise();
}
static K_WORK_DEFINE(adv_work, adv_work_handler);

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	if (err) {
		LOG_WRN("Connection to %s failed (0x%02x)", addr, err);
		return;
	}
	LOG_INF("Connected: %s", addr);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	LOG_INF("Disconnected (reason 0x%02x)", reason);
}

static void recycled(void)
{
	k_work_submit(&adv_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled,
};

int ble_ess_init(void)
{
	int err = bt_enable(NULL);

	if (err) {
		LOG_ERR("Bluetooth init failed (%d)", err);
		return err;
	}

	return advertise();
}

void ble_ess_update(const struct env_sample *s)
{
	int16_t t = sys_cpu_to_le16((int16_t)s->temp_centi_c);
	uint16_t h = sys_cpu_to_le16((uint16_t)s->hum_centi_pct);
	uint32_t p = sys_cpu_to_le32((uint32_t)s->press_deci_pa);
	k_spinlock_key_t key = k_spin_lock(&values_lock);

	values.temp_le = t;
	values.hum_le = h;
	values.press_le = p;
	k_spin_unlock(&values_lock, key);

	/* NULL conn = notify every subscribed connection. Returns an error
	 * when nobody is connected or subscribed; that is normal. */
	(void)bt_gatt_notify(NULL, ATTR_TEMP, &t, sizeof(t));
	(void)bt_gatt_notify(NULL, ATTR_HUM, &h, sizeof(h));
	(void)bt_gatt_notify(NULL, ATTR_PRESS, &p, sizeof(p));
}
