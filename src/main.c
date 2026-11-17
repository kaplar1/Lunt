/* LuntNode main: boot sequence and the consumer side of the data flow:
Button 0 ISR --sem--> sensor thread --msgq--> main thread --> log, LED

Boot sequece:
1.) Log version and MCUboot image state
2.) Self-test the sensor
3.) Pass -> confirm the image so MCUboot keeps it
	Fail on a test image -> reboot, MCUboot reverts to the old image
	Fail on a confirmed image -> keep running and keep retrying
4.) Start the sensor thread
*/

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <app_version.h>

#include "app_types.h"
#include "conversions.h"
#include "dfu_confirm.h"
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

static void boot_selftest_and_confirm(void)
{
	dfu_log_state();

	if (sensor_selftest()) {

		if (sensor_selftest()) {
			(void)dfu_confirm_image();
			return;
		}

		if (dfu_revert_pending()) {
			LOG_ERR("Self-test FAILED on a test image. Not confirming.");
			LOG_ERR("Rebooting in %d s;MCUboot will restore the previous image.",
				CONFIG_APP_SELFTEST_REBOOT_DELAY_SEC);
			k_sleep(K_SECONDS(CONFIG_APP_SELFTEST_REBOOT_DELAY_SEC));
			sys_reboot(SYS_REBOOT_COLD);
		}
		/*
		Confirmed (or debugger-flashed) image with nothing to revert to:
		rebooting would only loop. Run degraded and keep trying.
		LOG_ERR("Self-test FAILED. No previous image to revert to; running degraded.");
		*/
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
	boot_selftest_and_confirm();
	sensor_task_start();

	while (true) {
		struct lunt_sample s;

		k_msgq_get(&sample_msgq, &s, K_FOREVER);
		log_sample(&s);
		led_blink();
	}
	return 0;
}
