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

typedef enum {
    BUTTON_RED = 0,
    BUTTON_YELLOW
} button_id_t;

typedef struct {
    button_id_t button;
} button_event_t;

typedef struct {
    bool enabled;
} led_cmd_t;

QueueHandle_t xQueueButtonEvents;
QueueHandle_t xQueueLedRedCmd;
QueueHandle_t xQueueLedYellowCmd;

static void btn_callback(uint gpio, uint32_t events) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    button_event_t event;

    if ((events & GPIO_IRQ_EDGE_FALL) == 0) {
        return;
    }

    if (gpio == BTN_PIN_R) {
        event.button = BUTTON_RED;
        xQueueSendFromISR(xQueueButtonEvents, &event, &xHigherPriorityTaskWoken);
    }

    if (gpio == BTN_PIN_Y) {
        event.button = BUTTON_YELLOW;
        xQueueSendFromISR(xQueueButtonEvents, &event, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void button_task(void* p) {
    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);

    gpio_init(BTN_PIN_Y);
    gpio_set_dir(BTN_PIN_Y, GPIO_IN);
    gpio_pull_up(BTN_PIN_Y);

    gpio_set_irq_enabled_with_callback(BTN_PIN_R, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);
    gpio_set_irq_enabled(BTN_PIN_Y, GPIO_IRQ_EDGE_FALL, true);

    bool red_enabled = false;
    bool yellow_enabled = false;
    button_event_t event;
    led_cmd_t cmd;

    while (true) {
        if (xQueueReceive(xQueueButtonEvents, &event, portMAX_DELAY) == pdTRUE) {
            if (event.button == BUTTON_RED) {
                red_enabled = !red_enabled;
                cmd.enabled = red_enabled;
                xQueueOverwrite(xQueueLedRedCmd, &cmd);
            }

            if (event.button == BUTTON_YELLOW) {
                yellow_enabled = !yellow_enabled;
                cmd.enabled = yellow_enabled;
                xQueueOverwrite(xQueueLedYellowCmd, &cmd);
            }
        }
    }
}

void led_red_task(void* p) {
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);
    gpio_put(LED_PIN_R, 0);

    bool enabled = false;
    led_cmd_t cmd;

    while (true) {
        if (xQueueReceive(xQueueLedRedCmd, &cmd, 0) == pdTRUE) {
            enabled = cmd.enabled;
        }

        if (!enabled) {
            gpio_put(LED_PIN_R, 0);
            if (xQueueReceive(xQueueLedRedCmd, &cmd, pdMS_TO_TICKS(20)) == pdTRUE) {
                enabled = cmd.enabled;
            }
            continue;
        }

        gpio_put(LED_PIN_R, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_put(LED_PIN_R, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void led_yellow_task(void* p) {
    gpio_init(LED_PIN_Y);
    gpio_set_dir(LED_PIN_Y, GPIO_OUT);
    gpio_put(LED_PIN_Y, 0);

    bool enabled = false;
    led_cmd_t cmd;

    while (true) {
        if (xQueueReceive(xQueueLedYellowCmd, &cmd, 0) == pdTRUE) {
            enabled = cmd.enabled;
        }

        if (!enabled) {
            gpio_put(LED_PIN_Y, 0);
            if (xQueueReceive(xQueueLedYellowCmd, &cmd, pdMS_TO_TICKS(20)) == pdTRUE) {
                enabled = cmd.enabled;
            }
            continue;
        }

        gpio_put(LED_PIN_Y, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_put(LED_PIN_Y, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}



int main() {
    stdio_init_all();

    xQueueButtonEvents = xQueueCreate(8, sizeof(button_event_t));
    xQueueLedRedCmd = xQueueCreate(1, sizeof(led_cmd_t));
    xQueueLedYellowCmd = xQueueCreate(1, sizeof(led_cmd_t));

    xTaskCreate(button_task, "BTN_Task", 256, NULL, 2, NULL);
    xTaskCreate(led_red_task, "LED_R_Task", 256, NULL, 1, NULL);
    xTaskCreate(led_yellow_task, "LED_Y_Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while(1){}

    return 0;
}