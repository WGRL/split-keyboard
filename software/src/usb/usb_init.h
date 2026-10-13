// default usb init from documentation

#ifndef USB_INIT_H
#define USB_INIT_H

#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/usb/bos.h>
#include <zephyr/usb/usbd.h>

#include <zephyr/logging/log.h>

// It returns the configured and initialized USB device context on success,
// otherwise it returns NULL.
struct usbd_context *sample_usbd_init_device(usbd_msg_cb_t msg_cb);

/*
 * This function is similar to sample_usbd_init_device(), but does not
 * initialize the device. It allows the application to set additional features,
 * such as additional descriptors.
 */
struct usbd_context *sample_usbd_setup_device(usbd_msg_cb_t msg_cb);

#endif
