/*
 * Aether Sensor Node Firmware
 *
 * Copyright (c) 2022 Ian Wallace <ian.wallace@knights.ucf.edu>
 *            Paul Wood <>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Zephyr Headers */
#include <device.h>
#include <drivers/sensor.h>
#include <drivers/sensor/zmod4510.h>
#include <kernel.h>
#include <logging/log.h>
#include <lorawan/lorawan.h>
#include <zephyr.h>
#include <cayenne.h>
#include <drivers/gpio.h>
#include <pm/pm.h>
#include <pm/device.h>
#include <pm/device_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "pm.h"


LOG_MODULE_REGISTER(aether);


#define BME_STACK_SIZE    1024
#define ZMOD_STACK_SIZE   1024
#define SPS_STACK_SIZE     2048
#define LORA_STACK_SIZE   2048
#define USB_STACK_SIZE    1024

#define SENSOR_THREAD_STACK_SIZE 2048
#define PM_SENSOR_THREAD_STACK_SIZE 2048

#define SENSOR_PRIORITY 5
#define PM_SENSOR_PRIORITY 5

#define BME_PRIORITY    5
#define ZMOD_PRIORITY   5
#define SPS_PRIORITY     5
#define LORA_PRIORITY   5
#define USB_PRIORITY    5


// extern void zmod_entry_point(void *_msgq, void *arg2, void *arg3);
// extern void pm_sensor_thread(void *_msgq, void *arg2, void *arg3);
// extern void bme_entry_point(void *_msgq, void *arg2, void *arg3);
extern void lora_entry_point(void *_msgq, void *arg2, void *arg3);

extern void pm_sensor_thread(void*, void*, void*);
extern void sensor_thread(void*, void*, void*);

static struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

#ifdef CONFIG_PM
#define USB_DETECT_DEBOUNCE K_MSEC(10)

static struct gpio_dt_spec usb_detect = GPIO_DT_SPEC_GET(DT_NODELABEL(usb_wakeup), gpios);
static struct gpio_callback usb_detect_cb_data;
static const struct device *pwr_5v_domain = DEVICE_DT_GET(DT_NODELABEL(pwr_5v_domain));
void usb_debounce_power_set(struct k_work *work);
void usb_debounce_timer_handler(struct k_timer *timer);

K_TIMER_DEFINE(usb_debounce_timer, usb_debounce_timer_handler, NULL);
K_WORK_DEFINE(usb_debounce_work, usb_debounce_power_set);
#endif /* CONFIG_PM */


K_THREAD_DEFINE(sensor_tid, SENSOR_THREAD_STACK_SIZE,
                sensor_thread, NULL, NULL, NULL,
                SENSOR_PRIORITY, 0, 1000);

K_THREAD_DEFINE(pm_sensor_tid, PM_SENSOR_THREAD_STACK_SIZE,
                pm_sensor_thread, NULL, NULL, NULL,
                PM_SENSOR_PRIORITY, 0, 1000);

#ifdef CONFIG_LORAWAN
K_THREAD_DEFINE(lora_tid, LORA_STACK_SIZE,
                lora_entry_point, NULL, NULL, NULL,
                LORA_PRIORITY, 0, 1000);
#endif /* CONFIG_LORAWAN */


int init_status_led() {
  int ret = gpio_pin_configure_dt(&led, GPIO_DISCONNECTED);
  if (ret) {
    LOG_ERR("Error %d: failed to configure pin %d\n", ret, led.pin);
    return -EINVAL;
  }
  return 0;
}

#ifdef CONFIG_PM
static int handler_count = 0;

void usb_debounce_timer_handler(struct k_timer *timer) {
  k_work_submit(&usb_debounce_work);
}

void usb_debounce_power_set(struct k_work *work) {
  if (gpio_pin_get_dt(&usb_detect)) {
    disable_sleep();
    printk("usb inserted %d\n", pm_constraint_get(PM_STATE_SUSPEND_TO_IDLE));
  }
  else {
    enable_sleep();
    printk("usb removed %d\n", pm_constraint_get(PM_STATE_SUSPEND_TO_IDLE));
  }
}

void usb_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
  handler_count++;
  k_timer_start(&usb_debounce_timer, USB_DETECT_DEBOUNCE, K_NO_WAIT);
  printk("handler_count=%d\n", handler_count);
}

int init_usb_detect() {
  int ret;

  if (!pm_device_wakeup_enable((struct device *) usb_detect.port, true)) {
    LOG_ERR("Error: failed to enabled wakeup on %s pin %d\n",
        usb_detect.port->name,
        usb_detect.pin);
    return -EINVAL;
  }

  ret = gpio_pin_configure_dt(&usb_detect, GPIO_INPUT);
  if (ret) {
    LOG_ERR("Error %d: failed to configure pin %d\n", ret, usb_detect.pin);
    return -EINVAL;
  }

  ret = gpio_pin_interrupt_configure_dt(&usb_detect, GPIO_INT_EDGE_BOTH);
  if (ret) {
    LOG_ERR("Error %d: failed to configure interupt on %s pin %d\n", 
        ret, 
        usb_detect.port->name, 
        usb_detect.pin);
    return -EINVAL;
  }

  gpio_init_callback(&usb_detect_cb_data, usb_handler, BIT(usb_detect.pin));
  gpio_add_callback(usb_detect.port, &usb_detect_cb_data);

  gpio_port_value_t portb;
  if (gpio_port_get_raw(usb_detect.port, &portb)) {
    printk("unable to read portb\n");
    return -EINVAL;
  }
  LOG_INF("port=%x\n", portb);
#ifdef CONFIG_PM
  LOG_INF("usb_detect pin=%d\n", gpio_pin_get_dt(&usb_detect));
  if (gpio_pin_get_dt(&usb_detect)) {
    LOG_INF("disabling sleep %d\n", pm_constraint_get(PM_STATE_SUSPEND_TO_IDLE));
    disable_sleep();
  }
  else {
    printk("keeping sleep on\n");
  }
#endif /* CONFIG_PM */

  return 0;
}

#endif /* CONFIG_PM */


int pre_kernel2_init(const struct device *dev) {
  ARG_UNUSED(dev);
#ifdef CONFIG_PM
  disable_sleep();
#endif /* CONFIG_PM */

  return 0;
}

SYS_INIT(pre_kernel2_init, PRE_KERNEL_2, 0);

#define PWR_5V_DOMAIN DT_NODELABEL(pwr_5v_domain);


void main() 
{
  enable_sleep();
  LOG_INF("Entering main thread. Sleep enabled? %d", pm_constraint_get(PM_STATE_SUSPEND_TO_IDLE));

  // (ノಠ益ಠ)ノ彡┻━┻
  // It causes the I2C bus to lose arbitration??????????
  if (init_status_led()) {
    LOG_ERR("Unable to initialize status led");
    return;
  }
#ifdef CONFIG_PM
  if (init_usb_detect()) {
    LOG_ERR("Unable to initialize usb detect");
    return;
  }
#endif /* CONFIG_PM */


#ifdef CONFIG_THREAD_MONITOR
#ifdef CONFIG_LORAWAN
  k_thread_name_set(lora_tid, "lora");
#endif /* CONFIG_LORAWAN */
#endif /* CONFIG_THREAD_MONITOR */
}
