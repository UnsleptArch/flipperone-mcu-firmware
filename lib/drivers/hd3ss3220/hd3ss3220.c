#include "hd3ss3220.h"
#include <furi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_i2c.h>
#include <pico/error.h>
#include <pico/types.h>

#define TAG "Hd3ss3220"

#ifdef HD3SS3220_DEBUG_ENABLE
#define HD3SS3220_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define HD3SS3220_DEBUG(...)
#endif

struct Hd3ss3220 {
    const FuriHalI2cBusHandle* i2c_handle;
    uint8_t address;
    const GpioPin* pin_interrupt;
    Hd3ss3220CallbackInput input_callback;
    void* callback_context;
};

static Hd3ss3220Status hd3ss3220_check_status(int status) {
    Hd3ss3220Status ret = Hd3ss3220StatusUnknown;
    if(status >= PICO_OK) {
        ret = Hd3ss3220StatusOk;
    } else if(status == PICO_ERROR_GENERIC) {
        ret = Hd3ss3220StatusError;
    } else if(status == PICO_ERROR_TIMEOUT) {
        ret = Hd3ss3220StatusTimeout;
    } else {
        ret = Hd3ss3220StatusUnknown;
    }

    return ret;
}

static Hd3ss3220Status hd3ss3220_write_reg(Hd3ss3220* instance, Hd3ss3220Reg reg, uint8_t data) {
    furi_check(instance);

    uint8_t buffer[2] = {reg, data};

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    } else {
        HD3SS3220_DEBUG(TAG, "Wrote reg 0x%02X: %08b", reg, data);
    }

    return hd3ss3220_check_status(ret);
}

static Hd3ss3220Status hd3ss3220_read_buf(Hd3ss3220* instance, Hd3ss3220Reg reg, uint8_t* data, size_t length) {
    furi_check(instance);
    furi_check(data);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_trx_blocking(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, data, length, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to read reg 0x%02X", reg);
    } else {
        HD3SS3220_DEBUG(TAG, "Read reg 0x%02X: %d bytes", reg, length);
    }

    return hd3ss3220_check_status(ret);
}

static Hd3ss3220Status hd3ss3220_read_reg(Hd3ss3220* instance, Hd3ss3220Reg reg, uint8_t* data) {
    return hd3ss3220_read_buf(instance, reg, data, 1);
}

static __isr __not_in_flash_func(void) hd3ss3220_interrupt_handler(void* ctx) {
    Hd3ss3220* instance = (Hd3ss3220*)ctx;
    if(instance->input_callback) {
        instance->input_callback(instance->callback_context);
    }
}

Hd3ss3220* hd3ss3220_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address, const GpioPin* pin_interrupt) {
    Hd3ss3220* instance = (Hd3ss3220*)malloc(sizeof(Hd3ss3220));
    instance->i2c_handle = i2c_handle;
    instance->address = address;
    instance->pin_interrupt = pin_interrupt;

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);
    if(ret) {
        FURI_LOG_I(TAG, "HD3SS3220 device ready at address 0x%02X", instance->address);
        if(instance->pin_interrupt) {
            furi_hal_gpio_init_simple(instance->pin_interrupt, GpioModeInput);
            furi_hal_gpio_add_int_callback(instance->pin_interrupt, GpioConditionFall, hd3ss3220_interrupt_handler, instance);
        }
#ifdef HD3SS3220_DEBUG_ENABLE
        Hd3ss3220RegDeviceIdRegBits device_id = {0};
        hd3ss3220_read_buf(instance, Hd3ss3220RegDeviceId, device_id.id, sizeof(device_id.id));
        HD3SS3220_DEBUG(
            TAG,
            "Device ID: %02X %02X %02X %02X %02X %02X %02X %02X",
            device_id.id[0],
            device_id.id[1],
            device_id.id[2],
            device_id.id[3],
            device_id.id[4],
            device_id.id[5],
            device_id.id[6],
            device_id.id[7]);
        uint8_t revision = 0;
        hd3ss3220_read_reg(instance, Hd3ss3220RegDeviceRevision, &revision);
        HD3SS3220_DEBUG(TAG, "Revision: %02X", revision);
#endif
    } else {
        FURI_LOG_E(TAG, "HD3SS3220 device not ready at address 0x%02X", instance->address);
        free(instance);
        return NULL;
    }

    return instance;
}

void hd3ss3220_deinit(Hd3ss3220* instance) {
    furi_check(instance);
    if(instance->pin_interrupt) {
        furi_hal_gpio_remove_int_callback(instance->pin_interrupt);
        furi_hal_gpio_init_ex(instance->pin_interrupt, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    }
    free(instance);
}

void hd3ss3220_set_input_callback(Hd3ss3220* instance, Hd3ss3220CallbackInput callback, void* context) {
    furi_check(instance);
    FURI_CRITICAL_ENTER();
    instance->input_callback = callback;
    instance->callback_context = context;
    FURI_CRITICAL_EXIT();
}

Hd3ss3220Status hd3ss3220_get_device_id(Hd3ss3220* instance, Hd3ss3220RegDeviceIdRegBits* device_id) {
    furi_check(instance);
    furi_check(device_id);
    Hd3ss3220Status res = hd3ss3220_read_buf(instance, Hd3ss3220RegDeviceId, device_id->id, sizeof(device_id->id));
    if(res != Hd3ss3220StatusOk) {
        FURI_LOG_E(TAG, "Failed to get device id!");
    }
    return res;
}

Hd3ss3220Status hd3ss3220_get_connect_status(Hd3ss3220* instance, Hd3ss3220RegConnectStatusRegBits* status) {
    furi_check(instance);
    furi_check(status);
    Hd3ss3220Status res = hd3ss3220_read_reg(instance, Hd3ss3220RegConnectStatus, (uint8_t*)status);
    if(res != Hd3ss3220StatusOk) {
        FURI_LOG_E(TAG, "Failed to get connect status!");
    }
    return res;
}

Hd3ss3220Status hd3ss3220_get_control(Hd3ss3220* instance, Hd3ss3220RegControlRegBits* control) {
    furi_check(instance);
    furi_check(control);
    Hd3ss3220Status res = hd3ss3220_read_reg(instance, Hd3ss3220RegControl, (uint8_t*)control);
    if(res != Hd3ss3220StatusOk) {
        FURI_LOG_E(TAG, "Failed to get control status!");
    }
    return res;
}

Hd3ss3220Status hd3ss3220_get_general_control(Hd3ss3220* instance, Hd3ss3220RegGeneralControlRegBits* general_control) {
    furi_check(instance);
    furi_check(general_control);
    Hd3ss3220Status res = hd3ss3220_read_reg(instance, Hd3ss3220RegGeneralControl, (uint8_t*)general_control);
    if(res != Hd3ss3220StatusOk) {
        FURI_LOG_E(TAG, "Failed to get general control!");
    }
    return res;
}

Hd3ss3220Status hd3ss3220_get_device_revision(Hd3ss3220* instance, uint8_t* revision) {
    furi_check(instance);
    furi_check(revision);
    Hd3ss3220Status res = hd3ss3220_read_reg(instance, Hd3ss3220RegDeviceRevision, revision);
    if(res != Hd3ss3220StatusOk) {
        FURI_LOG_E(TAG, "Failed to get device revision!");
    }
    return res;
}

Hd3ss3220Status hd3ss3220_set_current_mode_advertise(Hd3ss3220* instance, Hd3ss3220CurrentModeAdvertise mode) {
    furi_check(instance);
    Hd3ss3220Status res = Hd3ss3220StatusUnknown;
    Hd3ss3220RegConnectStatusRegBits status = {0};
    do {
        res = hd3ss3220_read_reg(instance, Hd3ss3220RegConnectStatus, (uint8_t*)&status);
        if(res != Hd3ss3220StatusOk) {
            break;
        }
        status.current_mode_advertise = mode;
        res = hd3ss3220_write_reg(instance, Hd3ss3220RegConnectStatus, *(uint8_t*)&status);
    } while(0);
    if(res != Hd3ss3220StatusOk) {
        FURI_LOG_E(TAG, "Failed to set current mode advertise!");
    }
    return res;
}

Hd3ss3220Status hd3ss3220_set_mode_select(Hd3ss3220* instance, Hd3ss3220ModeSelect mode) {
    furi_check(instance);
    Hd3ss3220Status res = Hd3ss3220StatusUnknown;
    Hd3ss3220RegGeneralControlRegBits general_control = {0};
    do {
        res = hd3ss3220_read_reg(instance, Hd3ss3220RegGeneralControl, (uint8_t*)&general_control);
        if(res != Hd3ss3220StatusOk) {
            break;
        }
        general_control.mode_select = mode;
        res = hd3ss3220_write_reg(instance, Hd3ss3220RegGeneralControl, *(uint8_t*)&general_control);
    } while(0);
    if(res != Hd3ss3220StatusOk) {
        FURI_LOG_E(TAG, "Failed to set mode select!");
    }
    return res;
}

Hd3ss3220Status hd3ss3220_soft_reset(Hd3ss3220* instance) {
    furi_check(instance);
    Hd3ss3220Status res = Hd3ss3220StatusUnknown;
    Hd3ss3220RegGeneralControlRegBits general_control = {0};
    do {
        // Read-modify-write, same as the other General Control setters: this
        // bit shares a register with mode_select/source_pref/debounce, and a
        // plain write would reset those to 0 instead of leaving them alone.
        res = hd3ss3220_read_reg(instance, Hd3ss3220RegGeneralControl, (uint8_t*)&general_control);
        if(res != Hd3ss3220StatusOk) {
            break;
        }
        general_control.i2c_soft_reset = 1; // self-clearing
        res = hd3ss3220_write_reg(instance, Hd3ss3220RegGeneralControl, *(uint8_t*)&general_control);
    } while(0);
    if(res != Hd3ss3220StatusOk) {
        FURI_LOG_E(TAG, "Failed to soft reset!");
    }
    return res;
}
