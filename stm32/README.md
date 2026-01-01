# STM32L0 Implementation

## Overview
This implementation provides a low-power adjustable BPM (beats per minute) pin activation system for the STM32L0 series microcontroller (tested with STM32L053R8).

## Features
- **Adjustable BPM**: Pin PA5 is activated at adjustable rate (40-155 BPM, default 100 BPM)
- **Button Controls**:
  - **PC13**: Increase BPM by 5 (Blue button on Nucleo board)
  - **PB0**: Decrease BPM by 5
  - **PB1**: Reserved for future use
- **Low Power Mode**: Uses Stop mode with voltage regulator in low power mode
- **RTC Wake-up**: Real-Time Clock wake-up timer provides timing and wake-up from Stop mode (dynamically reconfigured)
- **Button Interrupts**: 3 buttons with EXTI interrupt-driven input and software debouncing
- **Power Optimization**:
  - MSI clock (2.097 MHz) for low power operation
  - Voltage scaling to Range 1 (1.8V)
  - Stop mode with LP voltage regulator
  - Debug disabled in low power modes
  - GPIO configured for low speed

## 3.3V Operation
- **Operating Voltage**: STM32L0 operates at 1.65V - 3.6V, fully compatible with 3.3V
- **Brownout Reset (BOR)**: 
  - BOR is enabled by default in STM32L0 (configured via option bytes)
  - Default BOR threshold: ~1.8V (BOR Level 1)
  - **For 3.3V operation**: BOR Level 1 or 2 is appropriate (1.8V - 2.3V)
  - BOR protects against brownout conditions and ensures proper reset during power-up/down
  - Can be configured using STM32CubeProgrammer or ST-Link Utility
- **Programmable Voltage Detector (PVD)**: 
  - Additional voltage monitoring available if needed (not enabled by default)
  - Can trigger interrupt when VDD drops below configurable threshold

## Hardware Configuration

### Output Pin
- **PA5**: Output pin for periodic activation (LED on Nucleo board)

### Button Pins (Active Low with Pull-ups)
- **PC13**: Increase BPM button (Blue button on Nucleo board)
- **PB0**: Decrease BPM button
- **PB1**: Reserved button (future use)

## BPM Configuration
- **Range**: 40 - 155 BPM
- **Default**: 100 BPM
- **Step Size**: ±5 BPM per button press
- **Activation Duration**: 50ms high pulse per beat

## Power Consumption
The STM32L0 in Stop mode:
- Typical: ~1-2 µA with RTC running
- Active mode: Brief periods during pin activation and wake-up
- MSI clock keeps power consumption low during active periods

**Note**: The current implementation uses a busy-wait loop for the 50ms activation period. For maximum power efficiency in production code, consider using a hardware timer (e.g., TIM2) to handle the 50ms duration more efficiently.

## Building

### Build Flags Explained

The `platformio.ini` file contains compiler and linker flags that optimize the code for size and performance:

#### Compiler Flags
- **`-Os`**: Optimize for size. Reduces code size while maintaining reasonable performance, important for microcontrollers with limited flash.
- **`-DSTM32L053xx`**: Defines the specific STM32 chip family for CMSIS headers and conditional compilation.
- **`-DSTM32L0`**: Defines a general STM32L0 series macro for broader categorization.
- **`-ffunction-sections`**: Places each function in its own section in the object file. Enables the linker to remove unused functions.
- **`-fdata-sections`**: Places each data variable in its own section. Enables the linker to remove unused data.
- **`-flto`**: Enables Link-Time Optimization (LTO). The compiler can optimize across translation units for smaller, faster code.

#### Linker Flags
- **`-Wl,--gc-sections`**: Garbage-collect unused sections during linking. Removes unreferenced functions and data, significantly reducing binary size.

These flags work together to produce the smallest possible binary while maintaining performance, critical for embedded systems with limited resources.

### Using PlatformIO
```bash
cd stm32
pio run
```

### Upload
```bash
pio run --target upload
```

The default configuration uses ST-Link programmer for Nucleo boards.

### Configuring Brownout Reset (BOR)

BOR is enabled by default. To check or modify BOR level, use STM32CubeProgrammer:

1. Connect ST-Link
2. Open STM32CubeProgrammer
3. Go to "Option Bytes" tab
4. Check/set BOR_LEV (Brownout Level):
   - **Level 0**: BOR disabled (not recommended)
   - **Level 1**: ~1.8V threshold
   - **Level 2**: ~2.1V threshold  
   - **Level 3**: ~2.4V threshold (default for most chips)
   - **Level 4**: ~2.7V threshold

For 3.3V operation, Level 1, 2, or 3 are appropriate.

## Clock Configuration
- **System Clock**: MSI at 2.097 MHz (low power, suitable for 3.3V operation)
- **RTC Clock**: LSI (~37 kHz ±5% tolerance)
- **Voltage Scale**: Range 1 (1.8V - suitable for MSI up to 4.2 MHz)
- **RTC Wake-up**: Dynamically configured based on BPM (uses ck_spre or ck_spre/16)

**Note**: The LSI oscillator has a typical ±5% frequency tolerance. For applications requiring precise timing, consider using an external 32.768 kHz crystal (LSE) for the RTC clock source. The MSI clock at 2.097 MHz is well within safe operating limits for 3.3V operation.

## Debouncing
Software debouncing with 200ms delay prevents multiple triggers from button bouncing.

## EXTI Configuration
External interrupts are configured for:
- **EXTI13**: Increase BPM button (PC13)
- **EXTI0**: Decrease BPM button (PB0)
- **EXTI1**: Reserved button (PB1)

All configured for falling edge detection (button press).

## Customization
- Modify pin definitions at the top of `main.cpp` to change GPIO assignments
- Adjust `BPM_MIN`, `BPM_MAX`, `BPM_DEFAULT`, and `BPM_STEP` for different BPM range and step size
- Adjust `ACTIVATION_DURATION_MS` for different pulse width
- Adjust `DEBOUNCE_DELAY_MS` for different debounce sensitivity
- Change board in `platformio.ini` for different STM32L0 variants

## How It Works
1. System starts at default BPM (100)
2. RTC wake-up timer is configured based on BPM
3. For high BPM (>60), RTC wakes at activation rate using ck_spre/16 (~16Hz)
4. For low BPM (≤60), RTC wakes every second and software tracks activation timing
5. Button presses adjust BPM and dynamically reconfigure the RTC wake-up timer
6. Between wake-ups, system enters Stop mode for minimum power consumption
7. After wake-up from Stop mode, system clock is automatically reconfigured

## Supported Boards
- Nucleo-L053R8 (default)
- Nucleo-L031K6
- Discovery-L053C8
- Other STM32L0 series boards (may require pin adjustments)

All STM32L0 series chips operate reliably at 3.3V with appropriate clock speeds.

## Notes
- After wake-up from Stop mode, the system clock is reconfigured
- RTC continues running in Stop mode for wake-up timing
- The implementation uses CMSIS framework for direct register access and maximum power efficiency
