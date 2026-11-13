#include "hog.h"

#include <errno.h>
#include <string.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/byteorder.h>

#define REPORT_ID_KEYBOARD 0x01

enum {
  HIDS_REMOTE_WAKE = BIT(0),
  HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

enum {
  HIDS_INPUT = 0x01,
  HIDS_OUTPUT = 0x02,
  HIDS_FEATURE = 0x03,
};

struct hids_info {
  uint16_t version; /* bcdHID */
  uint8_t code;     /* country code */
  uint8_t flags;
} __packed;

struct hids_report {
  uint8_t id;
  uint8_t type;
} __packed;

static const struct hids_info info = {
    .version = sys_cpu_to_le16(0x0111), /* HID 1.11 */
    .code = 0x00,
    .flags = HIDS_NORMALLY_CONNECTABLE | HIDS_REMOTE_WAKE,
};

static const struct hids_report input_ref = {
    .id = REPORT_ID_KEYBOARD,
    .type = HIDS_INPUT,
};

static const struct hids_report output_ref = {
    .id = REPORT_ID_KEYBOARD,
    .type = HIDS_OUTPUT,
};

static uint8_t input_report[KB_REPORT_COUNT];
static uint8_t led_report;
static uint8_t ctrl_point;

static const uint8_t report_map[] = {
    0x05, 0x01, /* Usage Page (Generic Desktop) */
    0x09, 0x06, /* Usage (Keyboard) */
    0xA1, 0x01, /* Collection (Application) */
    0x85, REPORT_ID_KEYBOARD, /* Report ID */

    /* Modifiers: 8 x 1 bit */
    0x05, 0x07, /* Usage Page (Keyboard/Keypad) */
    0x19, 0xE0, /* Usage Minimum (Left Control) */
    0x29, 0xE7, /* Usage Maximum (Right GUI) */
    0x15, 0x00, /* Logical Minimum (0) */
    0x25, 0x01, /* Logical Maximum (1) */
    0x75, 0x01, /* Report Size (1) */
    0x95, 0x08, /* Report Count (8) */
    0x81, 0x02, /* Input (Data,Var,Abs) */

    /* Reserved byte */
    0x75, 0x08, /* Report Size (8) */
    0x95, 0x01, /* Report Count (1) */
    0x81, 0x03, /* Input (Const) */

    /* LEDs: 5 x 1 bit + 3 bit padding */
    0x05, 0x08, /* Usage Page (LEDs) */
    0x19, 0x01, /* Usage Minimum (Num Lock) */
    0x29, 0x05, /* Usage Maximum (Kana) */
    0x75, 0x01, /* Report Size (1) */
    0x95, 0x05, /* Report Count (5) */
    0x91, 0x02, /* Output (Data,Var,Abs) */
    0x75, 0x03, /* Report Size (3) */
    0x95, 0x01, /* Report Count (1) */
    0x91, 0x03, /* Output (Const) */

    /* Key array: 6 x 8 bit */
    0x05, 0x07, /* Usage Page (Keyboard/Keypad) */
    0x19, 0x00, /* Usage Minimum (0) */
    0x29, 0x65, /* Usage Maximum (Keyboard Application) */
    0x15, 0x00, /* Logical Minimum (0) */
    0x25, 0x65, /* Logical Maximum (101) */
    0x75, 0x08, /* Report Size (8) */
    0x95, 0x06, /* Report Count (6) */
    0x81, 0x00, /* Input (Data,Array) */

    0xC0, /* End Collection */
};

#if CONFIG_SAMPLE_BT_USE_AUTHENTICATION
/* Require an authenticated (MITM, passkey) encrypted link. */
#define HOG_PERM_READ BT_GATT_PERM_READ_AUTHEN
#define HOG_PERM_WRITE BT_GATT_PERM_WRITE_AUTHEN
#else
/* Require an encrypted link (Just Works). */
#define HOG_PERM_READ BT_GATT_PERM_READ_ENCRYPT
#define HOG_PERM_WRITE BT_GATT_PERM_WRITE_ENCRYPT
#endif

static ssize_t read_info(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         void *buf, uint16_t len, uint16_t offset) {
  return bt_gatt_attr_read(conn, attr, buf, len, offset, &info, sizeof(info));
}

static ssize_t read_report_map(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, void *buf,
                               uint16_t len, uint16_t offset) {
  return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map,
                           sizeof(report_map));
}

static ssize_t read_report_ref(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, void *buf,
                               uint16_t len, uint16_t offset) {
  return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
                           sizeof(struct hids_report));
}

static ssize_t read_input_report(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr, void *buf,
                                 uint16_t len, uint16_t offset) {
  return bt_gatt_attr_read(conn, attr, buf, len, offset, input_report,
                           sizeof(input_report));
}

static ssize_t read_output_report(struct bt_conn *conn,
                                  const struct bt_gatt_attr *attr, void *buf,
                                  uint16_t len, uint16_t offset) {
  return bt_gatt_attr_read(conn, attr, buf, len, offset, &led_report,
                           sizeof(led_report));
}

static ssize_t write_output_report(struct bt_conn *conn,
                                   const struct bt_gatt_attr *attr,
                                   const void *buf, uint16_t len,
                                   uint16_t offset, uint8_t flags) {
  if (offset != 0) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
  }
  if (len != sizeof(led_report)) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  }

  led_report = *(const uint8_t *)buf;
  return len;
}

static ssize_t write_ctrl_point(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                const void *buf, uint16_t len, uint16_t offset,
                                uint8_t flags) {
  if (offset != 0 || len != sizeof(ctrl_point)) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  }

  /* 0x00 = suspend, 0x01 = exit suspend; nothing to do for now. */
  ctrl_point = *(const uint8_t *)buf;
  return len;
}

/* Attribute indices:
 *  0 service
 *  1/2  HID info
 *  3/4  report map
 *  5/6  input report, 7 CCC, 8 report reference
 *  9/10 output report (LEDs), 11 report reference
 *  12/13 control point
 */
#define HOG_INPUT_REPORT_ATTR_IDX 6

BT_GATT_SERVICE_DEFINE(
    hog_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ, HOG_PERM_READ,
                           read_info, NULL, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,
                           HOG_PERM_READ, read_report_map, NULL, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           HOG_PERM_READ, read_input_report, NULL, NULL),
    BT_GATT_CCC(NULL, HOG_PERM_READ | HOG_PERM_WRITE),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
                       read_report_ref, NULL, (void *)&input_ref),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
                               BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           HOG_PERM_READ | HOG_PERM_WRITE, read_output_report,
                           write_output_report, NULL),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
                       read_report_ref, NULL, (void *)&output_ref),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
                           BT_GATT_CHRC_WRITE_WITHOUT_RESP, HOG_PERM_WRITE,
                           NULL, write_ctrl_point, NULL), );

bool hog_is_subscribed(struct bt_conn *conn) {
  return conn != NULL &&
         bt_gatt_is_subscribed(conn, &hog_svc.attrs[HOG_INPUT_REPORT_ATTR_IDX],
                               BT_GATT_CCC_NOTIFY);
}

int hog_send_report(struct bt_conn *conn,
                    const uint8_t report[KB_REPORT_COUNT]) {
  if (!hog_is_subscribed(conn)) {
    return -EAGAIN;
  }

  memcpy(input_report, report, sizeof(input_report));

  return bt_gatt_notify(conn, &hog_svc.attrs[HOG_INPUT_REPORT_ATTR_IDX],
                        input_report, sizeof(input_report));
}

uint8_t hog_get_leds(void) { return led_report; }
