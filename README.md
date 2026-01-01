# compare-attiny-stm32

Comparison of ATTiny and STM32 implementations for the same low-power periodic task.

## Project Overview

This project demonstrates implementing the same functionality on two different microcontroller platforms:
- **ATTiny1616** (8-bit AVR)
- **STM32L0** (32-bit ARM Cortex-M0+)

## Task Specification

Both implementations provide:
- Periodic pin activation: 100 times per minute (every 600ms) for 50ms
- Low power sleep mode between activations
- Wake-up via RTC/Timer
- 3 button inputs with interrupt handling and debouncing
- Maximum power conservation

## Directory Structure

```
.
├── attiny/              # ATTiny1616 implementation
│   ├── main.cpp         # ATTiny source code
│   ├── platformio.ini   # Build configuration
│   └── README.md        # ATTiny-specific documentation
│
├── stm32/               # STM32L0 implementation
│   ├── main.cpp         # STM32 source code
│   ├── platformio.ini   # Build configuration
│   └── README.md        # STM32-specific documentation
│
└── README.md            # This file
```

## Key Differences

### Architecture
- **ATTiny1616**: 8-bit AVR RISC architecture, simpler peripheral set
- **STM32L0**: 32-bit ARM Cortex-M0+, more complex peripherals and features

### Power Management
- **ATTiny1616**: Power-Down sleep mode with RTC running (~1-2 µA)
- **STM32L0**: Stop mode with LP voltage regulator (~1-2 µA)

### Clock Configuration
- **ATTiny1616**: Uses internal 32kHz oscillator for RTC
- **STM32L0**: Uses LSI (~37 kHz) for RTC, MSI (2.097 MHz) for system clock

### Code Complexity
- **ATTiny1616**: ~150 lines, simpler register configuration
- **STM32L0**: ~300 lines, more complex clock and peripheral setup

### Interrupt Handling
- **ATTiny1616**: Port interrupts for buttons, single interrupt vector
- **STM32L0**: EXTI (External Interrupt) system with multiple vectors

## Building and Flashing

Both projects use PlatformIO for building and flashing.

### ATTiny1616
```bash
cd attiny
pio run --target upload
```

### STM32L0
```bash
cd stm32
pio run --target upload
```

## Hardware Requirements

### ATTiny1616
- ATTiny1616 microcontroller
- UPDI programmer (e.g., SerialUPDI, jtag2updi)
- 3 buttons connected to PB0, PB1, PB2 (active low)
- Output load on PA3

### STM32L0
- STM32L0 board (e.g., Nucleo-L053R8)
- ST-Link programmer (built-in on Nucleo boards)
- 3 buttons (PC13 is built-in on Nucleo, add PB0, PB1)
- Output on PA5 (LED on Nucleo board)

## Power Consumption Comparison

Both platforms achieve similar ultra-low power consumption in sleep mode:
- Sleep mode: ~1-2 µA
- Active periods: Brief wake-ups for pin activation

The STM32L0 has more flexibility in clock configuration and power modes, while the ATTiny1616 achieves similar results with simpler code.

## Use Cases

**Choose ATTiny1616 when:**
- Cost is critical
- Simple, compact design needed
- 8-bit performance is sufficient
- Programming in C/C++ with AVR toolchain

**Choose STM32L0 when:**
- Need more processing power
- Require more peripherals (SPI, I2C, UART, etc.)
- Want ecosystem support (ST libraries, HAL)
- 32-bit architecture benefits (larger memory, faster computation)

## License

This is sample code for educational and comparison purposes.
