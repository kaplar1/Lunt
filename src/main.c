#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <app_version.h>

LOG_MODULE_REGISTER(Lunt, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("Lunt: v%s booting on %s", APP_VERSION_STRING, CONFIG_BOARD_TARGET);

	while (true) {
		LOG_DBG("not shown: debug is below the INF level we registered");
		LOG_INF("alive");
		k_sleep(K_SECONDS(10));
	}
	return 0;
}
