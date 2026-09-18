#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>
#include "hd3ss3220_reg.h"

#define HD3SS3220_ADDRESS 0x47

typedef struct Hd3ss3220 Hd3ss3220;
typedef void (*Hd3ss3220CallbackInput)(void* context);

typedef enum {
    Hd3ss3220StatusUnknown = 0,
    Hd3ss3220StatusOk = 1,
    Hd3ss3220StatusError = -1,
    Hd3ss3220StatusTimeout = -2,
} Hd3ss3220Status;

typedef enum {
    Hd3ss3220CurrentModeAdvertiseDefault = 0b00, /** Default (500mA/900mA), initial value at startup */
    Hd3ss3220CurrentModeAdvertiseMid = 0b01, /** Mid (1.5A) */
    Hd3ss3220CurrentModeAdvertiseHigh = 0b10, /** High (3A) */
} Hd3ss3220CurrentModeAdvertise;

typedef enum {
    Hd3ss3220ModeSelectDrp = 0b00, /** DRP mode, start from unattached.SNK (default) */
    Hd3ss3220ModeSelectUfp = 0b01, /** UFP mode (unattached.SNK) */
    Hd3ss3220ModeSelectDfp = 0b10, /** DFP mode (unattached.SRC) */
} Hd3ss3220ModeSelect;

#ifdef __cplusplus
extern "C" {
#endif

Hd3ss3220* hd3ss3220_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address, const GpioPin* pin_interrupt);
void hd3ss3220_deinit(Hd3ss3220* instance);
void hd3ss3220_set_input_callback(Hd3ss3220* instance, Hd3ss3220CallbackInput callback, void* context);
Hd3ss3220Status hd3ss3220_get_device_id(Hd3ss3220* instance, Hd3ss3220RegDeviceIdRegBits* device_id);
Hd3ss3220Status hd3ss3220_get_connect_status(Hd3ss3220* instance, Hd3ss3220RegConnectStatusRegBits* status);
Hd3ss3220Status hd3ss3220_get_control(Hd3ss3220* instance, Hd3ss3220RegControlRegBits* control);
Hd3ss3220Status hd3ss3220_get_general_control(Hd3ss3220* instance, Hd3ss3220RegGeneralControlRegBits* general_control);
Hd3ss3220Status hd3ss3220_get_device_revision(Hd3ss3220* instance, uint8_t* revision);
Hd3ss3220Status hd3ss3220_set_current_mode_advertise(Hd3ss3220* instance, Hd3ss3220CurrentModeAdvertise mode);
Hd3ss3220Status hd3ss3220_set_mode_select(Hd3ss3220* instance, Hd3ss3220ModeSelect mode);
Hd3ss3220Status hd3ss3220_soft_reset(Hd3ss3220* instance);

#ifdef __cplusplus
}
#endif
