# ATTiny1616 Implementation

## Overview
This implementation provides a low-power adjustable BPM (beats per minute) pin activation system for the ATTiny1616 microcontroller.

## Features
- **Adjustable BPM**: Pin PA3 is activated at adjustable rate (40-155 BPM, default 100 BPM)
- **Button Controls**: 
  - **PB0**: Increase BPM by 5 (50ms debounce for snappy response)
  - **PB1**: Decrease BPM by 5 (50ms debounce for snappy response)
  - **PB2**: Reserved for future use
- **Low Power Mode**: Uses Power-Down sleep mode between activations
- **RTC Wake-up**: Real-Time Counter (RTC) provides precise timing and wake-up (dynamically reconfigured)
- **Watchdog Timer**: 8-second timeout for system reliability, runs in all sleep modes without extra power
- **Button Interrupts**: 3 buttons with interrupt-driven input and 50ms software debouncing
- **Power Optimization**: 
  - ADC disabled
  - Analog Comparator disabled
  - Power-Down sleep mode (lowest power consumption)
  - Run-standby enabled for RTC

## 3.3V Operation
- **Operating Voltage**: ATTiny1616 operates at 1.8V - 5.5V, fully compatible with 3.3V
- **Brownout Detection (BOD)**: 
  - Internal BOD is available and can be configured via fuses
  - **Recommended Setting**: BOD level at 2.6V for reliable 3.3V operation
  - BOD can be set using UPDI programming tools (e.g., pymcuprog, avrdude)
  - Protects against unstable operation during power supply fluctuations

## Hardware Configuration

### Output Pin
- **PA3**: Output pin for periodic activation

### Button Pins (Active Low with Pull-ups)
- **PB0**: Increase BPM button
- **PB1**: Decrease BPM button
- **PB2**: Reserved button (future use)

## BPM Configuration
- **Range**: 40 - 155 BPM
- **Default**: 100 BPM
- **Step Size**: ±5 BPM per button press
- **Activation Duration**: 50ms high pulse per beat

## Power Consumption
The ATTiny1616 in Power-Down mode with RTC and Watchdog running:
- Typical: ~1-2 µA
- Active mode: Brief periods during pin activation
- Watchdog adds negligible power consumption (<1 µA)

## 50ms Output Pulse Implementation
The 50ms output pulse uses a blocking delay (`_delay_ms()`). This approach is optimal because:
- **Power Efficient**: Keeping a hardware timer running during sleep would consume more power than the brief 50ms wake period
- **Simple & Reliable**: No timer configuration or interrupt handling needed
- **RTC Continues**: The RTC continues running during the delay, maintaining accurate BPM timing
- **Watchdog Safe**: Watchdog is serviced before and after sleep, so the 50ms delay doesn't cause issues

Alternative timer-based approaches would require keeping timers active during sleep mode, increasing baseline power consumption.

## Building

### Build Flags Explained

The `platformio.ini` file contains compiler and linker flags that optimize the code for size and performance:

#### Compiler Flags
- **`-Os`**: Optimize for size. Reduces code size while maintaining good performance, crucial for microcontrollers with limited flash memory.
- **`-DATTINY1616`**: Defines a preprocessor macro identifying the target chip. Can be used for conditional compilation.
- **`-ffunction-sections`**: Places each function in its own section in the object file. Enables the linker to remove unused functions.
- **`-fdata-sections`**: Places each data variable in its own section. Enables the linker to remove unused data.
- **`-flto`**: Enables Link-Time Optimization (LTO). Allows the compiler to optimize across translation units, resulting in smaller and faster code.

#### Linker Flags
- **`-Wl,--gc-sections`**: Instructs the linker to garbage-collect unused sections. Removes functions and data that are never referenced, significantly reducing final binary size.

These flags work together to minimize flash memory usage while maintaining optimal performance for low-power operation.

### Using PlatformIO
```bash
cd attiny
pio run
```

### Upload
```bash
pio run --target upload
```

Make sure you have the correct programmer configured in `platformio.ini`. Default is serialupdi.

### Configuring Brownout Detection (BOD)

To set BOD level for 3.3V operation, use pymcuprog or similar:

```bash
# Set BOD to 2.6V (recommended for 3.3V operation)
pymcuprog write -t uart -u /dev/ttyUSB0 -d attiny1616 --fuses 5:0x02
```

Or with avrdude:
```bash
avrdude -c serialupdi -p t1616 -U fuse5:w:0x02:m
```

## Watchdog Timer
The implementation uses an 8-second watchdog timeout. The watchdog:
- Runs in all sleep modes without additional power consumption
- Automatically resets the system if the main loop hangs
- Is serviced (reset) before each sleep cycle
- Provides system reliability without affecting low-power operation

## Debouncing
Software debouncing with 50ms delay provides snappy, responsive button feedback. The millisecond counter tracks approximate time for accurate debouncing independent of BPM settings.

## Customization
- Modify `OUTPUT_PIN` to change the output pin
- Modify `BUTTON_*_PIN` definitions to change button pins
- Adjust `BPM_MIN`, `BPM_MAX`, `BPM_DEFAULT`, and `BPM_STEP` for different BPM range and step size
- Adjust `ACTIVATION_DURATION_MS` for different pulse width
- Adjust `DEBOUNCE_DELAY_MS` for different debounce sensitivity

## How It Works
1. System starts at default BPM (100)
2. RTC period is calculated based on BPM: `Period = 60000ms / BPM`
3. RTC wakes the system at each period to activate the output pin
4. Button presses adjust BPM and dynamically reconfigure the RTC
5. Between activations, system enters Power-Down sleep mode for minimum power consumption
