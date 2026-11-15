/*Accessing LED and button through devicetree aliases, and the first
interrupt-to-thread handoff
device tree name LED0 -> "led0" Button 0 -> "sw0"
*/

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <app_version.h>

#include "app_types.h"
#include "conversions.h"

LOG_MODULE_REGISTER(Lunt, LOG_LEVEL_INF);

static const struct device *const lunt_dev = DEVICE_DT_GET(DT_ALIAS(luntsensor));
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static struct gpio_callback button_cb;

static K_SEM_DEFINE(wake_sem, 0, 1);

static void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	k_sem_give(&wake_sem);
}

static int read_sample(struct lunt_sample *out)
{
	struct sensor_value temp, hum, press;
	int rc = sensor_sample_fetch(lunt_dev);

	if (rc == 0) {
		rc = sensor_channel_get(lunt_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	}
	if (rc == 0) {
		rc = sensor_channel_get(lunt_dev, SENSOR_CHAN_HUMIDITY, &hum);
	}
	if (rc == 0) {
		rc = sensor_channel_get(lunt_dev, SENSOR_CHAN_PRESS, &press);
	}
	if (rc) {
		LOG_ERR("Sensor read failed (%d)", rc);
		return rc;
	}

	out->temp_centi_c = sv_to_centi(temp.val1, temp.val2);
	out->hum_centi_pct = sv_to_centi(hum.val1, hum.val2);
	out->press_deci_pa = kpa_sv_to_deci_pa(press.val1, press.val2);
	out->uptime_ms = k_uptime_get();
	return 0;
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

	if (!device_is_ready(lunt_dev)) {
		LOG_ERR("Sensor %s is not ready: check wiring 3.3 v V VDD amd I2C Address",
			lunt_dev->name);
		return 0;
	}

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&button, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&button_cb, button_isr, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb);

	while (true) {
		struct lunt_sample s;

		if (read_sample(&s) == 0) {
			log_sample(&s);
		}
		gpio_pin_toggle_dt(&led);
		(void)k_sem_take(&wake_sem, K_SECONDS(10));
	}
	return 0;
}
