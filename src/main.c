#include <zephyr/kernel.h>

int main(void)
{
	printk("Lunt: Hello for %s\n", CONFIG_BOARD_TARGET);

	while (true) {
		printk("uptime %u ms\n", k_uptime_get_32());
		k_msleep(1000);
	}
	return 0;
}
