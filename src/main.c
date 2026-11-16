/* Main is now only a consumer side
Button 0 ISR --sem--> sensor thread --msgq--> main thread --> log, LED */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <app_version.h>

#include "app_types.h"
#include "conversions.h"
#include "sensor_task.h"

LOG_MODULE_REGISTER(Lunt, CONFIG_APP_LOG_LEVEL);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static struct gpio_callback button_cb;

static void led_off_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	gpio_pin_set_dt(&led, 0);
}
static K_WORK_DELAYABLE_DEFINE(led_off_work, led_off_handler);

static void led_blink(void)
{
	if (led.port == NULL) {
		if (led.port == NULL) {
			return;
		}
		gpio_pin_set_dt(&led, 1);
		k_work_reschedule(&led_off_work, K_MSEC(50));
	}
}

static void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	sensor_task_trigger();
}

static void io_init(void)
{
	if (led.port && gpio_is_ready_dt(&led)) {
		gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	}

	if (button.port && gpio_is_ready_dt(&button)) {
		gpio_pin_configure_dt(&button, GPIO_INPUT);
		gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
		gpio_init_callback(&button_cb, button_isr, BIT(button.pin));
		gpio_add_callback(button.port, &button_cb);
		LOG_INF("Press Button 0 for an immediate reading");
	}
}
static void log_sample(const struct lunt_sample *s)
{
	char t[16], h[16], p[16];

	format_centi(t, sizeof(t), s->temp_centi_c);
	format_centi(h, sizeof(h), s->hum_centi_pct);
	/*deci-Pa / 10 = centi-hPa, printed as hPA with 2 decimals*/
	format_centi(p, sizeof(p), s->press_deci_pa / 10);

	LOG_INF("T=%s C RH=%s %% P=%s hPa", t, h, p);
}

int main(void)
{
	LOG_INF("Lunt: v%s booting on %s", APP_VERSION_STRING, CONFIG_BOARD_TARGET);

	io_init();
	sensor_task_start();

	while (true) {
		struct lunt_sample s;

		k_msgq_get(&sample_msgq, &s, K_FOREVER);
		log_sample(&s);
		led_blink();
	}
	return 0;
}
