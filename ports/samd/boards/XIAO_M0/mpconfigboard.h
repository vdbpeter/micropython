#define MICROPY_HW_BOARD_NAME "Seeed Xiao"
#define MICROPY_HW_MCU_NAME   "SAMD21G18A"

// MicroPython configs
// samd_flash.c flash parameters
// Build a 64k Flash storage at top. 256k-64k=196k
// 256*1024=262144 minus 64*1024=65536 = 196608 = 0x30000
#define MICROPY_HW_FLASH_STORAGE_BASE       (0x30000)
#define MICROPY_HW_FLASH_STORAGE_BYTES      (0xFFFF) 
#define VFS_BLOCK_SIZE_BYTES                (1536) //

// ASF4 MCU package specific Pin definitions
#include "samd21g18a.h"

//2/2/21 from CircuitPython- not yet built...
//#define DEFAULT_I2C_BUS_SCL (&pin_PA09) // was PA23
//#define DEFAULT_I2C_BUS_SDA (&pin_PA08) // was PA22

