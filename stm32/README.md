# STM32L0 Implementation

## Overview
This implementation provides a low-power periodic pin activation system for the STM32L0 series microcontroller (tested with STM32L053R8).

## Features
- **Periodic Activation**: Pin PA5 is activated 100 times per minute (every 600ms) for 50ms
- **Low Power Mode**: Uses Stop mode with voltage regulator in low power mode
- **RTC Wake-up**: Real-Time Clock wake-up timer provides timing and wake-up from Stop mode
- **Button Interrupts**: 3 buttons with EXTI interrupt-driven input and software debouncing
- **Power Optimization**:
  - MSI clock (2.097 MHz) for low power operation
  - Voltage scaling to Range 1 (1.8V)
  - Stop mode with LP voltage regulator
  - Debug disabled in low power modes
  - GPIO configured for low speed

## Hardware Configuration

### Output Pin
- **PA5**: Output pin for periodic activation (can use LED on Nucleo board)

### Button Pins (Active Low with Pull-ups)
- **PC13**: Button 1 (Blue button on Nucleo board)
- **PB0**: Button 2
- **PB1**: Button 3

## Pin Activation Timing
- **Frequency**: 100 times per minute
- **Period**: 600ms between activations
- **Active Duration**: 50ms high, then low

## Power Consumption
The STM32L0 in Stop mode:
- Typical: ~1-2 µA with RTC running
- Active mode: Brief periods during pin activation and wake-up
- MSI clock keeps power consumption low during active periods

**Note**: The current implementation uses a busy-wait loop for the 50ms activation period. For maximum power efficiency in production code, consider using a hardware timer (e.g., TIM2) to handle the 50ms duration more efficiently.

## Building

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

## Clock Configuration
- **System Clock**: MSI at 2.097 MHz (low power)
- **RTC Clock**: LSI (~37 kHz)
- **Voltage Scale**: Range 1 (1.8V)

## Debouncing
Software debouncing with 50ms delay prevents multiple triggers from button bouncing.

## EXTI Configuration
External interrupts are configured for:
- **EXTI13**: Button 1 (PC13)
- **EXTI0**: Button 2 (PB0)
- **EXTI1**: Button 3 (PB1)

All configured for falling edge detection (button press).

## Customization
- Modify pin definitions at the top of `main.cpp` to change GPIO assignments
- Adjust `ACTIVATION_PERIOD_MS` and `ACTIVATION_DURATION_MS` for different timing
- Adjust `DEBOUNCE_DELAY_MS` for different debounce sensitivity
- Change board in `platformio.ini` for different STM32L0 variants

## Button Event Handlers
Add your custom code in the interrupt handlers where indicated:
```cpp
// Handle button 1 press
// Add your button 1 action here
```

## Supported Boards
- Nucleo-L053R8 (default)
- Nucleo-L031K6
- Discovery-L053C8
- Other STM32L0 series boards (may require pin adjustments)

## Notes
- After wake-up from Stop mode, the system clock is reconfigured
- RTC continues running in Stop mode for wake-up timing
- The implementation uses CMSIS framework for direct register access and maximum power efficiency
