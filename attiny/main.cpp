/**
 * ATTiny1616 - Periodic Pin Activation with Low Power Mode
 * 
 * Features:
 * - Activates a pin 100 times per minute (every 600ms) for 50ms
 * - Low power sleep mode between activations
 * - RTC for wake-up timing
 * - 3 button inputs with interrupt and debouncing
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

// Pin configuration
#define OUTPUT_PIN PIN3_bm  // PA3 - Output pin for periodic activation
#define BUTTON1_PIN PIN0_bm // PB0 - Button 1
#define BUTTON2_PIN PIN1_bm // PB1 - Button 2
#define BUTTON3_PIN PIN2_bm // PB2 - Button 3

// Timing configuration
#define ACTIVATION_PERIOD_MS 600  // 100 times per minute = 600ms period
#define ACTIVATION_DURATION_MS 50 // Active for 50ms

// Debouncing
#define DEBOUNCE_DELAY_MS 50
volatile uint32_t button1_last_interrupt_time = 0;
volatile uint32_t button2_last_interrupt_time = 0;
volatile uint32_t button3_last_interrupt_time = 0;

volatile bool activation_flag = false;
volatile uint32_t millis_counter = 0;

// Initialize RTC for periodic wake-up
void rtc_init() {
    // Disable RTC during configuration
    while (RTC.STATUS > 0);
    
    // Select 32.768kHz external crystal or internal oscillator
    RTC.CLKSEL = RTC_CLKSEL_INT32K_gc; // Use internal 32kHz oscillator
    
    // Set period for 600ms wake-up (100 times per minute)
    // 32768 Hz / 32 = 1024 Hz (1ms per tick with prescaler)
    // 600ms = 614 ticks (approximately)
    RTC.PER = 614;
    
    // Enable periodic interrupt
    RTC.INTCTRL = RTC_OVF_bm;
    
    // Enable RTC with prescaler 32
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
    PORTB.DIRCLR = BUTTON1_PIN | BUTTON2_PIN | BUTTON3_PIN;
    
    // Enable pull-ups
    PORTB.PIN0CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;
    PORTB.PIN1CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;
    PORTB.PIN2CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;
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

// RTC overflow interrupt - triggers every 600ms
ISR(RTC_CNT_vect) {
    RTC.INTFLAGS = RTC_OVF_bm;  // Clear interrupt flag
    activation_flag = true;
    millis_counter++;
}

// Button 1 interrupt
ISR(PORTB_PORT_vect) {
    uint8_t flags = PORTB.INTFLAGS;
    PORTB.INTFLAGS = flags;  // Clear interrupt flags
    
    uint32_t current_time = millis_counter;
    
    // Debounce Button 1
    if ((flags & BUTTON1_PIN) && 
        (current_time - button1_last_interrupt_time > DEBOUNCE_DELAY_MS)) {
        button1_last_interrupt_time = current_time;
        // Handle button 1 press
        // Add your button 1 action here
    }
    
    // Debounce Button 2
    if ((flags & BUTTON2_PIN) && 
        (current_time - button2_last_interrupt_time > DEBOUNCE_DELAY_MS)) {
        button2_last_interrupt_time = current_time;
        // Handle button 2 press
        // Add your button 2 action here
    }
    
    // Debounce Button 3
    if ((flags & BUTTON3_PIN) && 
        (current_time - button3_last_interrupt_time > DEBOUNCE_DELAY_MS)) {
        button3_last_interrupt_time = current_time;
        // Handle button 3 press
        // Add your button 3 action here
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
        if (activation_flag) {
            activation_flag = false;
            activate_output();
        }
        
        // Enter low power sleep mode
        enter_sleep();
    }
    
    return 0;
}
