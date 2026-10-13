#include "hog.h"

#define SERVICE_REPORT_IDX 5

enum {
  HIDS_REMOTE_WAKE = BIT(0),
  HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

struct hids_info {
  uint16_t version; /* version number of base USB HID Specification */
  uint8_t code;     /* country HID Device hardware is localized for. */
  uint8_t flags;
} __packed;

struct hids_report {
  uint8_t id;   /* report id */
  uint8_t type; /* report type */
} __packed;

static struct hids_info info = {
    .version = 0x0000,
    .code = 0x00,
    .flags = HIDS_NORMALLY_CONNECTABLE,
};

enum {
  HIDS_INPUT = 0x01,
  HIDS_OUTPUT = 0x02,
  HIDS_FEATURE = 0x03,
};

static struct hids_report input = {
    .id = 0x01,
    .type = HIDS_INPUT,
};

static uint8_t notify_ready = 0;
static uint8_t ctrl_point;

static const uint8_t report_map[] = {0x05, 0x01, // Usage Page (Generic Desktop)
                                     0x09, 0x06, // Usage (Keyboard)
                                     0xA1, 0x01, // Collection (Application)

                                     0x85, 0x01, // Report ID = 1

                                     /* ===== MODIFIERS ===== */

                                     0x05, 0x07, // Usage Page (Keyboard/Keypad)
                                     0x19, 0xE0, // Usage Minimum (Left Control)
                                     0x29, 0xE7, // Usage Maximum (Right GUI)

                                     0x15, 0x00, // Logical Minimum (0)
                                     0x25, 0x01, // Logical Maximum (1)

                                     0x75, 0x01, // Report Size (1 bit)
                                     0x95, 0x08, // Report Count (8)

                                     0x81, 0x02, // Input (Data,Var,Abs)

                                     /* ===== RESERVED BYTE ===== */

                                     0x75, 0x08, // Report Size (8)
                                     0x95, 0x01, // Report Count (1)

                                     0x81, 0x03, // Input (Constant)

                                     /* ===== LEDS ===== */

                                     0x05, 0x08, // Usage Page (LEDs)

                                     0x19, 0x01, // Num Lock
                                     0x29, 0x05, // Kana

                                     0x75, 0x01, // 1 bit
                                     0x95, 0x05, // 5 LEDów

                                     0x91, 0x02, // Output (Data,Var,Abs)

                                     /* padding */

                                     0x75, 0x03, 0x95, 0x01, 0x91, 0x03,

                                     /* ===== KEY ARRAY ===== */

                                     0x05, 0x07, // Keyboard page

                                     0x19, 0x00, 0x29, 0x65,

                                     0x15, 0x00, 0x25, 0x65,

                                     0x75, 0x08, // 8 bitów
                                     0x95, 0x06, // 6 klawiszy

                                     0x81, 0x00, // Input (Data,Array)

                                     0xC0};

static uint8_t protocol_mode = 0x01;

static ssize_t read_protocol_mode(struct bt_conn *conn,
                                  const struct bt_gatt_attr *attr, void *buf,
                                  uint16_t len, uint16_t offset) {
  return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
                           sizeof(protocol_mode));
}

static ssize_t write_protocol_mode(struct bt_conn *conn,
                                   const struct bt_gatt_attr *attr,
                                   const void *buf, uint16_t len,
                                   uint16_t offset, uint8_t flags) {
  if (offset != 0) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
  }

  if (len != 1) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  }

  uint8_t mode = *(uint8_t *)buf;

  if (mode != 0x00 && mode != 0x01) { // dodaj makro
    return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
  }

  protocol_mode = mode;

  return len;
}

static ssize_t read_info(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         void *buf, uint16_t len, uint16_t offset) {
  return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
                           sizeof(struct hids_info));
}

static ssize_t read_report_map(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, void *buf,
                               uint16_t len, uint16_t offset) {
  printk("Reading report map, size: %zu\n", sizeof(report_map));
  return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map,
                           sizeof(report_map));
}

static ssize_t read_report(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr, void *buf,
                           uint16_t len, uint16_t offset) {
  return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
                           sizeof(struct hids_report));
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
  notify_ready = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static ssize_t read_input_report(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr, void *buf,
                                 uint16_t len, uint16_t offset) {
  printk("Reading input report from attr: %p\n", attr);
  return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

static ssize_t write_ctrl_point(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                const void *buf, uint16_t len, uint16_t offset,
                                uint8_t flags) {
  uint8_t *value = attr->user_data;

  if (offset + len > sizeof(ctrl_point)) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
  }

  memcpy(value + offset, buf, len);

  return len;
}

#if CONFIG_SAMPLE_BT_USE_AUTHENTICATION
/* Require encryption using authenticated link-key. */
#define SAMPLE_BT_PERM_READ BT_GATT_PERM_READ_AUTHEN
#define SAMPLE_BT_PERM_WRITE BT_GATT_PERM_WRITE_AUTHEN
#else
/* Require encryption. */
#define SAMPLE_BT_PERM_READ BT_GATT_PERM_READ_ENCRYPT
#define SAMPLE_BT_PERM_WRITE BT_GATT_PERM_WRITE_ENCRYPT
#endif

BT_GATT_SERVICE_DEFINE(
    hog_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, read_info, NULL, &info),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, read_report_map, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           SAMPLE_BT_PERM_READ, read_input_report, NULL, NULL),
    BT_GATT_CCC(input_ccc_changed, SAMPLE_BT_PERM_READ | SAMPLE_BT_PERM_WRITE),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ, read_report,
                       NULL, &input),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
                           BT_GATT_CHRC_WRITE_WITHOUT_RESP, BT_GATT_PERM_WRITE,
                           NULL, write_ctrl_point, &ctrl_point), );

void hog_init(void) {}

static bool debounce_flag = 0;

void send_report() {
  uint8_t report[KB_REPORT_COUNT];
  memset(&report, 0, sizeof(report));
  k_msgq_get(&msgq_kb_report, report, K_NO_WAIT);

  if (!notify_ready)
    return;

  if (debounce_flag == 0 && report[KB_KEY_CODE1] == 0)
    debounce_flag = 1;
  else
    debounce_flag = 0;

  if (!debounce_flag) {
    // printk("report: ");
    // for (int i = 0; i < 8; i++)
    //   printk("%d ", report[i]);
    // printk("\n");
    bt_gatt_notify(NULL, &hog_svc.attrs[SERVICE_REPORT_IDX], report,
                   sizeof(report));
  }
}
