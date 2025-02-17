/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Damien P. George
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
 */
#include <stdio.h>

#include "py/runtime.h"
#include "extmod/vfs.h"
#include "samd_soc.h"
#include "hal_flash.h"

// ASF 4
#include "hal_flash.h"
#if defined(MCU_SAMD21)
#include "lib/asf4/samd21/hal/include/hal_init.h"
#include "lib/asf4/samd21/hpl/gclk/hpl_gclk_base.h"
#include "lib/asf4/samd21/hpl/pm/hpl_pm_base.h"
#elif defined(MCU_SAMD51)
#include "lib/asf4/samd51/hal/include/hal_init.h"
#include "lib/asf4/samd51/hpl/gclk/hpl_gclk_base.h"
#include "lib/asf4/samd51/hpl/pm/hpl_pm_base.h"
#include "lib/asf4/samd51/hri/hri_mclk_d51.h"
#endif


static struct flash_descriptor flash_desc;
//STATIC mp_int_t BLOCK_SIZE = 1536; //24x 64B flash pages;
STATIC mp_int_t BLOCK_SIZE = VFS_BLOCK_SIZE_BYTES;
extern const mp_obj_type_t samd_flash_type;

typedef struct _samd_flash_obj_t {
    mp_obj_base_t base;
    uint32_t flash_base;
    uint32_t flash_size;
} samd_flash_obj_t;

// Build a 64k Flash storage at top. 256k-64k=196k
// 256*1024=262144 minus 64*1024=65536 = 196608 = 0x30000
STATIC samd_flash_obj_t samd_flash_obj = {
//STATIC samd_flash_obj_t = {
    .base = { &samd_flash_type },
    .flash_base = MICROPY_HW_FLASH_STORAGE_BASE,
    //.flash_base = 0x30000, // 
    .flash_size = MICROPY_HW_FLASH_STORAGE_BYTES,
    //.flash_size = 0xFFC0, // 0xFFFF-64B=FFC0,
};

///////////////// FLASH stuff
STATIC mp_obj_t samd_flash_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    // No args required. bdev=Flash(). Start Addr & Size defined in samd_flash_obj.
    mp_arg_check_num(n_args, n_kw, 0,0, false);

    // Return singleton object.
    return MP_OBJ_FROM_PTR(&samd_flash_obj);
}

// Flash init (from cctpy)
// Method is needed for when MP starts up in _boot.py
STATIC mp_obj_t samd_flash_init(void) {
    #ifdef SAMD51
    hri_mclk_set_AHBMASK_NVMCTRL_bit(MCLK);
    #endif
    #ifdef SAMD21
    _pm_enable_bus_clock(PM_BUS_APBB, NVMCTRL);
    #endif

    flash_init(&flash_desc, NVMCTRL);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(samd_flash_init_obj, samd_flash_init);

// Function for ioctl.
STATIC mp_obj_t eraseblock(uint32_t sector_in) {
    // Destination address aligned with page start to be erased.
    // Number of pages to be erased.
    uint32_t DEST_ADDR = sector_in; 
    // adf4 API call
    mp_int_t PAGE_SIZE = flash_get_page_size(&flash_desc);
    flash_erase(&flash_desc,DEST_ADDR,(BLOCK_SIZE / PAGE_SIZE));
    return mp_const_none;
}

STATIC mp_obj_t samd_flash_version(void) {
    printf("Flash Driver Version: %lu\n", flash_get_version());
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(samd_flash_version_obj, samd_flash_version);


STATIC mp_obj_t samd_flash_size(void) {
    // adf4 API calls
    mp_int_t PAGES = flash_get_total_pages(&flash_desc);
    mp_int_t PAGE_SIZE = flash_get_page_size(&flash_desc);
    printf("Flash Size: %u Bytes\n",  PAGES*PAGE_SIZE);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(samd_flash_size_obj, samd_flash_size);


STATIC mp_obj_t samd_flash_readblocks(size_t n_args, const mp_obj_t *args) {
    uint32_t offset = (mp_obj_get_int(args[1]) * BLOCK_SIZE) + samd_flash_obj.flash_base;
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[2], &bufinfo, MP_BUFFER_WRITE);
    if (n_args == 4) {
        offset += mp_obj_get_int(args[3]);
    }

    // Read data to flash (adf4 API)
    flash_read(&flash_desc,offset,bufinfo.buf,bufinfo.len);

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(samd_flash_readblocks_obj, 3, 4, samd_flash_readblocks);


STATIC mp_obj_t samd_flash_writeblocks(size_t n_args, const mp_obj_t *args) {
    uint32_t offset = (mp_obj_get_int(args[1]) * BLOCK_SIZE) + samd_flash_obj.flash_base;
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[2], &bufinfo, MP_BUFFER_READ);
    if (n_args == 3) {
        eraseblock(offset);
        // TODO check return value
    } else {
        offset += mp_obj_get_int(args[3]);
    }
    // Write data to flash (adf4 API)
    flash_write(&flash_desc,offset, bufinfo.buf, bufinfo.len);
    // TODO check return value
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(samd_flash_writeblocks_obj, 3, 4, samd_flash_writeblocks);


STATIC mp_obj_t samd_flash_ioctl(mp_obj_t self_in, mp_obj_t cmd_in, mp_obj_t arg_in) {
    samd_flash_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_int_t cmd = mp_obj_get_int(cmd_in);

    switch (cmd) {
        case MP_BLOCKDEV_IOCTL_INIT:
            samd_flash_init();
            return MP_OBJ_NEW_SMALL_INT(0);
        case MP_BLOCKDEV_IOCTL_DEINIT:
            return MP_OBJ_NEW_SMALL_INT(0);
        case MP_BLOCKDEV_IOCTL_SYNC:
            return MP_OBJ_NEW_SMALL_INT(0);
        case MP_BLOCKDEV_IOCTL_BLOCK_COUNT:
            return MP_OBJ_NEW_SMALL_INT(self->flash_size / BLOCK_SIZE);
        case MP_BLOCKDEV_IOCTL_BLOCK_SIZE:
            return MP_OBJ_NEW_SMALL_INT(BLOCK_SIZE);
        case MP_BLOCKDEV_IOCTL_BLOCK_ERASE: {
            eraseblock(mp_obj_get_int(arg_in) * BLOCK_SIZE + samd_flash_obj.flash_base);
            // TODO check return value
            return MP_OBJ_NEW_SMALL_INT(0);
        }
        default:
            return mp_const_none;
    }
}

STATIC MP_DEFINE_CONST_FUN_OBJ_3(samd_flash_ioctl_obj, samd_flash_ioctl);


STATIC const mp_rom_map_elem_t samd_flash_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_flash_version), MP_ROM_PTR(&samd_flash_version_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_size), MP_ROM_PTR(&samd_flash_size_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_init), MP_ROM_PTR(&samd_flash_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_readblocks), MP_ROM_PTR(&samd_flash_readblocks_obj) },
    { MP_ROM_QSTR(MP_QSTR_writeblocks), MP_ROM_PTR(&samd_flash_writeblocks_obj) },
    { MP_ROM_QSTR(MP_QSTR_ioctl), MP_ROM_PTR(&samd_flash_ioctl_obj) },
};
STATIC MP_DEFINE_CONST_DICT(samd_flash_locals_dict, samd_flash_locals_dict_table);

const mp_obj_type_t samd_flash_type = {
    { &mp_type_type },
    .name = MP_QSTR_Flash,
    .make_new = samd_flash_make_new,
    .locals_dict = (mp_obj_dict_t *)&samd_flash_locals_dict,
};
