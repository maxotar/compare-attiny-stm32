# ATTiny1616 Implementation

## Overview
This implementation provides a low-power adjustable BPM (beats per minute) pin activation system for the ATTiny1616 microcontroller.

## Features
- **Adjustable BPM**: Pin PA3 is activated at adjustable rate (40-155 BPM, default 100 BPM)
- **Button Controls**: 
  - **PB0**: Increase BPM by 5
  - **PB1**: Decrease BPM by 5
  - **PB2**: Reserved for future use
- **Low Power Mode**: Uses Power-Down sleep mode between activations
- **RTC Wake-up**: Real-Time Counter (RTC) provides precise timing and wake-up (dynamically reconfigured)
- **Button Interrupts**: 3 buttons with interrupt-driven input and software debouncing
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
The ATTiny1616 in Power-Down mode with RTC running:
- Typical: ~1-2 µA
- Active mode: Brief periods during pin activation

**Note**: The current implementation uses `_delay_ms()` for the 50ms activation period, which is a blocking delay. For maximum power efficiency in production code, consider using a hardware timer to handle the 50ms duration and return to sleep immediately after setting the pin high.

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

## Debouncing
## Debouncing
Software debouncing prevents multiple triggers from button bouncing. The debounce delay is 3 RTC periods, which varies with BPM setting (approximately 1.8-3 seconds depending on rate).

## Customization
- Modify `OUTPUT_PIN` to change the output pin
- Modify `BUTTON_*_PIN` definitions to change button pins
- Adjust `BPM_MIN`, `BPM_MAX`, `BPM_DEFAULT`, and `BPM_STEP` for different BPM range and step size
- Adjust `ACTIVATION_DURATION_MS` for different pulse width
- Adjust `DEBOUNCE_DELAY` for different debounce sensitivity (in RTC periods)

## How It Works
1. System starts at default BPM (100)
2. RTC period is calculated based on BPM: `Period = 60000ms / BPM`
3. RTC wakes the system at each period to activate the output pin
4. Button presses adjust BPM and dynamically reconfigure the RTC
5. Between activations, system enters Power-Down sleep mode for minimum power consumption
