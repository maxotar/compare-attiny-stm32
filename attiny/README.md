# ATTiny1616 Implementation

## Overview
This implementation provides a low-power periodic pin activation system for the ATTiny1616 microcontroller.

## Features
- **Periodic Activation**: Pin PA3 is activated 100 times per minute (every 600ms) for 50ms
- **Low Power Mode**: Uses Power-Down sleep mode between activations
- **RTC Wake-up**: Real-Time Counter (RTC) provides precise timing and wake-up
- **Button Interrupts**: 3 buttons with interrupt-driven input and software debouncing
- **Power Optimization**: 
  - ADC disabled
  - Analog Comparator disabled
  - Power-Down sleep mode (lowest power consumption)
  - Run-standby enabled for RTC

## Hardware Configuration

### Output Pin
- **PA3**: Output pin for periodic activation

### Button Pins (Active Low with Pull-ups)
- **PB0**: Button 1
- **PB1**: Button 2
- **PB2**: Button 3

## Pin Activation Timing
- **Frequency**: 100 times per minute
- **Period**: 600ms between activations
- **Active Duration**: 50ms high, then low

## Power Consumption
The ATTiny1616 in Power-Down mode with RTC running:
- Typical: ~1-2 µA
- Active mode: Brief periods during pin activation

## Building

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

## Debouncing
Software debouncing with 50ms delay prevents multiple triggers from button bouncing.

## Customization
- Modify `OUTPUT_PIN` to change the output pin
- Modify `BUTTON*_PIN` definitions to change button pins
- Adjust `ACTIVATION_PERIOD_MS` and `ACTIVATION_DURATION_MS` for different timing
- Adjust `DEBOUNCE_DELAY_MS` for different debounce sensitivity

## Button Event Handlers
Add your custom code in the `ISR(PORTB_PORT_vect)` interrupt handler where indicated:
```cpp
// Handle button 1 press
// Add your button 1 action here
```
