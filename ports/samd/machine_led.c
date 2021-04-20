/*
 * This is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2021 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Uses pins.h & pins.c to create board (MCU package) specific 'machine_led_obj' array.
 */

#include <stdio.h>
#include <string.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "lib/utils/mpirq.h"
#include "modmachine.h"
#include "extmod/virtpin.h"

#include "pins.h" // boards/<BOARD>/

// ASF4 (MCU package specific pin defs in 'boards')
#include "hal_gpio.h"
#include "hpl_gpio.h"
#include "hal_atomic.h"

#define GPIO_MODE_IN (0)
#define GPIO_MODE_OUT (1)
#define GPIO_MODE_OPEN_DRAIN (2)
#define GPIO_MODE_ALT (3)

// asf4 hpl_gpio.h gpio_pull_mode
//#define GPIO_PULL_OFF (0) // 0 = None
#define GPIO_PULL_UP (1)
#define GPIO_PULL_DOWN (2)

#define GPIO_IRQ_ALL (0xf)

// Macros to access the state of the hardware.
#define GPIO_GET_FUNCSEL(id) ((iobank0_hw->io[(id)].ctrl & IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS) >> IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB)
#define GPIO_IS_OUT(id) (sio_hw->gpio_oe & (1 << (id)))
#define GPIO_IS_PULL_UP(id) (padsbank0_hw->io[(id)] & PADS_BANK0_GPIO0_PUE_BITS)
#define GPIO_IS_PULL_DOWN(id) (padsbank0_hw->io[(id)] & PADS_BANK0_GPIO0_PDE_BITS)

// Open drain behaviour is simulated.
#define GPIO_IS_OPEN_DRAIN(id) (machine_pin_open_drain_mask & (1 << (id)))

// pin.init(mode, pull=None, *, value=None, alt=FUNC_SIO)
STATIC mp_obj_t machine_led_obj_init_helper(const machine_led_obj_t *self, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_mode, ARG_pull, ARG_value, ARG_alt };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_mode, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE}},
        { MP_QSTR_pull, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE}},
        { MP_QSTR_value, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE}},
    };
        //{ MP_QSTR_alt, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = GPIO_FUNC_SIO}},

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // set initial value (do this before configuring mode/pull)
    if (args[ARG_value].u_obj != mp_const_none) {
        gpio_set_pin_level(self->id, mp_obj_is_true(args[ARG_value].u_obj));
    }

    // configure mode
    mp_hal_pin_output(self->id);


    return mp_const_none;
}

// constructor(id, ...)
mp_obj_t mp_led_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, MP_OBJ_FUN_ARGS_MAX, true);

    // get the wanted pin object
    int wanted_pin = mp_obj_get_int(args[0]);
    printf("'Board' Pin: %u\n", wanted_pin);
    printf("Pin Array Size: %u\n", MP_ARRAY_SIZE(machine_led_obj));
    const machine_led_obj_t *self = NULL;
    if (0 <= wanted_pin && wanted_pin < MP_ARRAY_SIZE(machine_led_obj)) {
        self = (machine_led_obj_t *)&machine_led_obj[wanted_pin];
    }

    // the array could be padded with 'nulls' (see other Ports).
    // Will also error if the asked for pin (index) is greater than the array row size.
    if (self == NULL || self->base.type == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid pin"));
    }

    // If the previous test is true, we dont get this far...
    printf("Got 'real' Pin: %lu\n", self->id);

    if (n_args > 1 || n_kw > 0) {
        // pin mode given, so configure this GPIO
        mp_map_t kw_args;
        mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
        machine_led_obj_init_helper(self, n_args - 1, args + 1, &kw_args);
    }

    return MP_OBJ_FROM_PTR(self);
}


// fast method for getting/setting pin value
STATIC mp_obj_t machine_led_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 1, false);
    machine_led_obj_t *self = self_in;
    if (n_args == 0) {
        // get pin
        return MP_OBJ_NEW_SMALL_INT(gpio_get_pin_level(self->id));
    } else {
        // set pin
        bool value = mp_obj_is_true(args[0]);
        if (GPIO_IS_OPEN_DRAIN(self->id)) {
            // PvdB. Creates compilation error. Don't know what it does (see RP2)
            //MP_STATIC_ASSERT(GPIO_DIRECTION_IN == 0 && GPIO_DIRECTION_OUT == 1);
            gpio_set_pin_direction(self->id, 1 - value);
        } else {
            gpio_set_pin_level(self->id, value);
        }
        return mp_const_none;
    }
}

// pin.init(mode, pull)
STATIC mp_obj_t machine_led_obj_init(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    return machine_led_obj_init_helper(args[0], n_args - 1, args + 1, kw_args);
}
MP_DEFINE_CONST_FUN_OBJ_KW(machine_led_init_obj, 1, machine_led_obj_init);

// pin.value([value])
STATIC mp_obj_t machine_led_value(size_t n_args, const mp_obj_t *args) {
    return machine_led_call(args[0], n_args - 1, 0, args + 1);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_led_value_obj, 1, 2, machine_led_value);

// pin.low()
STATIC mp_obj_t machine_led_low(mp_obj_t self_in) {
    machine_led_obj_t *self = MP_OBJ_TO_PTR(self_in);
    gpio_set_pin_direction(self->id, GPIO_DIRECTION_OUT);
    gpio_set_pin_level(self->id, false);
/*
    if (GPIO_IS_OPEN_DRAIN(self->id)) {
        gpio_set_pin_direction(self->id, GPIO_DIRECTION_OUT);
    } else {
        gpio_clr_mask(1u << self->id);
    }
*/
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_led_low_obj, machine_led_low);

// pin.high()
STATIC mp_obj_t machine_led_high(mp_obj_t self_in) {
    machine_led_obj_t *self = MP_OBJ_TO_PTR(self_in);
    gpio_set_pin_direction(self->id, GPIO_DIRECTION_OUT);
    gpio_set_pin_level(self->id, true);
/*
    if (GPIO_IS_OPEN_DRAIN(self->id)) {
        gpio_set_pin_direction(self->id, GPIO_DIRECTION_IN);
    } else {
        gpio_set_mask(1u << self->id);
    }
*/
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_led_high_obj, machine_led_high);

// pin.toggle()
STATIC mp_obj_t machine_led_toggle(mp_obj_t self_in) {
    machine_led_obj_t *self = MP_OBJ_TO_PTR(self_in);
    gpio_set_pin_direction(self->id, GPIO_DIRECTION_OUT);
    gpio_toggle_pin_level(self->id);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_led_toggle_obj, machine_led_toggle);


STATIC const mp_rom_map_elem_t machine_led_locals_dict_table[] = {
    // instance methods
    //{ MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&machine_led_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_value), MP_ROM_PTR(&machine_led_value_obj) },
    { MP_ROM_QSTR(MP_QSTR_low), MP_ROM_PTR(&machine_led_low_obj) },
    { MP_ROM_QSTR(MP_QSTR_high), MP_ROM_PTR(&machine_led_high_obj) },
    { MP_ROM_QSTR(MP_QSTR_off), MP_ROM_PTR(&machine_led_low_obj) },
    { MP_ROM_QSTR(MP_QSTR_on), MP_ROM_PTR(&machine_led_high_obj) },
    { MP_ROM_QSTR(MP_QSTR_toggle), MP_ROM_PTR(&machine_led_toggle_obj) },
    //{ MP_ROM_QSTR(MP_QSTR_irq), MP_ROM_PTR(&machine_led_irq_obj) },

    // class constants
    { MP_ROM_QSTR(MP_QSTR_OUT), MP_ROM_INT(GPIO_MODE_OUT) },
    //{ MP_ROM_QSTR(MP_QSTR_OPEN_DRAIN), MP_ROM_INT(GPIO_MODE_OPEN_DRAIN) },
};
STATIC MP_DEFINE_CONST_DICT(machine_led_locals_dict, machine_led_locals_dict_table);

const mp_obj_type_t machine_led_type = {
    { &mp_type_type },
    .name = MP_QSTR_led,
    // .print = machine_led_print,
    .make_new = mp_led_make_new,
    .call = machine_led_call,
    //.protocol = &pin_pin_p,
    .locals_dict = (mp_obj_t)&machine_led_locals_dict,
};

