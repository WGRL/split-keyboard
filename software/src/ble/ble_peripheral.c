#include "ble_peripheral.h"
#include "ble_matrix_input.h"

#define GATT_SERVICE_ID 2

#define MIN_INTERVAL_REQ 10 //
#define MAX_INTERVAL_REQ 10 //
#define LATENCY_BLE 0
#define TIMEOUT_BLE 400 // 4s

#define RECONNECT_DELAY 1

static struct key_code tx_value_slave;
static uint16_t interval_ms;
static struct bt_conn *current_conn;

static struct bt_uuid_128 service_uuid =
    BT_UUID_INIT_128(0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
                     0x78, 0x56, 0x34, 0x12, 0x00, 0x01);

static struct bt_uuid_128 char_uuid =
    BT_UUID_INIT_128(0xf1, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
                     0x78, 0x56, 0x34, 0x12, 0x00, 0x01);

static ssize_t read_value(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          void *buf, uint16_t len, uint16_t offset) {
  return bt_gatt_attr_read(conn, attr, buf, len, offset, &tx_value_slave,
                           sizeof(tx_value_slave));
}

static void ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value) {
  ARG_UNUSED(attr);
  printk("CCCD changed: %u\n", value);
}

BT_GATT_SERVICE_DEFINE(
    my_service, BT_GATT_PRIMARY_SERVICE(&service_uuid),

    BT_GATT_CHARACTERISTIC(&char_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, read_value, NULL,
                           &tx_value_slave),

    BT_GATT_CCC(ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static struct bt_conn *default_conn;

static void connected(struct bt_conn *conn, uint8_t err) {
  if (err) {
    printk("Connection failed (err %u)\n", err);
    return;
  }
  bt_le_adv_stop();
  printk("Connected\n");
  default_conn = bt_conn_ref(conn);
  printk("conn=%p\n", conn);
  current_conn = bt_conn_ref(conn);

  bt_conn_le_param_update(conn,
                          BT_LE_CONN_PARAM(MIN_INTERVAL_REQ, MAX_INTERVAL_REQ,
                                           LATENCY_BLE, TIMEOUT_BLE));
}

static void ble_adv() {
  int8_t err =
      bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), NULL, 0);

  if (err) {
    printk("Advertising error\n");
    return;
  }
  struct bt_conn_info info;

  if (bt_conn_get_info(current_conn, &info) == RET_FINE) {
    if (info.type == BT_CONN_TYPE_LE) {
      interval_ms = info.le.interval;
    }
  }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {

  printk("Disconnected (reason %u)\n", reason);
  if (current_conn) {
    bt_conn_unref(current_conn);
    current_conn = NULL;
  }
  if (reason != BT_HCI_ERR_REMOTE_USER_TERM_CONN) {
    // delay to prevent instant reconnect
    // k_sleep(K_SECONDS(RECONNECT_DELAY));
    ble_adv();
  }
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval,
                             uint16_t latency, uint16_t timeout) {
  interval_ms = interval * INTERVAL_MUL;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {.connected = connected,
                                     .disconnected = disconnected,
                                     .le_param_updated = le_param_updated};

int ble_setup_slave() {
  int err;
  // pos_reset(&tx_value_slave);

  memset(&tx_value_slave, 0, sizeof(tx_value_slave));

  err = bt_enable(NULL);
  if (err) {
    printk("BLE init error\n");
    return -1;
  }

  printk("BLE ready\n");
  ble_adv();

  return 0;
}

void ble_task_slave(void *p1, void *p2, void *p3) {
  ble_setup_slave();
  const struct bt_gatt_attr *attr = &my_service.attrs[GATT_SERVICE_ID];

  while (1) {
    if (default_conn == NULL ||
        !bt_gatt_is_subscribed(default_conn, attr, BT_GATT_CCC_NOTIFY)) {
      k_sleep(K_MSEC(50));
      continue;
    }

    size_t size_to_push;
    struct key_code key_vals[HID_LIM];
    size_to_push = get_all_queue_l(key_vals);
    for (size_t i = 0; i < size_to_push; i++) {
      bt_gatt_notify(default_conn, attr, &key_vals[i], sizeof(key_vals[i]));
    }

    k_sleep(K_MSEC(interval_ms));
  }
}
