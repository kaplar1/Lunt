#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include <zephyr/kernel.h>

#include "app_types.h"

/* Readings are published here; main() is the consumer */
extern struct k_msgq sample_msgq;

/* Check the sensor is present ang gives palusible values. USed at boot to decide
whether a newly updated image is healthy enough to confirm. */
bool sensor_selftest(void);

/* Start the periodic sensor thread (it is created but not started at boot) */
void sensor_task_start(void);

/* Ask for an immediate reading. Sage to call from an interrupt handler.*/
void sensor_task_trigger(void);

#endif /* SENSOR_TASK_H_ */
