#pragma once
#include <furi.h>

#define RECORD_USBMUX "usbmux"

typedef struct UsbMux UsbMux;

typedef enum {
    UsbMuxDeviceHd3ss3220 = (1 << 0),
} UsbMuxDevice;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Checks if the UsbMux device is initialized.
 *
 * @param instance The UsbMux instance.
 * @param device Pointer to store the initialized device(s).
 * @return true  if initialization was successful, false otherwise.
 */
bool usb_mux_is_device_initialized(UsbMux* instance, UsbMuxDevice* device);

/**
 * @brief Enables or disables power to the board's Type-A (non-PD) USB port
 * switch. Disabled at boot: nothing in the normal boot sequence turns this
 * switch on, so the port has no power until this is called.
 *
 * @param instance The UsbMux instance.
 * @param enable true to enable the port's power switch, false to disable it.
 */
void usb_mux_usb_a_power_enable(UsbMux* instance, bool enable);

#ifdef __cplusplus
}
#endif
