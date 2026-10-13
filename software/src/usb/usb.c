#include "usb.h"
#include "zephyr/usb/usbd_msg.h"

#define HID_REGISTERED_ERR -1

static const uint8_t hid_report_desc[] = HID_KEYBOARD_REPORT_DESC();

// USBD_DEVICE_DEFINE(my_usb_device, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
//                    0x2fe3, 0x0001);

UDC_STATIC_BUF_DEFINE(report, KB_REPORT_COUNT);
static bool kb_ready;
static uint32_t kb_duration;

struct usbd_context *sample_usbd;

const struct device *hid_dev;
static bool hid_registered = 0;

static bool debounce_flag = 0;

static void input_cb(struct input_event *evt, void *user_data) {
  // useless dla mnie
}

static void kb_iface_ready(const struct device *dev, const bool ready) {
  printk("HID device %s interface is %s", dev->name,
         ready ? "ready" : "not ready");
  kb_ready = ready;
}

static int kb_get_report(const struct device *dev, const uint8_t type,
                         const uint8_t id, const uint16_t len,
                         uint8_t *const buf) {
  printk("Get Report not implemented, Type %u ID %u", type, id);
  // pc chce raport
  return 0;
}

static int kb_set_report(const struct device *dev, const uint8_t type,
                         const uint8_t id, const uint16_t len,
                         const uint8_t *const buf) {
  if (type != HID_REPORT_TYPE_OUTPUT) {
    printk("Unsupported report type");
    return -ENOTSUP;
  }

  // PC wysyla raport do klawy
  // input do klawy np. do leda capsa

  return 0;
}

// Idle duration is stored but not used to calculate idle reports.
static void kb_set_idle(const struct device *dev, const uint8_t id,
                        const uint32_t duration) {
  printk("Set Idle %u to %u", id, duration);
  kb_duration = duration;
}

static uint32_t kb_get_idle(const struct device *dev, const uint8_t id) {
  printk("Get Idle %u to %u", id, kb_duration);
  return kb_duration;
}

static void kb_set_protocol(const struct device *dev, const uint8_t proto) {
  printk("Protocol changed to %s",
         proto == 0U ? "Boot Protocol" : "Report Protocol");
}

static void kb_output_report(const struct device *dev, const uint16_t len,
                             const uint8_t *const buf) {
  kb_set_report(dev, HID_REPORT_TYPE_OUTPUT, 0U, len, buf);
}

struct hid_device_ops kb_ops = {
    .iface_ready = kb_iface_ready,
    .get_report = kb_get_report,
    .set_report = kb_set_report,
    .set_idle = kb_set_idle,
    .get_idle = kb_get_idle,
    .set_protocol = kb_set_protocol,
    .output_report = kb_output_report,
};

static void msg_cb(struct usbd_context *const usbd_ctx,
                   const struct usbd_msg *const msg) {
  printk("USBD message: %s", usbd_msg_type_string(msg->type));

  switch (msg->type) {
  case USBD_MSG_CONFIGURATION:
    usb_ready_changed(true);
    printk(" - USB ready");
    k_sem_give(&usb_sem);
    break;

  case USBD_MSG_VBUS_READY:
    printk(" - VBUS ready");
    if (usbd_enable(usbd_ctx)) {
      printk(" - Failed to enable USB");
    }
    break;

  case USBD_MSG_VBUS_REMOVED:
    printk(" - VBUS removed");
    usb_ready_changed(false);

    if (usbd_disable(usbd_ctx)) {
      printk(" - Failed to disable USB");
    }
    break;

  case USBD_MSG_SUSPEND:
    usb_ready_changed(false);
    printk(" - USB suspended");
    break;

  case USBD_MSG_RESUME:
    usb_ready_changed(true);
    printk(" - USB resumed");
    break;

  default:
    printk(" - USB Unhandled event");
    break;
  }

  printk("\n");
}

static int register_hid_device(void) {
  int ret;

  hid_dev = DEVICE_DT_GET_ONE(zephyr_hid_device);

  printk("HID device name: %s\n", hid_dev->name);

  if (!device_is_ready(hid_dev)) {
    printk("HID Device is not ready");
    return -EIO;
  }

  ret = hid_device_register(hid_dev, hid_report_desc, sizeof(hid_report_desc),
                            &kb_ops);
  if (ret != 0) {
    printk("Failed to register HID Device, %d", ret);
    return ret;
  }

  hid_registered = 1;

  return 0;
}

static int configure_hid_polling(void) {
  int ret;

  if (IS_ENABLED(CONFIG_USBD_HID_SET_POLLING_PERIOD)) {
    ret = hid_device_set_in_polling(hid_dev, 1000);
    if (ret) {
      printk("Failed to set IN report polling period, %d", ret);
      return ret;
    }

    ret = hid_device_set_out_polling(hid_dev, 1000);
    if (ret != 0 && ret != -ENOTSUP) {
      printk("Failed to set OUT report polling period, %d", ret);
      return ret;
    }
  }

  return 0;
}

static int initialize_usb_device(void) {
  sample_usbd = sample_usbd_init_device(msg_cb);

  if (sample_usbd == NULL) {
    printk("Failed to initialize USB device");
    return -ENODEV;
  }

  return 0;
}

int create_usb(void) {
  int ret;

  printk("usb_hid_init()\n");

  ret = register_hid_device();
  if (ret) {
    return ret;
  }

  ret = configure_hid_polling();
  if (ret) {
    return ret;
  }

  ret = initialize_usb_device();
  if (ret) {
    return ret;
  }

  printk("USB HID INITIALIZED (not enabled yet)\n");

  return 0;
}

static int validate_usb_hid(void) {
  if (!hid_registered) {
    printk("usb - hid not registered\n");
    return HID_REGISTERED_ERR;
  }

  if (!device_is_ready(hid_dev)) {
    printk("HID Device is not ready\n");
    return -EIO;
  }

  return 0;
}

static int handle_remote_wakeup(void) {
  int ret;

  if (IS_ENABLED(CONFIG_SAMPLE_USBD_REMOTE_WAKEUP) &&
      usbd_is_suspended(sample_usbd)) {

    // on a press of any button, send wakeup request
    if (report[KB_KEY_CODE1] != 0x0) {
      ret = usbd_wakeup_request(sample_usbd);
      if (ret) {
        printk("Remote wakeup error, %d", ret);
        return ret;
      }
    }
  }

  return 0;
}

int usb_send(void) {
  int ret;

  ret = validate_usb_hid();
  if (ret) {
    return ret;
  }

  ret = handle_remote_wakeup();
  if (ret) {
    return ret;
  }

  ret = hid_device_submit_report(hid_dev, KB_REPORT_COUNT, report);
  if (ret) {
    return ret;
  }

  return 0;
}

static int usb_start(void) {
  int ret;
  if (!usbd_can_detect_vbus(sample_usbd)) {
    ret = usbd_enable(sample_usbd);
    if (ret) {
      printk("Failed to enable USB device support, %d\n", ret);
      return ret;
    }
  }
  printk("USB enabled\n");
  return 0;
}

static int usb_stop(void) {
  int ret = usbd_disable(sample_usbd);
  if (ret) {
    printk("Failed to disable USB device support, %d\n", ret);
    return ret;
  }
  printk("USB disabled\n");
  return 0;
}

static int initialize_usb_task(bool *usb_active) {
  if (create_usb() != 0) {
    printk("USB HID init failed, task exiting\n");
    return -1;
  }

  *usb_active = false;

  if (usb_ready) {
    usb_start();
    *usb_active = true;
  }

  return 0;
}

static void handle_usb_events(uint32_t events, bool *usb_active) {
  if (events & USB_READY_BIT) {
    if (!*usb_active) {
      usb_start();
      *usb_active = true;
    }
  } else if (events & USB_UNREADY_BIT) {
    if (*usb_active) {
      usb_stop();
      *usb_active = false;
    }
  }
}

static void process_usb_report(void) {
  memset(report, 0, sizeof(report));

  k_msgq_get(&msgq_kb_report, report, K_NO_WAIT);

  if (debounce_flag == 0 && report[KB_KEY_CODE1] == 0)
    debounce_flag = 1;
  else
    debounce_flag = 0;

  if (!debounce_flag) {
    usb_send();
  }
}

void usb_task(void *p1, void *p2, void *p3) {
  bool usb_active;

  if (initialize_usb_task(&usb_active) != 0) {
    return;
  }

  while (1) {
    uint32_t events =
        k_event_wait(&usb_state_event, USB_READY_BIT | USB_UNREADY_BIT, true,
                     usb_active ? K_MSEC(pc_interval) : K_FOREVER);

    handle_usb_events(events, &usb_active);

    if (!usb_active) {
      continue;
    }

    process_usb_report();
  }
}
