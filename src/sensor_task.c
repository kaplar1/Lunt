/*
Add Device Power management (sensor_power()).

Sensor thread: wakes every CONFIG_APP_SAMPLE_INTERVAL_SEC seconds, or immediately when the button is
pressed, reads th eBME280 and publishes an lunt_sample to a message queue.

*/

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

#include "conversions.h"
#include "sensor_task.h"

LOG_MODULE_REGISTER(sensor_task, CONFIG_APP_LOG_LEVEL);

#define SENSOR_NODE DT_ALIAS(luntsensor)

#if !DT_NODE_HAS_STATUS_OKAY(SENSOR_NODE)
#error "No 'luntsensor' alias with status okay. Check boards/*.overlay"
#endif

static const struct device *const lunt_dev = DEVICE_DT_GET(SENSOR_NODE);

/* Queue of 8 samples. If the consumer falls behind, we drop the oldest. */
K_MSGQ_DEFINE(sample_msgq, sizeof(struct lunt_sample), 8, 4);

/* Binary semaphore used as a "measure now" doorbell from the button ISR. */
static K_SEM_DEFINE(measure_now_sem, 0, 1);

static int sensor_power(enum pm_device_action action)
{
#if defined(CONFIG_PM_DEVICE)
	int rc = pm_device_action_run(lunt_dev, action);

	/* -EALREADY: already in that state, which is fine. */
	if (rc == -EALREADY || rc == -ENOSYS || rc == -ENOTSUP) {
		return 0;
	}
	return rc;
#else
	ARG_UNUSED(action);
	return 0;
#endif
}

static int sensor_read_once(struct lunt_sample *out)
{
	struct sensor_value temp, hum, press;
	int rc;

	rc = sensor_power(PM_DEVICE_ACTION_RESUME);
	if (rc) {
		LOG_WRN("Resume failed (%d)", rc);
	}

	rc = sensor_sample_fetch(lunt_dev);
	if (rc == 0) {
		rc = sensor_channel_get(lunt_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	}
	if (rc == 0) {
		rc = sensor_channel_get(lunt_dev, SENSOR_CHAN_HUMIDITY, &hum);
	}

	/* Put the sensor back to sleep whatever happened. */
	(void)sensor_power(PM_DEVICE_ACTION_SUSPEND);

	if (rc) {
		LOG_ERR("Sensor read failed (%d)", rc);
		return rc;
	}
	out->temp_centi_c = sv_to_centi(temp.val1, temp.val2);
	out->hum_centi_pct = sv_to_centi(hum.val1, hum.val2);
	out->press_deci_pa = kpa_sv_to_deci_pa(press.val1, press.val2);
	out->uptime_ms = k_uptime_get();

	LOG_DBG("raw temp %d.%06d, hum %d.%06d, press %d.%06d kPa", temp.val1, temp.val2, hum.val1,
		hum.val2, press.val1, press.val2);
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

	LOG_INF("Sensor thread started, interval %d s", CONFIG_APP_SAMPLE_INTERVAL_SEC);

	while (true) {
		struct lunt_sample s;

		if (device_is_ready(lunt_dev) && sensor_read_once(&s) == 0) {
			publish(&s);
		}

		/* Sleep until the interval passes OR the button is pressed.
		   While every thread is blocked like this, the kernel idles and
		   the SoC can drop into its low-power state. */
		(void)k_sem_take(&measure_now_sem, K_SECONDS(CONFIG_APP_SAMPLE_INTERVAL_SEC));
	}
}

/* Created at boot but not started (delay = SYS_FOREVER_MS):
   main() starts it once everything else is initialised. Priority 7 (preemptible).
*/

K_THREAD_DEFINE(sensor_tid, 1536, sensor_thread, NULL, NULL, NULL, 7, 0, SYS_FOREVER_MS);

void sensor_task_start(void)
{
	k_thread_start(sensor_tid);
}
