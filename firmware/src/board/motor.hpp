/****************************************************************************
*
*   Copyright (C) 2016  Zubax Robotics  <info@zubax.com>
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in
*    the documentation and/or other materials provided with the
*    distribution.
* 3. Neither the name PX4 nor the names of its contributors may be
*    used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
* ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
****************************************************************************/

#pragma once

#include <cstdint>
#include <math/math.hpp>
#include <zubax_chibios/util/heapless.hpp>


namespace board
{
/**
 * This namespace contains API to the motor control hardware - PWM, ADC, Driver IC, etc.
 * All functions except init() may be invoked from IRQ context.
 */
namespace motor
{
/**
 * After initialization the driver will enter the inactive mode.
 */
void init();

/**
 * Activates/deactivates the power stage hardware.
 * Must be activated before the motor can be started.
 * Must be deactivated after the motor is stopped.
 * In inactive mode, all current measurements will be reported as zero.
 * The default state is deactivated.
 */
void setActive(bool active);

/**
 * @ref setActive().
 */
bool isActive();

/**
 * This function can be invoked to perform zero offset calibration.
 * It must be guaranteed that during such calibration the motor is NOT spinning,
 * and that no other component will be using the driver while the calibration is in progress.
 * The driver activity state will be restored upon completion of the calibration process.
 * See also @ref isCalibrationInProgress().
 */
void beginCalibration();

/**
 * Always returns false unless @ref beginCalibration() was invoked recently.
 */
bool isCalibrationInProgress();

/**
 * Meaningful results guaranteed only after initialization.
 * @return PWM carrier period in seconds.
 */
float getPWMPeriod();

/**
 * Meaningful results guaranteed only after initialization.
 * @return PWM dead time in seconds.
 */
float getPWMDeadTime();

/**
 * Returns the power stage voltage, aka VBAT, in volts.
 */
float getInverterVoltage();

/**
 * This function must not be called if the driver is not active; see @ref setActive().
 * @param abc           PWM values per channel in the range [0, 1].
 */
void setPWM(const math::Vector<3>& abc);

/**
 * Immediately deactivates the PWM outputs (shuts down the carrier).
 * Further use of the driver may not be possible.
 * This function can be called from ANY context, e.g. from Hard Fault handler.
 */
void emergency();

/**
 * @ref getStatus().
 */
struct Status
{
    float inverter_temperature = 0.0F;          ///< Kelvin
    float inverter_voltage = 0.0F;              ///< Volt

    math::Vector<2> phase_current_zero_offset;  ///< Volt

    bool power_ok = false;                      ///< PWRGD
    bool overload = false;                      ///< OCTW
    bool fault = false;                         ///< FAULT

    bool isOkay() const
    {
        const bool bad = (!power_ok) || overload || fault;
        return !bad;
    }

    auto toString() const
    {
        return os::heapless::format("Inverter Temperature: %.0f C\n"
                                    "Inverter Voltage    : %.1f\n"
                                    "Current ADC Z-Offset: %.3f, %.3f\n"
                                    "Power OK            : %u\n"
                                    "Overload            : %u\n"
                                    "Fault               : %u\n",
                                    double(math::convertKelvinToCelsius(inverter_temperature)),
                                    double(inverter_voltage),
                                    double(phase_current_zero_offset[0]), double(phase_current_zero_offset[1]),
                                    power_ok,
                                    overload,
                                    fault);
    }
};

/**
 * Returns immediate status information. @ref Status.
 */
Status getStatus();

/**
 * This external handler is invoked from the SECOND HIGHEST PRIORITY IRQ context shortly after the middle of every
 * N-th PWM period. The N is defined by the driver. This handler is preemptible by the fast IRQ only.
 *
 * @param period                        Equals N * @ref getPWMPeriod(), in seconds.
 * @param phase_currents_ab             Instant currents of phases A and B, in Amperes.
 * @param inverter_voltage              Low-pass filtered VBUS voltage of the inverter, in Volts.
 */
extern void handleMainIRQ(const float period,
                          const math::Vector<2>& phase_currents_ab,
                          const float inverter_voltage);

/**
 * This external handler is invoked from the HIGHEST PRIORITY IRQ context at some fixed point of each PWM period.
 * The point is chosen in such a way as to make sure that the freshly computed PWM values will be applied on the very
 * next PWM period, without any additional latency.
 * This IRQ preempts every other process and IRQ handler in the system.
 *
 * @param period                        Equals @ref getPWMPeriod(), in seconds.
 */
extern void handleFastIRQ(const float period);

/**
 * This critical section disables ALL maskable IRQ, including the motor control ones.
 * This is unlike os::CriticalSectionLocker, which keeps them enabled.
 * WARNING: This locker does not restore the last state, it just blindly enables/disables.
 * RTFM: http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0553a/CHDBIBGJ.html
 */
typedef class AbsoluteCriticalSectionLockerImpl_
{
public:
    AbsoluteCriticalSectionLockerImpl_()
    {
        /*
         * Making sure there's no nesting going on.
         * Nesting is not allowed because motor critical sections are extremely expensive.
         */
        assert(__get_PRIMASK() == 0);

        __disable_irq();
    }

    ~AbsoluteCriticalSectionLockerImpl_()
    {
        __enable_irq();
    }
} volatile AbsoluteCriticalSectionLocker;

}
}
