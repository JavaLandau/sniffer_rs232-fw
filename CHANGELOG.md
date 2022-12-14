### V.1.0 - 2022-10-23

Release accepted

**Documentation**
+ Updated doxygen documentation with version 1.2
+ Added documentation "Configuration menu" with version 1.0

**Scripts**
+ Implemented script for UART tests of the device

### V.1.0-RC5 - 2022-10-23

**Common**
+ Minimal length of recognized LIN break is increased to 12 bits in algorithm of Sniffer RS-232 (more stability in detection)

**Bug fixe(s)**
+ Timer TIM6 is replaced with TIM5 with 32-bit counter (it lets recognize LIN break with low UART speed)

### V.1.0-RC4 - 2022-10-23

**Documentation**
+ Added doxygen documentation v.1.1 for the project

**Bug fixe(s)**
+ Fixed BSP UART driver to read 9-bit data or data with parity bit from RS-232 channels
+ Added workaround into BSP UART IRQ handler if UART error occurred before DMA receiver is enabled
+ Fixed display of LIN support in case of not presettings
+ Added entry to upper menu "Algorithm" from menu item "Defaults->YES"

### V.1.0-RC3 - 2022-10-13

**New feature(s)**
+ #11: LIN Support
  - added support of LIN protocol to the algorithm and UART presetting
  - corrected BSP UART to LIN support
+ #12: Implement HardFault and other fault handlers
  - implemented handlers for basic MPU interrupts

### V.1.0-RC2 - 2022-10-10

**New feature(s)**
+ Extended BSP UART API with "bsp_uart_is_started()"
+ Added display of firmware version

**Bug fixe(s)**
+ Fixed API "cli_welcome()"

### V.1.0-RC1 - 2022-10-08

**New feature(s)**
+ Implementation of BSP modules
+ Implementation of algorithm of Sniffer RS-232
+ Implementation of menu library
+ Implementation of CLI
+ #4: LED business logic
  - refactoring of LED RGB driver
  - implemented LED business logic in app_led.c
+ #3: Display business logic
  - added API "lcd1602_cprintf()"
  - added display messages for cases of initialization and algorithm work
  - added LCD1602 messages for cases of RS232 monitoring
+ #8: Output of monitored RS-232 via CLI
  - implemented RS232 trace via CLI with configuration features
+ #7: Button business logic
  - implemented button business logic
+ #10: Option to save found RS-232 to presettings
  - implemented option to save recognized RS-232 parameters to presettings
+ Added version and welcome routine to the CLI

**Bug fixe(s)**
+ Added skipped RS-232 channel type into menu and other modules
+ Added usage of TX buffer into BSP UART driver
+ Disabled word alignment for structure of flash config