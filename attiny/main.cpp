/**
 * ATTiny1616 - Adjustable BPM Pin Activation with Low Power Mode
 * 
 * Features:
 * - Activates a pin at adjustable BPM rate (40-155 BPM) for 50ms
 * - Button controls: PB0=Increase BPM, PB1=Decrease BPM (±5 BPM steps)
 * - Low power sleep mode between activations
 * - RTC for wake-up timing (dynamically reconfigured) with external 32.768kHz crystal
 * - 3 button inputs with interrupt and proper 50ms blocking debounce
 * - Watchdog timer (8s timeout) for system reliability without affecting sleep
 * 
 * Hardware Requirements:
 * - External 32.768kHz crystal connected to TOSC1/TOSC2 (PA0/PA1) for precise timing
 * - Crystal provides ±20 ppm typical accuracy vs ±3% for internal oscillator
 * 
 * Debouncing: Uses blocking delay approach - stays awake for 50ms after button press
 * to properly debounce. Since button presses are infrequent (1-10 every few minutes),
 * this has minimal impact on average power consumption.
 * 
 * 50ms Output Pulse: Uses blocking _delay_ms() during activation. This approach
 * is more power efficient than keeping a timer running during sleep. The brief
 * 50ms wake period has minimal impact on overall power consumption.
 * 
 * 3.3V Operation: ATTiny1616 operates at 1.8-5.5V, fully compatible with 3.3V
 * Brownout Detection: Internal BOD can be configured via fuses (recommended: 2.6V for 3.3V operation)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>

// Pin configuration
#define OUTPUT_PIN PIN3_bm  // PA3 - Output pin for periodic activation
#define BUTTON_INC_PIN PIN0_bm // PB0 - Button to increase BPM
#define BUTTON_DEC_PIN PIN1_bm // PB1 - Button to decrease BPM
#define BUTTON3_PIN PIN2_bm    // PB2 - Button 3 (reserved for future use)

// BPM configuration
#define BPM_MIN 40
#define BPM_MAX 155
#define BPM_DEFAULT 100
#define BPM_STEP 5

// Timing configuration
volatile uint16_t current_bpm = BPM_DEFAULT;
volatile uint16_t activation_period_ms = 60000 / BPM_DEFAULT;  // Calculate period from BPM
#define ACTIVATION_DURATION_MS 50 // Active for 50ms

// Debouncing - using blocking delay for true 50ms debounce
#define DEBOUNCE_DELAY_MS 50  // 50ms debounce for snappy response

volatile bool activation_flag = false;
volatile uint32_t rtc_counter = 0;  // Counts RTC periods
volatile bool reconfigure_rtc = false;

// Button press flags set by ISR, processed in main loop
volatile bool button_inc_pressed = false;
volatile bool button_dec_pressed = false;
volatile bool button3_pressed = false;

// Calculate RTC period based on BPM
uint16_t calculate_rtc_period(uint16_t bpm) {
    // BPM = beats per minute, period in ms = 60000 / BPM
    // 32768 Hz / 32 = 1024 Hz (approximately 1ms per tick with prescaler)
    // RTC ticks = period_ms * 1.024
    uint32_t period_ms = 60000UL / bpm;
    return (uint16_t)(period_ms * 1024 / 1000);
}

// Initialize RTC for periodic wake-up
void rtc_init() {
    // Disable RTC during configuration
    while (RTC.STATUS > 0);
    
    // Select 32.768kHz external crystal for precise timing (±20 ppm typical)
    // External crystal connected to TOSC1/TOSC2 pins (PA0/PA1)
    // For internal oscillator (±3% accuracy), use: RTC_CLKSEL_INT32K_gc
    RTC.CLKSEL = RTC_CLKSEL_TOSC32K_gc; // Use external 32.768kHz crystal
    
    // Set period based on current BPM
    RTC.PER = calculate_rtc_period(current_bpm);
    
    // Enable periodic interrupt
    RTC.INTCTRL = RTC_OVF_bm;
    
    // Enable RTC with prescaler 32
    RTC.CTRLA = RTC_PRESCALER_DIV32_gc | RTC_RTCEN_bm | RTC_RUNSTDBY_bm;
}

// Update RTC period when BPM changes
void update_rtc_period() {
    // Disable RTC
    RTC.CTRLA = 0;
    while (RTC.STATUS > 0);
    
    // Update period
    RTC.PER = calculate_rtc_period(current_bpm);
    activation_period_ms = 60000 / current_bpm;
    
    // Re-enable RTC
    RTC.CTRLA = RTC_PRESCALER_DIV32_gc | RTC_RTCEN_bm | RTC_RUNSTDBY_bm;
}

// Initialize output pin
void output_pin_init() {
    PORTA.DIRSET = OUTPUT_PIN;  // Set as output
    PORTA.OUTCLR = OUTPUT_PIN;  // Start low
}

// Initialize button pins with interrupts
void button_init() {
    // Configure buttons as inputs with pull-up
    PORTB.DIRCLR = BUTTON_INC_PIN | BUTTON_DEC_PIN | BUTTON3_PIN;
    
    // Enable pull-ups and falling edge interrupts
    PORTB.PIN0CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;  // Increase BPM
    PORTB.PIN1CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;  // Decrease BPM
    PORTB.PIN2CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;  // Reserved
}

// Activate output pin for specified duration
// Note: Uses blocking delay (_delay_ms). This keeps the MCU awake during the 50ms pulse.
// The RTC continues running during this time, so timing accuracy is maintained.
// Alternative timer-based approach would require keeping a timer running during sleep,
// which increases power consumption more than this brief wake period.
void activate_output() {
    PORTA.OUTSET = OUTPUT_PIN;  // Set pin high
    _delay_ms(ACTIVATION_DURATION_MS);  // Blocking delay for 50ms pulse
    PORTA.OUTCLR = OUTPUT_PIN;  // Set pin low
}

// Enter sleep mode
void enter_sleep() {
    // Reset watchdog before sleeping
    wdt_reset();
    
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sei();  // Enable interrupts
    sleep_cpu();
    sleep_disable();
}

// RTC overflow interrupt - triggers based on BPM setting
ISR(RTC_CNT_vect) {
    RTC.INTFLAGS = RTC_OVF_bm;  // Clear interrupt flag
    activation_flag = true;
    rtc_counter++;
}

// Button interrupt handler - sets flags for main loop processing
ISR(PORTB_PORT_vect) {
    uint8_t flags = PORTB.INTFLAGS;
    PORTB.INTFLAGS = flags;  // Clear interrupt flags
    
    // Set flags for button presses - actual debouncing and processing in main loop
    if (flags & BUTTON_INC_PIN) {
        button_inc_pressed = true;
    }
    
    if (flags & BUTTON_DEC_PIN) {
        button_dec_pressed = true;
    }
    
    if (flags & BUTTON3_PIN) {
        button3_pressed = true;
    }
}

// Process button press with proper debouncing
// Stays awake for 50ms to debounce - acceptable since button presses are infrequent
void process_button_presses() {
    if (button_inc_pressed) {
        button_inc_pressed = false;
        _delay_ms(DEBOUNCE_DELAY_MS);  // Wait for bounce to settle
        
        // Check if button still pressed after debounce
        if (!(PORTB.IN & BUTTON_INC_PIN)) {  // Active low
            if (current_bpm < BPM_MAX) {
                current_bpm += BPM_STEP;
                if (current_bpm > BPM_MAX) {
                    current_bpm = BPM_MAX;
                }
                reconfigure_rtc = true;
            }
            // Wait for button release
            while (!(PORTB.IN & BUTTON_INC_PIN)) {
                _delay_ms(10);
            }
            _delay_ms(DEBOUNCE_DELAY_MS);  // Debounce release
        }
    }
    
    if (button_dec_pressed) {
        button_dec_pressed = false;
        _delay_ms(DEBOUNCE_DELAY_MS);  // Wait for bounce to settle
        
        // Check if button still pressed after debounce
        if (!(PORTB.IN & BUTTON_DEC_PIN)) {  // Active low
            if (current_bpm > BPM_MIN) {
                current_bpm -= BPM_STEP;
                if (current_bpm < BPM_MIN) {
                    current_bpm = BPM_MIN;
                }
                reconfigure_rtc = true;
            }
            // Wait for button release
            while (!(PORTB.IN & BUTTON_DEC_PIN)) {
                _delay_ms(10);
            }
            _delay_ms(DEBOUNCE_DELAY_MS);  // Debounce release
        }
    }
    
    if (button3_pressed) {
        button3_pressed = false;
        _delay_ms(DEBOUNCE_DELAY_MS);  // Wait for bounce to settle
        
        // Check if button still pressed after debounce
        if (!(PORTB.IN & BUTTON3_PIN)) {  // Active low
            // Reserved for future functionality
            
            // Wait for button release
            while (!(PORTB.IN & BUTTON3_PIN)) {
                _delay_ms(10);
            }
            _delay_ms(DEBOUNCE_DELAY_MS);  // Debounce release
        }
    }
}

int main(void) {
    // Disable unused peripherals to save power
    // Turn off ADC
    ADC0.CTRLA &= ~ADC_ENABLE_bm;
    
    // Turn off AC (Analog Comparator)
    AC0.CTRLA &= ~AC_ENABLE_bm;
    
    // Initialize watchdog timer (WDT) for system reliability
    // 8 second timeout - provides protection without affecting sleep mode
    // WDT continues running in all sleep modes on ATTiny1616
    wdt_enable(WDTO_8S);
    
    // Initialize peripherals
    output_pin_init();
    button_init();
    rtc_init();
    
    // Enable global interrupts
    sei();
    
    // Main loop
    while (1) {
        // Reset watchdog timer
        wdt_reset();
        
        // Process any pending button presses with proper debouncing
        process_button_presses();
        
        // Update RTC period if BPM changed
        if (reconfigure_rtc) {
            reconfigure_rtc = false;
            update_rtc_period();
        }
        
        if (activation_flag) {
            activation_flag = false;
            activate_output();
        }
        
        // Enter low power sleep mode
        enter_sleep();
    }
    
    return 0;
}
