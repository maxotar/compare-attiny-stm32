/**
 * ATTiny1616 - Adjustable BPM Pin Activation with Low Power Mode
 * 
 * Features:
 * - Activates a pin at adjustable BPM rate (40-155 BPM) for 50ms
 * - Button controls: PB0=Increase BPM, PB1=Decrease BPM (±5 BPM steps)
 * - Low power sleep mode between activations
 * - RTC for wake-up timing (dynamically reconfigured)
 * - 3 button inputs with interrupt and debouncing
 * 
 * 3.3V Operation: ATTiny1616 operates at 1.8-5.5V, fully compatible with 3.3V
 * Brownout Detection: Internal BOD can be configured via fuses (recommended: 2.6V for 3.3V operation)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
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

// Debouncing
#define DEBOUNCE_DELAY 3  // Debounce delay in RTC periods (3 * period = ~1.8-3s depending on BPM)
volatile uint32_t button_inc_last_interrupt_time = 0;
volatile uint32_t button_dec_last_interrupt_time = 0;
volatile uint32_t button3_last_interrupt_time = 0;

volatile bool activation_flag = false;
volatile uint32_t rtc_counter = 0;  // Counts RTC periods
volatile bool reconfigure_rtc = false;

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
    
    // Select 32.768kHz external crystal or internal oscillator
    RTC.CLKSEL = RTC_CLKSEL_INT32K_gc; // Use internal 32kHz oscillator
    
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
void activate_output() {
    PORTA.OUTSET = OUTPUT_PIN;  // Set pin high
    _delay_ms(ACTIVATION_DURATION_MS);
    PORTA.OUTCLR = OUTPUT_PIN;  // Set pin low
}

// Enter sleep mode
void enter_sleep() {
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

// Button interrupt handler
ISR(PORTB_PORT_vect) {
    uint8_t flags = PORTB.INTFLAGS;
    PORTB.INTFLAGS = flags;  // Clear interrupt flags
    
    uint32_t current_time = rtc_counter;
    
    // Button Increase BPM (PB0)
    if ((flags & BUTTON_INC_PIN) && 
        (current_time - button_inc_last_interrupt_time > DEBOUNCE_DELAY)) {
        button_inc_last_interrupt_time = current_time;
        
        if (current_bpm < BPM_MAX) {
            current_bpm += BPM_STEP;
            if (current_bpm > BPM_MAX) {
                current_bpm = BPM_MAX;
            }
            reconfigure_rtc = true;
        }
    }
    
    // Button Decrease BPM (PB1)
    if ((flags & BUTTON_DEC_PIN) && 
        (current_time - button_dec_last_interrupt_time > DEBOUNCE_DELAY)) {
        button_dec_last_interrupt_time = current_time;
        
        if (current_bpm > BPM_MIN) {
            current_bpm -= BPM_STEP;
            if (current_bpm < BPM_MIN) {
                current_bpm = BPM_MIN;
            }
            reconfigure_rtc = true;
        }
    }
    
    // Button 3 (PB2) - Reserved for future use
    if ((flags & BUTTON3_PIN) && 
        (current_time - button3_last_interrupt_time > DEBOUNCE_DELAY)) {
        button3_last_interrupt_time = current_time;
        // Reserved for future functionality
    }
}

int main(void) {
    // Disable unused peripherals to save power
    // Turn off ADC
    ADC0.CTRLA &= ~ADC_ENABLE_bm;
    
    // Turn off AC (Analog Comparator)
    AC0.CTRLA &= ~AC_ENABLE_bm;
    
    // Initialize peripherals
    output_pin_init();
    button_init();
    rtc_init();
    
    // Enable global interrupts
    sei();
    
    // Main loop
    while (1) {
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
