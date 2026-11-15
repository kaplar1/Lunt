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

static void read_and_log(void)
{
	struct sensor_value temp, hum, press;

	if (sensor_sample_fetch(lunt_dev) != 0) {
		LOG_ERR("Sensor fetch failed");
		return;
	}
	sensor_channel_get(lunt_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	sensor_channel_get(lunt_dev, SENSOR_CHAN_HUMIDITY, &hum);
	sensor_channel_get(lunt_dev, SENSOR_CHAN_PRESS, &press);

	/*val1 = integer part, val2 = millionths */
	LOG_INF("T=%d.%06d C RH=%d.%06d %% P=%d.%06d kPa", temp.val1, temp.val2, hum.val1, hum.val2,
		press.val1, press.val2);
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
		read_and_log();
		gpio_pin_toggle_dt(&led);
		(void)k_sem_take(&wake_sem, K_SECONDS(10));
	}
	return 0;
}
