#ifndef TILTMOUSE_USB_HID_MOUSE_H
#define TILTMOUSE_USB_HID_MOUSE_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "tiltmouse/mouse_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t tiltmouse_usb_hid_mouse_init(void);
bool tiltmouse_usb_hid_mouse_is_mounted(void);
bool tiltmouse_usb_hid_mouse_is_suspended(void);

/* One selected logical-report transport, with no USB raw-emission bypass.
 * Asynchronous TinyUSB callback events are drained on publish/service/status
 * in the serialized application worker. TM-006 owns regular service cadence.
 */
tm_mouse_transport_t tiltmouse_usb_hid_mouse_transport(void);
void tiltmouse_usb_hid_mouse_service(void);

/* Compatibility entry points now use the same single-owner report ledger.
 * -128 cannot be sent: the descriptor and motion contract are [-127,127].
 * ESP_OK means TinyUSB accepted a new transfer, NOT host delivery.
 * release_all latches a persistent all-up obligation even while unavailable.
 */
esp_err_t tiltmouse_usb_hid_mouse_send(
    bool left_pressed, bool right_pressed, int8_t x, int8_t y);
esp_err_t tiltmouse_usb_hid_mouse_release_all(void);

#ifdef __cplusplus
}
#endif

#endif
