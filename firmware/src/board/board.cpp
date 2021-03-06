/**
 * Copyright (c) 2016  Zubax Robotics OU  <info@zubax.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 *    following disclaimer in the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "board.hpp"
#include <ch.hpp>
#include <zubax_chibios/util/heapless.hpp>
#include <cstring>
#include <unistd.h>
#include <numeric>

// Making sure that the priority level 0 (highest) is not occupied by the OS.
#ifndef CORTEX_PRIORITY_SVCALL
# error "CORTEX_PRIORITY_SVCALL is not defined, probably this version of ChibiOS/RT is not supported"
#endif
#if CORTEX_PRIORITY_SVCALL < 1
# error "This application must be able to preempt the OS IRQ; current configuration does not allow that"
#endif

// Making sure the OS is configured to never disable the higher priority IRQ.
#ifndef CORTEX_SIMPLIFIED_PRIORITY
# error "CORTEX_SIMPLIFIED_PRIORITY is not defined, probably this version of ChibiOS/RT is not supported"
#endif
#if CORTEX_SIMPLIFIED_PRIORITY
# error "This application requires BASEPRI based critical section management; current configuration is invalid"
#endif

/// PAL setup
const ::PALConfig pal_default_config =
{
#if STM32_HAS_GPIOA
    {VAL_GPIOA_MODER,VAL_GPIOA_OTYPER,VAL_GPIOA_OSPEEDR,VAL_GPIOA_PUPDR,VAL_GPIOA_ODR,VAL_GPIOA_AFRL,VAL_GPIOA_AFRH},
#endif
#if STM32_HAS_GPIOB
    {VAL_GPIOB_MODER,VAL_GPIOB_OTYPER,VAL_GPIOB_OSPEEDR,VAL_GPIOB_PUPDR,VAL_GPIOB_ODR,VAL_GPIOB_AFRL,VAL_GPIOB_AFRH},
#endif
#if STM32_HAS_GPIOC
    {VAL_GPIOC_MODER,VAL_GPIOC_OTYPER,VAL_GPIOC_OSPEEDR,VAL_GPIOC_PUPDR,VAL_GPIOC_ODR,VAL_GPIOC_AFRL,VAL_GPIOC_AFRH},
#endif
#if STM32_HAS_GPIOD
    {VAL_GPIOD_MODER,VAL_GPIOD_OTYPER,VAL_GPIOD_OSPEEDR,VAL_GPIOD_PUPDR,VAL_GPIOD_ODR,VAL_GPIOD_AFRL,VAL_GPIOD_AFRH},
#endif
#if STM32_HAS_GPIOE
    {VAL_GPIOE_MODER,VAL_GPIOE_OTYPER,VAL_GPIOE_OSPEEDR,VAL_GPIOE_PUPDR,VAL_GPIOE_ODR,VAL_GPIOE_AFRL,VAL_GPIOE_AFRH},
#endif
#if STM32_HAS_GPIOF
    {VAL_GPIOF_MODER,VAL_GPIOF_OTYPER,VAL_GPIOF_OSPEEDR,VAL_GPIOF_PUPDR,VAL_GPIOF_ODR,VAL_GPIOF_AFRL,VAL_GPIOF_AFRH},
#endif
#if STM32_HAS_GPIOG
    {VAL_GPIOG_MODER,VAL_GPIOG_OTYPER,VAL_GPIOG_OSPEEDR,VAL_GPIOG_PUPDR,VAL_GPIOG_ODR,VAL_GPIOG_AFRL,VAL_GPIOG_AFRH},
#endif
#if STM32_HAS_GPIOH
    {VAL_GPIOH_MODER,VAL_GPIOH_OTYPER,VAL_GPIOH_OSPEEDR,VAL_GPIOH_PUPDR,VAL_GPIOH_ODR,VAL_GPIOH_AFRL,VAL_GPIOH_AFRH}
#endif
};

/// Provided by linker
const extern std::uint8_t DeviceSignatureStorage[];

namespace board
{
namespace
{

static void initLEDPWM()
{
    {
        os::CriticalSectionLocker cs;
        // Power-on and reset
        RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
        RCC->APB1RSTR |=  RCC_APB1RSTR_TIM3RST;
        RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM3RST;
    }

    TIM3->ARR = 0xFFFF;
    TIM3->CR1 = 0;
    TIM3->CR2 = 0;

    // CC2, CC3, CC4 are R, G, B. Inverted mode.
    TIM3->CCMR1 = TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1;
    TIM3->CCMR2 = TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1 |
                  TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1;

    // All enabled, all inverted.
    TIM3->CCER = TIM_CCER_CC4E | TIM_CCER_CC3E | TIM_CCER_CC2E |
                 TIM_CCER_CC4P | TIM_CCER_CC3P | TIM_CCER_CC2P;

    // Start
    TIM3->EGR = TIM_EGR_UG | TIM_EGR_COMG;
    TIM3->CR1 |= TIM_CR1_CEN;
}

}

os::watchdog::Timer init(unsigned watchdog_timeout_msec,
                         os::config::IStorageBackend& cfg_backend)
{
    /*
     * OS initialization first
     */
    halInit();
    chibios_rt::System::init();

    /*
     * Serial port
     */
    sdStart(&STDOUT_SD, nullptr);

    /*
     * Watchdog
     */
    os::watchdog::init();
    os::watchdog::Timer wdt;
    wdt.startMSec(watchdog_timeout_msec);

    /*
     * Indication
     */
    initLEDPWM();

    /*
     * Configuration manager
     */
    const int config_init_res = os::config::init(&cfg_backend);
    if (config_init_res < 0)
    {
        die(config_init_res);
    }

    /*
     * Prompt
     */
    os::lowsyslog(PRODUCT_NAME_STRING " %d.%d.%08x / %d %s\n",
                  FW_VERSION_MAJOR, FW_VERSION_MINOR, GIT_HASH, config_init_res,
                  watchdogTriggeredLastReset() ? "WDTRESET" : "OK");
    return wdt;
}

__attribute__((noreturn))
void die(int reason)
{
    chibios_rt::System::halt(os::heapless::intToString(reason));
    while (1)
    {
    }
}

void restart()
{
    NVIC_SystemReset();
}

UniqueID readUniqueID()
{
    UniqueID bytes;
    std::fill(bytes.begin(), bytes.end(), 0);
    std::memcpy(bytes.data(), reinterpret_cast<const void*>(0x1FFF7A10), 12);
    return bytes;
}

bool tryReadDeviceSignature(DeviceSignature& out_sign)
{
    std::memcpy(out_sign.data(), &DeviceSignatureStorage[0], std::tuple_size<DeviceSignature>::value);
    for (auto x : out_sign)
    {
        if (x != 0xFF && x != 0x00)          // All 0xFF/0x00 is not a valid signature, it's empty storage
        {
            return true;
        }
    }
    return false;
}

HardwareVersion detectHardwareVersion()
{
    auto v = HardwareVersion();

    v.major = HW_VERSION;
    v.minor = 0;                // Some detection will be added in future versions

    return v;
}

void setRGBLED(const RGB& rgb)
{
    constexpr unsigned Multiplier = 0xFFFF;

    TIM3->CCR2 = unsigned(rgb[0] * Multiplier + 0.4F);
    TIM3->CCR3 = unsigned(rgb[1] * Multiplier + 0.4F);
    TIM3->CCR4 = unsigned(rgb[2] * Multiplier + 0.4F);
}

} // namespace board

/*
 * Early init from ChibiOS
 */
extern "C"
{

void __early_init(void)
{
    stm32_clock_init();

    /*
     * Making sure all peripherals that could be used by the bootloader are reset or disabled!
     * The bootloader is expected to shut everything down, but one can never be too safe.
     */
    // CAN
    RCC->APB1RSTR |=  (RCC_APB1RSTR_CAN1RST | RCC_APB1RSTR_CAN2RST);
    RCC->APB1RSTR &= ~(RCC_APB1RSTR_CAN1RST | RCC_APB1RSTR_CAN2RST);

    CAN1->IER = CAN2->IER = 0;                                  // Disable interrupts
    CAN1->MCR = CAN2->MCR = CAN_MCR_SLEEP | CAN_MCR_RESET;      // Software reset

    NVIC_ClearPendingIRQ(CAN1_RX0_IRQn);
    NVIC_ClearPendingIRQ(CAN1_RX1_IRQn);
    NVIC_ClearPendingIRQ(CAN1_TX_IRQn);
    NVIC_ClearPendingIRQ(CAN1_SCE_IRQn);

    NVIC_ClearPendingIRQ(CAN2_RX0_IRQn);
    NVIC_ClearPendingIRQ(CAN2_RX1_IRQn);
    NVIC_ClearPendingIRQ(CAN2_TX_IRQn);
    NVIC_ClearPendingIRQ(CAN2_SCE_IRQn);

    // USB
    RCC->AHB2RSTR |=  RCC_AHB2RSTR_OTGFSRST;
    RCC->AHB2RSTR &= ~RCC_AHB2RSTR_OTGFSRST;

    NVIC_ClearPendingIRQ(OTG_FS_IRQn);
    NVIC_ClearPendingIRQ(OTG_FS_WKUP_IRQn);
}

void boardInit(void)
{
}

void fastLowLevelSystemIntegrityCheckHook()
{
    /*
     * Ensuring that the hard real-time IRQ processing is still configured correctly.
     * Remember that these IRQ must never be disabled, except by the FOC module itself,
     * which in turn must never access the RTOS services on its own.
     */
    ASSERT_ALWAYS((__get_PRIMASK() & 1) == 0);
    ASSERT_ALWAYS((__get_FAULTMASK() & 1) == 0);

    /*
     * ChibiOS is one of the very few open source RTOS that are fully preemptible and support tickless mode.
     *
     * Fully preemptible means that the OS never disables IRQ whose priority is higher than a certain
     * pre-configured level; therefore these IRQ can be used to perform hard real time tasks with zero latency/jitter
     * imposed by the RTOS. In this application we make heavy use of this feature.
     *
     * A tickless mode is another neat feature that enables the OS to use a free-running counter with programmable
     * alarms which trigger IRQ only when needed, rather than triggering a system tick IRQ at a fixed rate even when
     * it's not needed. I won't go into detail here, this feature is well described in the documentation.
     *
     * So, we have two neat features that have a major caveat: when used together, they turn the RTOS into a fucking
     * minefield. When the RTOS configures a new deadline on the system timer, it basically performs the following
     * steps:
     *
     *  1. Read the current system timer value.
     *  2. Add the time remaining to the deadline.
     *  3. Write the result into the compare register.
     *
     * See, the timer keeps running the whole time the sequence is executed, so it is possible that by the time we
     * reach the step 3, the value obtained at the step 1 is obsolete. Obviously, the designer of the RTOS has taken
     * that into account, and put in place two safeguards:
     *
     *  - The minimum configurable delay is limited, typically to just 2 timer ticks. So if the timer increments in the
     *    process, it's fine. The minimum is defined by the configuration parameter CH_CFG_ST_TIMEDELTA.
     *
     *  - The process is protected by the critical section, which makes it quasiatomic.
     *
     * Now remember that we have hard real time IRQ that can interrupt the RTOS at any moment.
     * See the problem? Took me 2 days...
     *
     * If the OS is interrupted between the steps 1 and 3, it may configure a deadline that is already in the past.
     * The IRQ will never fire until the system timer overflows, which may take days. Until that time, the RTOS will
     * not receive any tick interrupts, which will halt a lot of its functions. For example, if a thread goes to sleep,
     * it will never wake up (until the system timer overflows). Worse yet, if a thread is locked on a non-time related
     * event, such as arrival of data via some communication interface or whatever, it will continue to function
     * correctly, blowing the developer's mind even further.
     *
     * The check below verifies that the deadline is always configured in the future, otherwise it crashes the system.
     *
     * Read the discussion here: http://www.chibios.com/forum/viewtopic.php?f=3&t=3651
     */
#if CH_CFG_ST_TIMEDELTA > 0
    {
        using TimeType = decltype(st_lld_get_counter());
        static constexpr TimeType HalfRange = std::numeric_limits<TimeType>::max() / 2;
        static constexpr TimeType DetectionThreshold = S2ST(2);

        const TimeType counter = st_lld_get_counter();
        const TimeType real_alarm = st_lld_get_alarm();
        const TimeType alarm_with_offset = real_alarm + DetectionThreshold;

        if (TimeType(alarm_with_offset - counter) >= HalfRange)
        {
            chibios_rt::System::halt(os::heapless::concatenate(
                "OS TIMER DEADLINE MISSED: CNT=", counter, " ALARM=", real_alarm
                ).c_str());
        }
    }
#endif
}

}
