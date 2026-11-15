/*Accessing LED and button through devicetree aliases, and the first
interrupt-to-thread handoff
device tree name LED0 -> "led0" Button 0 -> "sw0"
*/

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <app_version.h>

LOG_MODULE_REGISTER(Lunt, LOG_LEVEL_INF);

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

int main(void)
{
	LOG_INF("Lunt: v%s booting on %s", APP_VERSION_STRING, CONFIG_BOARD_TARGET);

	if (!gpio_is_ready_dt(&led) || !gpio_is_ready_dt(&button)) {
		LOG_ERR("GPIO not ready");
		return 0;
	}

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&button, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&button_cb, button_isr, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb);

	while (true) {
		int rc = k_sem_take(&wake_sem, K_SECONDS(10));
		LOG_INF("%s", rc == 0 ? "button pressed" : "10 s timeout");
		gpio_pin_toggle_dt(&led);
	}
	return 0;
}
