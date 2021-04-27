Port of MicroPython to Microchip SAMD MCUs
==========================================

Supports SAMD21 and SAMD51.

Features:
- REPL over USB VCP

Added these Boards to the Port:

 * Seeed XIAO_M0 (SAMD21G18A)
 * Seeed WIO Terminal D51 (SAMD51P19A).

Stuff that's working:

 * Internal modules and functions
 * Internal Flash Block Device (samd.Flash() initialised with littlefs1 in frozen module '_boot.py'.)
 * Block Device size is set in ports/samd/boards//mpconfigboard.h (XIAO_M0: 0xFFFF and 0x1FFFF for the M4's)
 * machine.Pin() class. Only GPIO methods & functions (IN, OUT etc) at this stage. Each board has its own pin numbering scheme, so please see boards//pins.c for pin numbers referenced by 'Pin'. EG; XIAO_M0/pins.c: {{&machine_pin_type}, PIN_PA02}, // A0/D0 means MicroPython Pin(0) is XIAO pin "A0/D0" on SAMD21G18A PortA,Pin2.
 * machine.led() class. OUT only. As above, please see boards//pins.c for pin numbers referenced by 'led'. EG; XIAO_M0/pins.c: {{&machine_led_type}, PIN_PA17}, // W13 means MicroPython led(0) is XIAO led "W13" connected to SAMD21G18A PortA,Pin17.
