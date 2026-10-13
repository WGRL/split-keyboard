#include "ble_client.h"
#include "ble_matrix_input.h"

#define MAX_KEYS 6
#define KEY_LIST_START 2

static struct bt_uuid_128 service_uuid =
    BT_UUID_INIT_128(0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
                     0x78, 0x56, 0x34, 0x12, 0x00, 0x01);

static struct bt_uuid_128 char_uuid =
    BT_UUID_INIT_128(0xf1, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
                     0x78, 0x56, 0x34, 0x12, 0x00, 0x01);

static struct bt_conn *default_conn;
static struct bt_gatt_subscribe_params subscribe_params;

static struct bt_gatt_discover_params discover_params;
static uint16_t value_handle;
static uint16_t ccc_handle;

static struct key_code tx_value_master;
struct pos button;

static void enqueue_key_codes(struct key_code *key_vals, size_t size_to_push) {
  for (size_t i = 0; i < size_to_push; i++) {
    while (k_msgq_put(&msgq_key_codes, &key_vals[i], K_NO_WAIT) != RET_FINE) {

      /* message queue is full: purge old data & try again */
      k_msgq_purge(&msgq_key_codes);
    }
  }
}

static void enqueue_keyboard_report(void) {
  uint8_t report[KB_REPORT_COUNT];

  memset(report, 0, sizeof(report));

  create_report(report);

  // printk("report: ");
  // for (int i = 0; i < KB_REPORT_COUNT; i++)
  //   printk("%d ", report[i]);
  // printk("\n");

  if (report[KEY_LIST_START] != 0) {
    while (k_msgq_put(&msgq_kb_report, report, K_NO_WAIT) != 0) {

      /* message queue is full: purge old data & try again */
      k_msgq_purge(&msgq_kb_report);
    }
  }
}

static void process_master_cycle(void) {
  struct key_code key_vals[HID_LIM];

  size_t size_to_push = get_all_queue_l(key_vals);

  enqueue_key_codes(key_vals, size_to_push);

  enqueue_keyboard_report();
}

void ble_task_master(void *p1, void *p2, void *p3) {
  ble_setup_master();

  while (1) {
    process_master_cycle();

    k_sleep(K_MSEC(pc_interval));
  }
}

static uint8_t notify_func(struct bt_conn *conn,
                           struct bt_gatt_subscribe_params *params,
                           const void *data, uint16_t length) {
  if (!data) {
    printk("Subscription terminated\n");
    return BT_GATT_ITER_STOP;
  }

  if (length >= 1) {
    tx_value_master = *((struct key_code *)data);
  }
  while (k_msgq_put(&msgq_key_codes, &tx_value_master, K_NO_WAIT) != 0) {
    /* message queue is full: purge old data & try again */
    k_msgq_purge(&msgq_key_codes);
  }

  memset(&tx_value_master, 0, sizeof(tx_value_master));

  return BT_GATT_ITER_CONTINUE;
}

static void subscribe_to_characteristic(void) {
  int err;

  subscribe_params.notify = notify_func;
  subscribe_params.value = BT_GATT_CCC_NOTIFY;
  subscribe_params.value_handle = value_handle;
  subscribe_params.ccc_handle = ccc_handle;

  err = bt_gatt_subscribe(default_conn, &subscribe_params);
  if (err) {
    printk("Subscribe failed (%d)\n", err);
  }
}
static bool data_cb(struct bt_data *data, void *user_data) {
  char *name = user_data;

  switch (data->type) {

  case BT_DATA_NAME_COMPLETE:
  case BT_DATA_NAME_SHORTENED:

    memcpy(name, data->data, data->data_len);
    name[data->data_len] = 0;

    return false;

  default:
    return true;
  }
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad) {
  char name[32] = {0};

  bt_data_parse(ad, data_cb, name);

  if (strcmp(name, DEVICE_NAME) != 0) {
    printk("NOT!!!! Found target: %s\n", name);

    return;
  }

  // printk("!!!! Found target: %s\n", name);

  bt_le_scan_stop();

  int err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
                              BT_LE_CONN_PARAM_DEFAULT, &default_conn);

  if (err) {
    printk("Create conn failed (%d)\n", err);
    bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
  }
}

int ble_setup_master() {
  int err;
  printk("BLE Central Start\n");

  err = bt_enable(NULL);
  if (err) {
    printk("Bluetooth init failed (%d)\n", err);
    return -1;
  }
  printk("Bluetooth ready\n");

  settings_load();
  k_sem_give(&ble_init_sem);

  err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
  if (err) {
    printk("Scan failed (%d)\n", err);
    return -1;
  }

  // printk("Scanning...\n");
  return 0;
}

static uint8_t discover_ccc_cb(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               struct bt_gatt_discover_params *params) {
  if (!attr) {
    printk("CCC not found\n");
    memset(params, 0, sizeof(*params));
    return BT_GATT_ITER_STOP;
  }

  ccc_handle = attr->handle;

  // printk("CCC handle = 0x%04x\n", ccc_handle);

  memset(params, 0, sizeof(*params));

  subscribe_to_characteristic();

  return BT_GATT_ITER_STOP;
}

static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param) {
  printk("Requested interval %d-%d latency %d timeout %d\n",
         param->interval_min, param->interval_max, param->latency,
         param->timeout);

  return true; // accept everything
}

static uint8_t discover_char_cb(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                struct bt_gatt_discover_params *params) {
  if (!attr) {
    printk("Characteristic not found\n");
    memset(params, 0, sizeof(*params));
    return BT_GATT_ITER_STOP;
  }

  const struct bt_gatt_chrc *chrc = attr->user_data;

  value_handle = chrc->value_handle;

  // printk("Value handle = 0x%04x\n", value_handle);

  discover_params.uuid = BT_UUID_GATT_CCC;
  discover_params.start_handle = value_handle + 1;
  discover_params.end_handle = 0xffff;
  discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
  discover_params.func = discover_ccc_cb;

  int err = bt_gatt_discover(conn, &discover_params);

  if (err) {
    printk("CCC discover failed (%d)\n", err);
  }

  return BT_GATT_ITER_STOP;
}

static void connected(struct bt_conn *conn, uint8_t err) {
  struct bt_conn_info info;
  if (bt_conn_get_info(conn, &info) == 0 && info.role != BT_CONN_ROLE_CENTRAL) {
    return; /* to polaczenie peryferyjne do PC, nie nasze */
  }

  if (err) {
    printk("Connection failed (%u)\n", err);
    return;
  }

  // printk("Connected\n");

  if (default_conn == NULL)
    default_conn = bt_conn_ref(conn);

  discover_params.uuid = &char_uuid.uuid;
  discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
  discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
  discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
  discover_params.func = discover_char_cb;

  err = bt_gatt_discover(conn, &discover_params);

  if (err) {
    printk("Discover failed (%d)\n", err);
  }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
  if (conn != default_conn) {
    return; /* nie nasze polaczenie */
  }
  printk("Disconnected reason=%u\n", reason);

  if (default_conn) {
    bt_conn_unref(default_conn);
    default_conn = NULL;
  }
  bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {.connected = connected,
                                     .disconnected = disconnected,
                                     .le_param_req = le_param_req};
