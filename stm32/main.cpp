/**
 * STM32L0 - Adjustable BPM Pin Activation with Low Power Mode
 * 
 * Features:
 * - Activates a pin at adjustable BPM rate (40-155 BPM) for 50ms
 * - Button controls: PC13=Increase BPM, PB0=Decrease BPM (±5 BPM steps)
 * - Low power stop mode between activations
 * - RTC for wake-up timing (dynamically reconfigured)
 * - 3 button inputs with EXTI interrupt and debouncing
 * 
 * 3.3V Operation: STM32L0 operates at 1.65-3.6V, fully compatible with 3.3V
 * Brownout Detection: PVD (Programmable Voltage Detector) available, BOR enabled by default
 */

#include "stm32l0xx.h"

// Pin configuration (adjust based on your hardware)
#define OUTPUT_PORT GPIOA
#define OUTPUT_PIN GPIO_PIN_5  // PA5 - Output pin

#define BUTTON_INC_PORT GPIOC
#define BUTTON_INC_PIN GPIO_PIN_13  // PC13 - Button to increase BPM

#define BUTTON_DEC_PORT GPIOB
#define BUTTON_DEC_PIN GPIO_PIN_0   // PB0 - Button to decrease BPM

#define BUTTON3_PORT GPIOB
#define BUTTON3_PIN GPIO_PIN_1   // PB1 - Button 3 (reserved)

// BPM configuration
#define BPM_MIN 40
#define BPM_MAX 155
#define BPM_DEFAULT 100
#define BPM_STEP 5

// Timing configuration
volatile uint16_t current_bpm = BPM_DEFAULT;
volatile uint16_t activation_period_ms = 60000 / BPM_DEFAULT;  // Calculate period from BPM
#define ACTIVATION_DURATION_MS 50 // Active for 50ms
#define DEBOUNCE_DELAY_MS 200  // 200ms debounce delay

volatile bool activation_flag = false;
volatile uint32_t millis_counter = 0;  // Approximate millisecond counter
volatile uint32_t last_activation_time = 0;  // Track last activation
volatile uint32_t button_inc_last_interrupt_time = 0;
volatile uint32_t button_dec_last_interrupt_time = 0;
volatile uint32_t button3_last_interrupt_time = 0;
volatile bool reconfigure_rtc = false;

// System Clock Configuration
void SystemClock_Config(void) {
    // Enable Power Control clock
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    
    // Set voltage scale to range 1 (1.8V) for low power
    PWR->CR |= PWR_CR_VOS_0;
    
    // Use MSI as system clock (2.097 MHz by default for low power)
    RCC->CR |= RCC_CR_MSION;
    while (!(RCC->CR & RCC_CR_MSIRDY));
    
    // Set MSI as system clock
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_MSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_MSI);
}

// GPIO Initialization
void GPIO_Init(void) {
    // Enable GPIO clocks
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN | RCC_IOPENR_GPIOCEN;
    
    // Configure output pin (PA5)
    GPIOA->MODER = (GPIOA->MODER & ~(3U << (5 * 2))) | (1U << (5 * 2)); // Output mode
    GPIOA->OTYPER &= ~(1U << 5);  // Push-pull
    GPIOA->OSPEEDR &= ~(3U << (5 * 2));  // Low speed for power saving
    GPIOA->PUPDR &= ~(3U << (5 * 2));  // No pull-up/pull-down
    GPIOA->ODR &= ~(1U << 5);  // Start low
    
    // Configure button pins as input with pull-up
    // PC13 - Button 1
    GPIOC->MODER &= ~(3U << (13 * 2));  // Input mode
    GPIOC->PUPDR = (GPIOC->PUPDR & ~(3U << (13 * 2))) | (1U << (13 * 2));  // Pull-up
    
    // PB0 - Button 2
    GPIOB->MODER &= ~(3U << (0 * 2));  // Input mode
    GPIOB->PUPDR = (GPIOB->PUPDR & ~(3U << (0 * 2))) | (1U << (0 * 2));  // Pull-up
    
    // PB1 - Button 3
    GPIOB->MODER &= ~(3U << (1 * 2));  // Input mode
    GPIOB->PUPDR = (GPIOB->PUPDR & ~(3U << (1 * 2))) | (1U << (1 * 2));  // Pull-up
}

// Calculate wake-up interval based on BPM
// Since LSI is ~37kHz with prescaler giving 1Hz, we use software timing
// Wake up at a rate faster than needed, check in ISR
uint16_t calculate_wakeup_interval_ms(uint16_t bpm) {
    // Period in ms = 60000 / BPM
    uint32_t period_ms = 60000UL / bpm;
    
    // For very fast BPM, wake more frequently
    // For slower BPM, can wake less frequently
    if (period_ms < 1000) {
        return period_ms;  // Wake at activation rate
    } else {
        return 1000;  // Wake every second for slow rates
    }
}

// RTC Configuration for periodic wake-up
void RTC_Init(void) {
    // Enable PWR clock
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    
    // Enable access to RTC domain
    PWR->CR |= PWR_CR_DBP;
    
    // Enable LSI (Low Speed Internal oscillator)
    RCC->CSR |= RCC_CSR_LSION;
    while (!(RCC->CSR & RCC_CSR_LSIRDY));
    
    // Select LSI as RTC clock source
    RCC->CSR = (RCC->CSR & ~RCC_CSR_RTCSEL) | RCC_CSR_RTCSEL_LSI;
    
    // Enable RTC clock
    RCC->CSR |= RCC_CSR_RTCEN;
    
    // Disable RTC write protection
    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;
    
    // Enter initialization mode
    RTC->ISR |= RTC_ISR_INIT;
    while (!(RTC->ISR & RTC_ISR_INITF));
    
    // Configure RTC prescaler
    // LSI = ~37kHz, set prescaler for 1Hz RTC clock
    RTC->PRER = 0x007F00FF;  // Async = 127, Sync = 255
    
    // Exit initialization mode
    RTC->ISR &= ~RTC_ISR_INIT;
    
    // Configure wake-up timer based on BPM
    // Disable wake-up timer
    RTC->CR &= ~RTC_CR_WUTE;
    while (!(RTC->ISR & RTC_ISR_WUTWF));
    
    // Calculate wake interval
    uint16_t wake_interval_ms = calculate_wakeup_interval_ms(current_bpm);
    activation_period_ms = 60000 / current_bpm;
    
    // Use ck_spre (1Hz) for slower rates, or faster clock for high BPM
    RTC->CR &= ~RTC_CR_WUCKSEL;
    
    if (wake_interval_ms >= 1000) {
        // Use ck_spre (1Hz) - wake every second
        RTC->CR |= (4 << RTC_CR_WUCKSEL_Pos);
        RTC->WUTR = 0;  // Wake every 1 second
    } else {
        // Use ck_spre/16 for faster wake-up (16Hz)
        RTC->CR |= (5 << RTC_CR_WUCKSEL_Pos);
        // Calculate ticks: wake_interval_ms / 62.5ms (16Hz = 62.5ms per tick)
        uint16_t ticks = (wake_interval_ms * 16) / 1000;
        if (ticks < 1) ticks = 1;
        RTC->WUTR = ticks - 1;
    }
    
    // Enable wake-up timer interrupt
    RTC->CR |= RTC_CR_WUTIE;
    RTC->CR |= RTC_CR_WUTE;
    
    // Enable RTC write protection
    RTC->WPR = 0xFF;
    
    // Enable RTC wake-up interrupt in EXTI
    EXTI->IMR |= EXTI_IMR_IM20;  // RTC Wakeup is on EXTI line 20
    EXTI->RTSR |= EXTI_RTSR_RT20;
    
    // Enable RTC wake-up interrupt in NVIC
    NVIC_EnableIRQ(RTC_IRQn);
}

// Update RTC wake-up timer when BPM changes
void RTC_UpdateWakeup(void) {
    // Disable RTC write protection
    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;
    
    // Disable wake-up timer
    RTC->CR &= ~RTC_CR_WUTE;
    while (!(RTC->ISR & RTC_ISR_WUTWF));
    
    // Calculate new wake interval
    uint16_t wake_interval_ms = calculate_wakeup_interval_ms(current_bpm);
    activation_period_ms = 60000 / current_bpm;
    
    // Update clock selection and timer value
    RTC->CR &= ~RTC_CR_WUCKSEL;
    
    if (wake_interval_ms >= 1000) {
        RTC->CR |= (4 << RTC_CR_WUCKSEL_Pos);
        RTC->WUTR = 0;
    } else {
        RTC->CR |= (5 << RTC_CR_WUCKSEL_Pos);
        uint16_t ticks = (wake_interval_ms * 16) / 1000;
        if (ticks < 1) ticks = 1;
        RTC->WUTR = ticks - 1;
    }
    
    // Re-enable wake-up timer
    RTC->CR |= RTC_CR_WUTE;
    
    // Enable RTC write protection
    RTC->WPR = 0xFF;
    
    // Reset timing
    last_activation_time = millis_counter;
}

// EXTI Configuration for button interrupts
void EXTI_Init(void) {
    // Enable SYSCFG clock
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    
    // Configure EXTI for PC13 (Button Increase BPM)
    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~SYSCFG_EXTICR4_EXTI13) | SYSCFG_EXTICR4_EXTI13_PC;
    EXTI->IMR |= EXTI_IMR_IM13;
    EXTI->FTSR |= EXTI_FTSR_FT13;  // Falling edge trigger
    
    // Configure EXTI for PB0 (Button Decrease BPM)
    SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0] & ~SYSCFG_EXTICR1_EXTI0) | SYSCFG_EXTICR1_EXTI0_PB;
    EXTI->IMR |= EXTI_IMR_IM0;
    EXTI->FTSR |= EXTI_FTSR_FT0;  // Falling edge trigger
    
    // Configure EXTI for PB1 (Button 3 - Reserved)
    SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0] & ~SYSCFG_EXTICR1_EXTI1) | SYSCFG_EXTICR1_EXTI1_PB;
    EXTI->IMR |= EXTI_IMR_IM1;
    EXTI->FTSR |= EXTI_FTSR_FT1;  // Falling edge trigger
    
    // Enable EXTI interrupts in NVIC
    NVIC_EnableIRQ(EXTI0_1_IRQn);  // Handles EXTI0 and EXTI1
    NVIC_EnableIRQ(EXTI4_15_IRQn); // Handles EXTI13
}

// Simple delay function (approximate, based on instruction cycles)
// Note: This is not accurate for precise timing. The actual delay depends on
// the system clock (MSI at 2.097 MHz) and optimization level.
// For production, use SysTick or a hardware timer for accurate delays.
void delay_ms(uint32_t ms) {
    // Approximate: at 2.097 MHz, ~2000 cycles per ms
    for (uint32_t i = 0; i < ms * 500; i++) {
        __NOP();
    }
}

// Activate output pin
void activate_output(void) {
    GPIOA->ODR |= (1U << 5);  // Set pin high
    delay_ms(ACTIVATION_DURATION_MS);
    GPIOA->ODR &= ~(1U << 5);  // Set pin low
}

// Enter low power stop mode
void enter_stop_mode(void) {
    // Set voltage regulator to low power mode during stop
    PWR->CR |= PWR_CR_LPSDSR;
    
    // Clear wake-up flag
    PWR->CR |= PWR_CR_CWUF;
    
    // Set SLEEPDEEP bit for Stop mode
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    
    // Enter Stop mode
    __WFI();
    
    // After wake-up, system clock needs to be reconfigured
    SystemClock_Config();
}

// RTC Wake-up Timer interrupt handler
extern "C" void RTC_IRQHandler(void) {
    if (RTC->ISR & RTC_ISR_WUTF) {
        // Clear wake-up timer flag
        RTC->ISR &= ~RTC_ISR_WUTF;
        
        // Clear EXTI flag
        EXTI->PR |= EXTI_PR_PIF20;
        
        // Increment millisecond counter based on wake interval
        uint16_t wake_interval_ms = calculate_wakeup_interval_ms(current_bpm);
        millis_counter += wake_interval_ms;
        
        // Check if it's time to activate based on BPM
        if (millis_counter - last_activation_time >= activation_period_ms) {
            last_activation_time = millis_counter;
            activation_flag = true;
        }
    }
}

// EXTI0-1 interrupt handler (Button Decrease BPM and Button 3)
extern "C" void EXTI0_1_IRQHandler(void) {
    uint32_t current_time = millis_counter;
    
    // Button Decrease BPM on PB0 (EXTI0)
    if (EXTI->PR & EXTI_PR_PIF0) {
        EXTI->PR |= EXTI_PR_PIF0;  // Clear interrupt flag
        
        if (current_time - button_dec_last_interrupt_time > DEBOUNCE_DELAY_MS) {
            button_dec_last_interrupt_time = current_time;
            
            if (current_bpm > BPM_MIN) {
                current_bpm -= BPM_STEP;
                if (current_bpm < BPM_MIN) {
                    current_bpm = BPM_MIN;
                }
                reconfigure_rtc = true;
            }
        }
    }
    
    // Button 3 on PB1 (EXTI1) - Reserved
    if (EXTI->PR & EXTI_PR_PIF1) {
        EXTI->PR |= EXTI_PR_PIF1;  // Clear interrupt flag
        
        if (current_time - button3_last_interrupt_time > DEBOUNCE_DELAY_MS) {
            button3_last_interrupt_time = current_time;
            // Reserved for future functionality
        }
    }
}

// EXTI4-15 interrupt handler (Button Increase BPM)
extern "C" void EXTI4_15_IRQHandler(void) {
    // Button Increase BPM on PC13 (EXTI13)
    if (EXTI->PR & EXTI_PR_PIF13) {
        EXTI->PR |= EXTI_PR_PIF13;  // Clear interrupt flag
        
        uint32_t current_time = millis_counter;
        if (current_time - button_inc_last_interrupt_time > DEBOUNCE_DELAY_MS) {
            button_inc_last_interrupt_time = current_time;
            
            if (current_bpm < BPM_MAX) {
                current_bpm += BPM_STEP;
                if (current_bpm > BPM_MAX) {
                    current_bpm = BPM_MAX;
                }
                reconfigure_rtc = true;
            }
        }
    }
}

int main(void) {
    // Configure system clock for low power
    SystemClock_Config();
    
    // Initialize peripherals
    GPIO_Init();
    RTC_Init();
    EXTI_Init();
    
    // Disable unused peripherals for power saving
    // Disable debugging in low power modes
    DBGMCU->CR = 0;
    
    // Main loop
    while (1) {
        // Update RTC wake-up timer if BPM changed
        if (reconfigure_rtc) {
            reconfigure_rtc = false;
            RTC_UpdateWakeup();
        }
        
        if (activation_flag) {
            activation_flag = false;
            activate_output();
        }
        
        // Enter low power stop mode
        enter_stop_mode();
    }
    
    return 0;
}
