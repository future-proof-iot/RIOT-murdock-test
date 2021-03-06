/*
 * Copyright 2016, Imagination Technologies Limited and/or its
 *                 affiliated group companies.
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

 /**
  * @ingroup     cpu_mips32r2_common
  * @ingroup     drivers_periph_timer
  * @{
  *
  * @file
  * @brief       Implementation of the low-level timer driver
  *
  * @}
  */

#include <assert.h>
#include <mips/cpu.h>
#include <mips/m32c0.h>
#include <mips/regdef.h>
#include <mips/asm.h>
#include <string.h>

#include <periph/timer.h>
#include "cpu_conf.h"
#include <stdio.h>
#include "sched.h"
#include "thread.h"
#include "board.h"
#include "irq.h"
#include "timex.h"
#include "div.h"
#include <sys/time.h>

#include "eic.h"

/*
 * setting TIMER_ACCURACY_SHIFT lower will improve accuracy
 * at the cost of more regular interrupts (hence less power efficient).
 * */
#define TIMER_ACCURACY_SHIFT (10)
#define TIMER_ACCURACY       (1 << TIMER_ACCURACY_SHIFT)
#define CHANNELS             (3)

/* TICKS_PER_US must be set in the board file */
#ifndef TICKS_PER_US
#error "Please set TICK_PER_US in your board file"
#endif

/*
 * The base MIPS count / compare timer is fixed frequency at core clock / 2
 * and is pretty basic This timer is currently only supported in Vectored
 * Interrupt Mode (VI), EIC mode is partially supported in non-vectored mode
 * only.
 *
 * RIOT's xtimer expects the timer to operate at 1MHZ or any 2^n multiple or
 * factor of this, thus we maintain a software timer which counts at 1MHz.
 * This is not particularly power efficient and may add latency too.
 *
 * If other SoC specific timers are available which are more flexible then
 * it is advised to use them, this timer is a fallback version
 * that should work on all MIPS implementations.
 *
 */

static timer_isr_ctx_t timer_isr_ctx;
volatile unsigned int counter;
volatile unsigned int compares[CHANNELS];

static void timer_isr(void)
{
    IFS0CLR =_IFS0_CTIF_MASK;

    uint32_t status = irq_disable();
    counter += TIMER_ACCURACY;
    irq_restore(status);

    if (counter == compares[0]) {
        /*
         * The Xtimer code expects the ISR to take some time
         * but our counter is a fake software one, so bump it a
         * bit to give the impression some time elapsed in the ISR.
         * Without this the callback ( _shoot(timer) on xtimer_core.c )
         * never fires.
         */
        counter += TIMER_ACCURACY;
        timer_isr_ctx.cb(timer_isr_ctx.arg, 0);
        mips32r2_isr_end();
    }
    if (counter == compares[1]) {
        timer_isr_ctx.cb(timer_isr_ctx.arg, 1);
        mips32r2_isr_end();
    }
    if (counter == compares[2]) {
        timer_isr_ctx.cb(timer_isr_ctx.arg, 2);
        mips32r2_isr_end();
    }

    mips_setcompare(mips_getcount() + TICKS_PER_US * TIMER_ACCURACY);
}

/*
 * The mips toolchain C library does not implement gettimeofday()
 *
 * implement it here using the timer.
 *
 */
int gettimeofday(struct timeval *__restrict __p, void *__restrict __tz)
{
    (void)__tz;

    uint64_t now = counter * US_PER_MS;
    __p->tv_sec = div_u64_by_1000000(now);
    __p->tv_usec = now - (__p->tv_sec * US_PER_SEC);

    return 0;
}

int timer_init(tim_t dev, uint32_t freq, timer_cb_t cb, void *arg)
{
    assert(dev == 0);

    (void)dev;
    (void)freq; /* Cannot adjust Frequency */

    timer_isr_ctx.cb = cb;
    timer_isr_ctx.arg = arg;

    /* Clear down soft counters */
    memset((void *)compares, 0, sizeof(compares));

    counter = (1 << TIMER_ACCURACY_SHIFT);

    /* Set compare up */
    mips_setcompare(mips_getcount() + TICKS_PER_US * TIMER_ACCURACY);

    /* Start the timer if stopped */
    mips32_bc_c0(C0_CAUSE, CR_DC);

    /* Enable Timer Interrupts */
    set_external_isr_cb(_CORE_TIMER_VECTOR, timer_isr);
    eic_configure_priority(_CORE_TIMER_VECTOR, 1, 0);
    eic_enable(_CORE_TIMER_VECTOR);

    return 0;
}

int timer_set(tim_t dev, int channel, unsigned int timeout)
{
    assert(dev == 0);
    assert(channel < CHANNELS);

    (void)dev;

    timeout >>= TIMER_ACCURACY_SHIFT;
    timeout <<= TIMER_ACCURACY_SHIFT;

    uint32_t status = irq_disable();
    compares[channel] = counter + timeout;
    irq_restore(status);

    return 0;
}

int timer_set_absolute(tim_t dev, int channel, unsigned int value)
{
    assert(dev == 0);
    assert(channel < CHANNELS);

    (void)dev;

    value >>= TIMER_ACCURACY_SHIFT;
    value <<= TIMER_ACCURACY_SHIFT;

    uint32_t status = irq_disable();
    compares[channel] = value;
    irq_restore(status);

    return 0;
}

int timer_clear(tim_t dev, int channel)
{
    assert(dev == 0);
    assert(channel < CHANNELS);

    (void)dev;

    uint32_t status = irq_disable();
    compares[channel] = 0;
    irq_restore(status);

    return 0;
}

unsigned int timer_read(tim_t dev)
{
    assert(dev == 0);

    (void)dev;

    return counter;
}

void timer_start(tim_t dev)
{
    (void)dev;
    mips32_bc_c0(C0_CAUSE, CR_DC);
}

void timer_stop(tim_t dev)
{
    (void)dev;
    mips32_bs_c0(C0_CAUSE, CR_DC);
}
