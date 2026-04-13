/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define BTN_PIN_R 28
#define BTN_PIN_Y 21
#define LED_PIN_R 5
#define LED_PIN_Y 10
#define BUTTON_RED 0
#define BUTTON_YELLOW 1

QueueHandle_t xQueueBtn;
SemaphoreHandle_t xSemaphoreLedR;
SemaphoreHandle_t xSemaphoreLedY;

static void btn_callback(uint gpio, uint32_t events) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t button_id;

    if ((events & GPIO_IRQ_EDGE_FALL) == 0) {
        return;
    }

    if (gpio == BTN_PIN_R) {
        button_id = BUTTON_RED;
        xQueueSendFromISR(xQueueBtn, &button_id, &xHigherPriorityTaskWoken);
    }

    if (gpio == BTN_PIN_Y) {
        button_id = BUTTON_YELLOW;
        xQueueSendFromISR(xQueueBtn, &button_id, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void button_task(void* p) {
    (void) p;

    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);

    gpio_init(BTN_PIN_Y);
    gpio_set_dir(BTN_PIN_Y, GPIO_IN);
    gpio_pull_up(BTN_PIN_Y);

    gpio_set_irq_enabled_with_callback(BTN_PIN_R, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);
    gpio_set_irq_enabled(BTN_PIN_Y, GPIO_IRQ_EDGE_FALL, true);

    uint8_t button_id;

    while (true) {
        if (xQueueReceive(xQueueBtn, &button_id, portMAX_DELAY) == pdTRUE) {
            if (button_id == BUTTON_RED) {
                xSemaphoreGive(xSemaphoreLedR);
            }

            if (button_id == BUTTON_YELLOW) {
                xSemaphoreGive(xSemaphoreLedY);
            }
        }
    }
}

void led_red_task(void* p) {
    (void) p;

    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);
    gpio_put(LED_PIN_R, 0);

    bool enabled = false;
    bool led_state = false;

    while (true) {
        if (!enabled) {
            gpio_put(LED_PIN_R, 0);
            led_state = false;

            if (xSemaphoreTake(xSemaphoreLedR, portMAX_DELAY) == pdTRUE) {
                enabled = !enabled;
            }

            continue;
        }

        if (xSemaphoreTake(xSemaphoreLedR, pdMS_TO_TICKS(100)) == pdTRUE) {
            enabled = false;
            gpio_put(LED_PIN_R, 0);
            led_state = false;
            continue;
        }

        led_state = !led_state;
        gpio_put(LED_PIN_R, led_state);
    }
}

void led_yellow_task(void* p) {
    (void) p;

    gpio_init(LED_PIN_Y);
    gpio_set_dir(LED_PIN_Y, GPIO_OUT);
    gpio_put(LED_PIN_Y, 0);

    bool enabled = false;
    bool led_state = false;

    while (true) {
        if (!enabled) {
            gpio_put(LED_PIN_Y, 0);
            led_state = false;

            if (xSemaphoreTake(xSemaphoreLedY, portMAX_DELAY) == pdTRUE) {
                enabled = !enabled;
            }

            continue;
        }

        if (xSemaphoreTake(xSemaphoreLedY, pdMS_TO_TICKS(100)) == pdTRUE) {
            enabled = false;
            gpio_put(LED_PIN_Y, 0);
            led_state = false;
            continue;
        }

        led_state = !led_state;
        gpio_put(LED_PIN_Y, led_state);
    }
}



int main() {
    stdio_init_all();

    xQueueBtn = xQueueCreate(8, sizeof(uint8_t));
    xSemaphoreLedR = xSemaphoreCreateBinary();
    xSemaphoreLedY = xSemaphoreCreateBinary();

    xTaskCreate(button_task, "BTN_Task", 256, NULL, 2, NULL);
    xTaskCreate(led_red_task, "LED_R_Task", 256, NULL, 1, NULL);
    xTaskCreate(led_yellow_task, "LED_Y_Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while(1){}

    return 0;
}