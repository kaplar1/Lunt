/*
The sensor gets its own thread.

Button ISR --k_sem--> sensor thread --k_msgq--> main thread
The intent is to make sensor (producer) and Log/LED/BLE (consumer) independent. Each blocks while it
has nothing to do, so the kernel dies.

*/

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "conversions.h"
#include "sensor_task.h"

LOG_MODULE_REGISTER(sensor_task, LOG_LEVEL_INF);

#define SAMPLE_INTERVAL_SEC 10

#define SENSOR_NODE DT_ALIAS(luntsensor)

#if !DT_NODE_HAS_STATUS_OKAY(SENSOR_NODE)
#error "No 'luntsensor' alias with status okay. Check boards/*.overlay"
#endif

static const struct device *const lunt_dev = DEVICE_DT_GET(SENSOR_NODE);

/* Queue of 8 samples. If the consumer falls behind, we drop the oldest. */
K_MSGQ_DEFINE(sample_msgq, sizeof(struct lunt_sample), 8, 4);

/* Binary semaphore used as a "measure now" doorbell from the button ISR. */
static K_SEM_DEFINE(measure_now_sem, 0, 1);

static int sensor_read_once(struct lunt_sample *out)
{
	struct sensor_value temp, hum, press;
	int rc = sensor_sample_fetch(lunt_dev);

	if (rc == 0) {
		rc = sensor_channel_get(lunt_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	}
	if (rc == 0) {
		rc = sensor_channel_get(lunt_dev, SENSOR_CHAN_HUMIDITY, &hum);
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

void sensor_task_trigger(void)
{
	/* k_sem_give is ISR-safe. Never read the sensor from the ISR itself:
	   I2C transfers block and must run in the thread context */
	k_sem_give(&measure_now_sem);
}

static void publish(const struct lunt_sample *s)
{
	while (k_msgq_put(&sample_msgq, s, K_NO_WAIT) != 0) {
		struct lunt_sample dropped;

		/* Queue full: drop the oldest so the newest reading alöways gets in.*/
		(void)k_msgq_get(&sample_msgq, &dropped, K_NO_WAIT);
		LOG_WRN("Sample queue full, dropped oldest");
	}
}

static void sensor_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("Sensor thread started, interval %d s", SAMPLE_INTERVAL_SEC);

	while (true) {
		struct lunt_sample s;

		if (device_is_ready(lunt_dev) && sensor_read_once(&s) == 0) {
			publish(&s);
		}

		/* Sleep until the interval passes OR the button is pressed. */
		(void)k_sem_take(&measure_now_sem, K_SECONDS(SAMPLE_INTERVAL_SEC));
	}
}

/* Created at boot but not started (delay = SYS_FOREVER_MS):
   main() starts it once everything is initialised.
*/

K_THREAD_DEFINE(sensor_tid, 1536, sensor_thread, NULL, NULL, NULL, 7, 0, SYS_FOREVER_MS);

void sensor_task_start(void)
{
	k_thread_start(sensor_tid);
}
