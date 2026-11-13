#include "ble_hid.h"

#include <zephyr/sys/atomic.h>

/* Connection to the PC (we are the BLE peripheral there).
 * The link to the right half (we are central there) lives in ble_client.c,
 * so every callback here ignores connections that are not in the
 * peripheral role. */

/* Fixed passkey, only used with CONFIG_SAMPLE_BT_USE_AUTHENTICATION=y. */
#define BLE_PASSKEY 123456

/* Preferred connection parameters, units of 1.25 ms / 10 ms. */
#define CONN_INTERVAL_MIN 6  /* 7.5 ms */
#define CONN_INTERVAL_MAX 12 /* 15 ms */
#define CONN_LATENCY 0
#define CONN_TIMEOUT 400 /* 4 s */

#define BLE_INIT_DELAY_MS 500

/* A single empty report between two non-empty ones is treated as a glitch
 * (scan timing between halves). Release is sent after this many empties. */
#define RELEASE_EMPTY_FRAMES 2

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
                  BT_BYTES_LIST_LE16(CONFIG_BT_DEVICE_APPEARANCE)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* Written from the ble_pc thread, read from BT callbacks. */
static atomic_t ble_active;
static struct bt_conn *pc_conn;

static void adv_work_handler(struct k_work *work);
static K_WORK_DEFINE(adv_work, adv_work_handler);

static bool is_pc_conn(struct bt_conn *conn) {
  struct bt_conn_info info;

  return bt_conn_get_info(conn, &info) == 0 &&
         info.role == BT_CONN_ROLE_PERIPHERAL;
}

/* Advertising is (re)evaluated on the system workqueue so it never runs
 * inside BT callbacks, and the controller is free to accept it. */
static void adv_work_handler(struct k_work *work) {
  ARG_UNUSED(work);

  if (!atomic_get(&ble_active)) {
    int err = bt_le_adv_stop();
    if (err) {
      printk("Advertising stop failed (%d)\n", err);
    }
    return;
  }

  if (pc_conn) {
    return; /* already connected to the PC */
  }

  int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd,
                            ARRAY_SIZE(sd));
  if (err == -EALREADY) {
    return;
  }
  if (err) {
    printk("Advertising failed to start (%d)\n", err);
    return;
  }
  printk("Advertising to PC\n");
}

static void update_pc_interval(uint16_t interval) {
  uint16_t ms = interval * INTERVAL_MUL;

  pc_interval = ms > 0 ? ms : 1;
}

static void connected(struct bt_conn *conn, uint8_t err) {
  if (!is_pc_conn(conn)) {
    return;
  }

  if (err) {
    printk("PC connection failed, err 0x%02x %s\n", err,
           bt_hci_err_to_str(err));
    k_work_submit(&adv_work);
    return;
  }

  printk("PC connected %s\n", bt_conn_dst_str(conn));

  if (pc_conn) {
    bt_conn_unref(pc_conn);
  }
  pc_conn = bt_conn_ref(conn);

  struct bt_conn_info info;
  if (bt_conn_get_info(conn, &info) == 0) {
    update_pc_interval(info.le.interval);
  }

  /* Bonded PCs re-encrypt right away; new ones start pairing. */
  int ret = bt_conn_set_security(conn, IS_ENABLED(CONFIG_SAMPLE_BT_USE_AUTHENTICATION)
                                           ? BT_SECURITY_L3
                                           : BT_SECURITY_L2);
  if (ret) {
    printk("Failed to set security (%d)\n", ret);
  }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
  if (conn != pc_conn) {
    return;
  }

  printk("PC disconnected, reason 0x%02x %s\n", reason,
         bt_hci_err_to_str(reason));

  bt_conn_unref(pc_conn);
  pc_conn = NULL;
  pc_interval = USB_FREQ;
}

/* Called once a connection object is freed: the right moment to advertise
 * again, regardless of who closed the link or why. */
static void recycled(void) { k_work_submit(&adv_work); }

static void le_param_updated(struct bt_conn *conn, uint16_t interval,
                             uint16_t latency, uint16_t timeout) {
  if (conn != pc_conn) {
    return;
  }

  printk("PC conn params: interval %u (x1.25ms) latency %u timeout %u\n",
         interval, latency, timeout);
  update_pc_interval(interval);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
  if (conn != pc_conn) {
    return;
  }

  if (err) {
    printk("Security failed: %s level %u err %s(%d)\n", bt_conn_dst_str(conn),
           level, bt_security_err_to_str(err), err);
    return;
  }

  printk("Security changed: %s level %u\n", bt_conn_dst_str(conn), level);

  /* Ask for a fast interval once the link is trusted; hosts tend to ignore
   * requests made before pairing/discovery finishes. */
  int ret = bt_conn_le_param_update(
      conn, BT_LE_CONN_PARAM(CONN_INTERVAL_MIN, CONN_INTERVAL_MAX,
                             CONN_LATENCY, CONN_TIMEOUT));
  if (ret) {
    printk("Conn param update request failed (%d)\n", ret);
  }
}

BT_CONN_CB_DEFINE(pc_conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .recycled = recycled,
    .le_param_updated = le_param_updated,
    .security_changed = security_changed,
};

/* ---- Pairing ----
 * Default (CONFIG_SAMPLE_BT_USE_AUTHENTICATION=n): "Just Works" pairing, no
 * prompt on the PC, link is encrypted and bonded.
 *
 * With CONFIG_SAMPLE_BT_USE_AUTHENTICATION=y the PC has to type BLE_PASSKEY.
 * GNOME's Bluetooth settings do not implement that prompt (RequestPasskey)
 * and just drop the connection; it only works from e.g. bluetoothctl. */

#if defined(CONFIG_SAMPLE_BT_USE_AUTHENTICATION)
static uint32_t auth_app_passkey(struct bt_conn *conn) {
  ARG_UNUSED(conn);
  return BLE_PASSKEY;
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey) {
  printk("Passkey for %s: %06u\n", bt_conn_dst_str(conn), passkey);
}

static void auth_cancel(struct bt_conn *conn) {
  printk("Pairing cancelled: %s\n", bt_conn_dst_str(conn));
}

static struct bt_conn_auth_cb auth_cb = {
    .passkey_display = auth_passkey_display,
    .app_passkey = auth_app_passkey,
    .cancel = auth_cancel,
};
#endif /* CONFIG_SAMPLE_BT_USE_AUTHENTICATION */

static void pairing_complete(struct bt_conn *conn, bool bonded) {
  printk("Pairing complete: %s bonded=%d\n", bt_conn_dst_str(conn), bonded);
}

/* A PC only starts a fresh pairing when it no longer has our keys (bonded
 * PCs just re-encrypt). If that pairing fails, the bond we still hold for it
 * is stale and would block every later attempt, so drop it. Done from the
 * workqueue, not from inside the SMP callback. */
static bt_addr_le_t stale_peer;

static void unpair_work_handler(struct k_work *work) {
  ARG_UNUSED(work);

  int err = bt_unpair(BT_ID_DEFAULT, &stale_peer);
  if (err && err != -ENOENT) {
    printk("Removing stale bond failed (%d)\n", err);
  }
}

static K_WORK_DEFINE(unpair_work, unpair_work_handler);

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason) {
  printk("Pairing failed: %s reason %s(%d)\n", bt_conn_dst_str(conn),
         bt_security_err_to_str(reason), reason);

  if (conn != pc_conn) {
    return;
  }

  bt_addr_le_copy(&stale_peer, bt_conn_get_dst(conn));
  k_work_submit(&unpair_work);
}

static struct bt_conn_auth_info_cb auth_info_cb = {
    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed,
};

/* ---- USB / BLE switching ---- */

static void ble_set_active(bool active) {
  if (atomic_set(&ble_active, active) == active) {
    return;
  }

  printk("BLE to PC %s\n", active ? "enabled" : "disabled");

  if (!active && pc_conn) {
    int err = bt_conn_disconnect(pc_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    if (err) {
      printk("Disconnect failed (%d)\n", err);
    }
  }

  k_work_submit(&adv_work);
}

static void initialize_ble_pc(void) {
  /* bt_enable() and settings_load() (bonds) are done in ble_client.c. */
  k_sem_take(&ble_init_sem, K_FOREVER);
  k_sleep(K_MSEC(BLE_INIT_DELAY_MS));

  int err;

#if defined(CONFIG_SAMPLE_BT_USE_AUTHENTICATION)
  err = bt_conn_auth_cb_register(&auth_cb);
  if (err) {
    printk("Auth callback register failed (%d)\n", err);
  }
#endif

  err = bt_conn_auth_info_cb_register(&auth_info_cb);
  if (err) {
    printk("Auth info callback register failed (%d)\n", err);
  }

  ble_set_active(!usb_ready);
}

/* ---- Report sending ---- */

static uint8_t last_sent[KB_REPORT_COUNT];
static uint8_t empty_frames;
static struct bt_conn *last_conn;

static bool report_is_empty(const uint8_t report[KB_REPORT_COUNT]) {
  for (size_t i = 0; i < KB_REPORT_COUNT; i++) {
    if (report[i] != 0) {
      return false;
    }
  }
  return true;
}

static void send_pc_report(void) {
  uint8_t report[KB_REPORT_COUNT] = {0};

  /* Always drain the queue so stale reports don't pile up. */
  k_msgq_get(&msgq_kb_report, report, K_NO_WAIT);

  struct bt_conn *conn = pc_conn;

  if (!hog_is_subscribed(conn)) {
    memset(last_sent, 0, sizeof(last_sent));
    empty_frames = 0;
    last_conn = NULL;
    return;
  }

  /* New subscription: host assumes everything is released. */
  if (conn != last_conn) {
    memset(last_sent, 0, sizeof(last_sent));
    empty_frames = 0;
    last_conn = conn;
  }

  if (report_is_empty(report)) {
    if (report_is_empty(last_sent)) {
      return;
    }
    if (++empty_frames < RELEASE_EMPTY_FRAMES) {
      return;
    }
  } else {
    empty_frames = 0;
  }

  if (memcmp(report, last_sent, sizeof(report)) == 0) {
    return; /* nothing changed */
  }

  int err = hog_send_report(conn, report);
  if (err) {
    /* Keep last_sent unchanged so the change is retried next cycle. */
    return;
  }

  memcpy(last_sent, report, sizeof(last_sent));
}

void ble_pc_task(void *p1, void *p2, void *p3) {
  initialize_ble_pc();

  while (1) {
    bool active = atomic_get(&ble_active);
    uint32_t events =
        k_event_wait(&ble_state_event, USB_READY_BIT | USB_UNREADY_BIT, true,
                     active ? K_MSEC(pc_interval) : K_FOREVER);

    if (events & USB_READY_BIT) {
      ble_set_active(false);
    } else if (events & USB_UNREADY_BIT) {
      ble_set_active(true);
    }

    if (atomic_get(&ble_active)) {
      send_pc_report();
    }
  }
}
