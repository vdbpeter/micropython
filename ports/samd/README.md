Port of MicroPython to Microchip SAMD MCUs
==========================================

Supports SAMD21 and SAMD51.

## Features:

### REPL

- REPL over USB VCP
- REPL over USART using board specified USART pins (initialised on startup).
  - The USART Pins are board specific, defined in `boards/$(BOARD)/mpconfigboard.h`, and set at compile time. At this stage, the USART cannot be moved to different pins unless `mpconfigboard.h` is edited and the port recompiled.
  - Two functions are accessible through MicroPython:
    - uart_init(). The 'C' function behind this function is what initialises the USART on startup. Calling this function in MicroPython resets any other peripheral operating on these pins and reconnects the USART SERCOM to the designated pins.
    - uart_deinit(). This simply 'disconnects' the SERCOM from the pins. The USART remains operating over USB VCP to maintain access to the REPL.

### Boards

- Added these Boards to the Port:

  - Seeed XIAO_M0 (SAMD21G18A)

  - Seeed WIO Terminal D51 (SAMD51P19A).

- Tweaked:

  - Adafruit ItsyBitsy_M4

### Modules

- Internal modules and functions:

`>>>help('modules')`
`__main__          micropython       uheapq            ustruct`
`_boot             samd              uio               usys`
`_uasyncio         uarray            ujson             utime`
`builtins          uasyncio          uos               uzlib`
`gc                ubinascii         urandom`
`machine           uctypes           ure`
`Plus any modules on the filesystem`

#### Flash

- Internal Flash Block Device `samd.Flash()` initialised with littlefs1 in frozen module '`_boot.py`'. 

- **No external SPI Flash driver**.

- Block Device size is set in `ports/samd/boards/$(BOARD)/mpconfigboard.h` :

  - SAMD21: (eg; XIAO_M0): 64k `0xFFFF` 

   *  SAMD51: (eg; M4's): 128k `0x1FFFF`

#### Pins & LEDs

##### `machine.Pin()` class. 

- GPIO methods & constants:

`value           IN              OUT             PULL_DOWN`
`PULL_UP         high            init            low`
`off             on              toggle`

- Each board has its own pin numbering scheme, so please see the table below (the structure is defined in`boards/$(BOARD)/pins.c`) for pin numbers referenced (index) by 'Pin'. EG; `XIAO_M0/pins.c`: `{{&machine_pin_type}, PIN_PA02}, // A0/D0` means MicroPython `Pin(0)` is XIAO_M0 pin "A0/D0" on SAMD21G18A PortA, Pin2.
- NB. On the XIAO_M0, if the TX & TX pins are used by the `Pin()` class, the `Pin()` initialisation disconnects the pins from the SERCOM similar to the same way the `machine.uart_deinit()` does.

| MicroPython Pin() | XIAO_M0 Pin / SAMD21 Pin# | WIO_Terminal Pin/ SAMD51 Pin# |
| ----------------- | ------------------------- | ----------------------------- |
| Pin(0)            | PA02 / A0/D0              | PB08 / A0/D0                  |
| Pin(1)            | PA04 / A1/D1              | PB09 / A1/D1                  |
| Pin(2)            | PA10 / A2/D2              | PA07 / A2/D2                  |
| Pin(3)            | PA11 / A3/D3              | PB04 / A3/D3                  |
| Pin(4)            | PA08 / A4/D4              | PB05 / A4/D4                  |
| Pin(5)            | PA09 / A5/D5              | PB06 / A5/D5                  |
| Pin(6)            | PB08 / A6/D6/TX           | PA04 / A6/D6                  |
| Pin(7)            | PB09 / A7/D7/RX           | PB07 / A7/D7                  |
| Pin(8)            | PA07 / A8/D8              | PA06 / A8/D8                  |
| Pin(9)            | PA05 / A9/D9              | PD08 / SWITCH_X               |
| Pin(10)           | PA06 / A10/D10            | PD09 / SWITCH_Y               |
| Pin(11)           |                           | PD10 / SWITCH_Z               |
| Pin(12)           |                           | PD12 / SWITCH_B               |
| Pin(13)           |                           | PD20 / SWITCH_U               |
| Pin(14)           |                           | PC26 / BUTTON_1               |
| Pin(15)           |                           | PC27 / BUTTON_2               |
| Pin(16)           |                           | PC28 / BUTTON_3               |
| Pin(17)           |                           | PD11 / BUZZER_CTR             |

##### `machine.Led()` class. 

- GPIO methods & constants:

`value           OUT             high            low`
`off             on              toggle`

- As above, please see `boards/$(BOARD)/pins.c` for pin numbers referenced by 'Led'. EG; `XIAO_M0/pins.c`: `{{&machine_led_type}, PIN_PA17}, // W13` means MicroPython `Led(0)` is XIAO_M0 LED "W13" connected to SAMD21G18A PortA, Pin17.

| MicroPython Led() | XIAO Pin / SAMD21 Pin# | WIO_Terminal Pin/ SAMD51 Pin# |
| ----------------- | ---------------------- | ----------------------------- |
| Led(0)            | PA17 / W13             | PA15 / USER_LED (Blue)        |
| Led(1)            | PA18 / RX_LED          | PC05 / LCD_BACKLIGHT_CTR      |
| Led(2)            | PA19 / TX_LED          |                               |