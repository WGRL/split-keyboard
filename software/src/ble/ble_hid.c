#include "ble_hid.h"

#define MIN_INTERVAL_REQ 12 // 15 ms
#define MAX_INTERVAL_REQ 12 // 15 ms
#define LATENCY_BLE 0
#define TIMEOUT_BLE 400 // 4s

#define BLE_DELAY 300
#define BLE_INIT_DELAY 500
#define RECONNECT_DELAY 1

static bool ble_active = false;
static struct bt_conn *current_conn;

static void le_param_updated(struct bt_conn *conn, uint16_t interval,
                             uint16_t latency, uint16_t timeout) {
  pc_interval = interval * INTERVAL_MUL;
}

static const uint16_t appearance = CONFIG_BT_DEVICE_APPEARANCE;
// static const struct bt_data ad[] = {
//     BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
//     BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
//                   BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
//
//     BT_DATA(BT_DATA_GAP_APPEARANCE, (const uint8_t *)&appearance,
//             sizeof(appearance)),
// };

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static bool is_peripheral_connection(struct bt_conn *conn) {
  struct bt_conn_info info;

  if (bt_conn_get_info(conn, &info) == 0 &&
      info.role != BT_CONN_ROLE_PERIPHERAL) {
    return false; /* to polaczenie centralne do drugiej polowki, nie nasze */
  }

  return true;
}

static bool handle_connection_error(struct bt_conn *conn, uint8_t err) {
  if (err) {
    printk("Failed to connect to %s, err 0x%02x %s\n", bt_conn_dst_str(conn),
           err, bt_hci_err_to_str(err));
    return true;
  }

  return false;
}

// static void update_connection_interval(void) {
//   struct bt_conn_info info;
//
//   k_sleep(K_MSEC(100));
//
//   if (bt_conn_get_info(current_conn, &info) == RET_FINE) {
//     if (info.type == BT_CONN_TYPE_LE) {
//       pc_interval = info.le.interval;
//     }
//   }
// }

static void configure_connection(struct bt_conn *conn) {
  printk("Connected %s\n", bt_conn_dst_str(conn));

  current_conn = bt_conn_ref(conn);

  bt_conn_le_param_update(
      conn,
      BT_LE_CONN_PARAM(MIN_INTERVAL_REQ, // min 7.5ms
                       MAX_INTERVAL_REQ, // max interval = 15 ms
                       LATENCY_BLE,      // latency
                       TIMEOUT_BLE       // timeout = 4 s
                       ));

  // update_connection_interval();
}

static void connected(struct bt_conn *conn, uint8_t err) {
  // if (!is_peripheral_connection(conn)) {
  //   return;
  // }

  if (handle_connection_error(conn, err)) {
    return;
  }

  configure_connection(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
  if (conn != current_conn) {
    return;
  }

  printk("Disconnected from %s, reason 0x%02x %s\n", bt_conn_dst_str(conn),
         reason, bt_hci_err_to_str(reason));

  bt_conn_unref(current_conn);
  current_conn = NULL;

  if (ble_active && reason != BT_HCI_ERR_REMOTE_USER_TERM_CONN) {
    k_sleep(K_SECONDS(RECONNECT_DELAY));
    bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd,
                    ARRAY_SIZE(sd));
  }
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
  if (!err) {
    printk("Security changed: %s level %u\n", bt_conn_dst_str(conn), level);
  } else {
    printk("Security failed: %s level %u err %s(%d)\n", bt_conn_dst_str(conn),
           level, bt_security_err_to_str(err), err);
  }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {.connected = connected,
                                     .disconnected = disconnected,
                                     .security_changed = security_changed,
                                     .le_param_updated = le_param_updated};

static void auth_confirm(struct bt_conn *conn, unsigned int passkey) {
  printk("Pairing confirmed\n");
  bt_conn_auth_passkey_confirm(conn);
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey) {
  bt_conn_auth_passkey_confirm(conn);

  printk("Passkey for %s: %06u\n", bt_conn_dst_str(conn), passkey);
}

static void auth_cancel(struct bt_conn *conn) {
  printk("Pairing cancelled: %s\n", bt_conn_dst_str(conn));
}

static struct bt_conn_auth_cb auth_cb_display = {
    .passkey_confirm = auth_confirm,
    .passkey_display = auth_passkey_display,
    .passkey_entry = NULL,
    .cancel = auth_cancel,
};

static void ble_stop(void) {
  int err = bt_le_adv_stop();
  if (err) {
    printk("Advertising stop failed (err %d)\n", err);
  } else {
    printk("Advertising stopped\n");
  }

  if (current_conn) {
    err = bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    if (err) {
      printk("Disconnect failed (err %d)\n", err);
    }
  }
}

static void ble_start(void) {
  int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd,
                            ARRAY_SIZE(sd));
  if (err) {
    printk("Advertising failed to start (err %d)\n", err);
    return;
  }
  printk("Advertising successfully started\n");
}

static void initialize_ble_pc(void) {
  k_sem_take(&ble_init_sem, K_FOREVER);
  k_sleep(K_MSEC(BLE_INIT_DELAY));

  if (IS_ENABLED(CONFIG_SAMPLE_BT_USE_AUTHENTICATION)) {
    bt_conn_auth_cb_register(&auth_cb_display);
    int err = bt_passkey_set(123456);
    if (err) {
      printk("Failed to set fixed passkey (%d)\n", err);
    }
    printk("Bluetooth authentication callbacks registered.\n");
  }

  if (!usb_ready) {
    ble_start();
    ble_active = true;
  }
}

static void handle_ble_state_events(uint32_t events) {
  if (events & USB_READY_BIT) {
    if (ble_active) {
      ble_active = false;
      ble_stop();
    }
  } else if (events & USB_UNREADY_BIT) {
    if (!ble_active) {
      ble_active = true;
      ble_start();
    }
  }
}

void ble_pc_task(void *p1, void *p2, void *p3) {
  initialize_ble_pc();

  while (1) {
    uint32_t events =
        k_event_wait(&ble_state_event, USB_READY_BIT | USB_UNREADY_BIT, true,
                     ble_active ? K_MSEC(pc_interval) : K_FOREVER);

    handle_ble_state_events(events);

    if (ble_active) {
      send_report();
    }
  }
}
