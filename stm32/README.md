# STM32L0 Implementation

## Overview
This implementation provides a low-power adjustable BPM (beats per minute) pin activation system for the STM32L0 series microcontroller (tested with STM32L053R8).

## Features
- **Adjustable BPM**: Pin PA5 is activated at adjustable rate (40-155 BPM, default 100 BPM)
- **Button Controls**:
  - **PC13**: Increase BPM by 5 (Blue button on Nucleo board, true 50ms debounce)
  - **PB0**: Decrease BPM by 5 (true 50ms debounce)
  - **PB1**: Reserved for future use
- **Low Power Mode**: Uses Stop mode with voltage regulator in low power mode
- **RTC Wake-up**: Real-Time Clock with external 32.768kHz crystal for precise timing (±20 ppm accuracy)
- **Independent Watchdog**: ~7 second timeout, runs in Stop mode without extra power consumption
- **Button Interrupts**: 3 buttons with EXTI interrupt-driven input and blocking 50ms debounce
- **Power Optimization**:
  - MSI clock (2.097 MHz) for low power operation
  - Voltage scaling to Range 1 (1.8V)
  - Stop mode with LP voltage regulator
  - Debug disabled in low power modes
  - GPIO configured for low speed

## Timing Accuracy
- **External Crystal**: ±20 ppm typical (±0.002% accuracy)
- **Expected BPM drift**: ±0.001 BPM at 60 BPM setting
- **Alternative**: Internal LSI oscillator available (±5% accuracy, ±3 BPM drift at 60 BPM)

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

### External Crystal (Required)
- **32.768kHz Crystal**: Connected to OSC32_IN/OSC32_OUT pins (PC14/PC15)
- **Load Capacitors**: Typically 6-12pF on each crystal pin to ground (check crystal datasheet)
- **Purpose**: Provides precise timing with ±20 ppm typical accuracy
- **Note**: Many Nucleo boards have the crystal pre-installed
- **Alternative**: Internal LSI oscillator (~37kHz, ±5% accuracy) can be used by changing `RCC_CSR_RTCSEL_LSE` to `RCC_CSR_RTCSEL_LSI` in code

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
The STM32L0 in Stop mode with RTC and IWDG running:
- Typical: ~1-2 µA with RTC running
- Active mode: Brief periods during pin activation and wake-up
- MSI clock keeps power consumption low during active periods
- IWDG (Independent Watchdog) adds negligible power consumption in Stop mode

## 50ms Output Pulse Implementation
The 50ms output pulse uses a blocking delay. This approach is optimal because:
- **Power Efficient**: Running a hardware timer during Stop mode would prevent entering the deepest sleep state
- **Simple & Reliable**: No complex timer configuration or additional interrupts needed
- **IWDG Compatible**: The Independent Watchdog continues running in Stop mode, providing reliability without extra power
- **RTC Continues**: Wake-up timing remains accurate

Alternative timer-based approaches would require keeping timers active, significantly increasing power consumption compared to the brief 50ms wake period.

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

## Watchdog Timer
The implementation uses the Independent Watchdog (IWDG) with ~7 second timeout:
- Runs on LSI (Low Speed Internal oscillator)
- Continues operating in Stop mode without additional power consumption
- Automatically resets the system if the main loop hangs
- Serviced (reloaded) before each sleep cycle and in the main loop
- Provides system reliability without affecting low-power operation

## Clock Configuration
- **System Clock**: MSI at 2.097 MHz (low power, suitable for 3.3V operation)
- **RTC Clock**: LSI (~37 kHz ±5% tolerance)
- **Voltage Scale**: Range 1 (1.8V - suitable for MSI up to 4.2 MHz)
- **RTC Wake-up**: Dynamically configured based on BPM (uses ck_spre or ck_spre/16)

**Note**: The LSI oscillator has a typical ±5% frequency tolerance. For applications requiring precise timing, consider using an external 32.768 kHz crystal (LSE) for the RTC clock source. The MSI clock at 2.097 MHz is well within safe operating limits for 3.3V operation.

## Debouncing
True 50ms software debouncing using blocking delay:
- Button interrupt sets a flag
- Main loop processes the flag with 50ms delay
- Reads button state after delay to confirm press
- Waits for button release with additional debouncing
- **Power impact**: Minimal - stays awake ~100-150ms per button press
- Since button presses are infrequent (1-10 every few minutes), average power consumption remains <2µA

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
