/*
 * Copyright (C) 2021 Luo Jia (HUST IoT Security Lab)
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     boards_alientek-pandora
 * @{
 *
 * @file
 * @brief       Board specific implementations for the Alientek Pandora board
 *
 * @author      Luo Jia <luojia@hust.edu.cn>
 *
 * @}
 */

#include "board.h"
#include "periph/gpio.h"

void board_init(void)
{
    /* initialize LEDs */
    gpio_init(LED0_PIN, GPIO_OUT);
    gpio_init(LED1_PIN, GPIO_OUT);
    gpio_init(LED2_PIN, GPIO_OUT);
}
