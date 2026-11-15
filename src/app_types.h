/*
Shared data types. One reading of the environment, in fixed point integers so we never need floating
point on the device
*/

#ifndef APP_TYPES_H_
#define APP_TYPES_H_
#include <stdint.h>

struct lunt_sample {
	int32_t temp_centi_c;  /* 0.01 degC (2345 = 23.45 degC)*/
	int32_t hum_centi_pct; /* 0.01 %RH (4512 = 45.12 %)*/
	int32_t press_deci_pa; /* 0.1 Pa (1013250 = 1013.25 hPa)*/
	int64_t uptime_ms;     /* when the reading was taken */
};
#endif /*APP_TYPES_H_*/
