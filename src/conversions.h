/*
Conversions helpers with no zephyr dependencies, so they can be unit-tested on their own.

Zephyr's sensor API returns values as tow integers:
val1 = integer part, val2 = millionths (same sigh as val1)
Ex: 23.45 -> val1 = 23, val2 = 4500000
    -0.5 -> val1 = 0, val2 = -500000
*/

#ifndef CONVERSIONS_H_
#define CONVERSIONS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*Temperature (degC) or humidity (%) to hundreths. */
int32_t sv_to_centi(int32_t val1, int32_t val2);

/* Pressure in kPa (as the BME280 driver reports it) to tenths of a pascal. */
int32_t kpa_sv_to_deci_pa(int32_t val1, int32_t val2);

/* Format hundredths as text: 2345 -> "23.45", -5 -> "-0.05". Returns snprintf result. */
int format_centi(char *buf, size_t len, int32_t centi);

/* Sanity check against the BME280's operating range. */
bool lunt_values_plausible(int32_t temp_centi_c, int32_t hum_centi_pct, int32_t press_deci_pa);
#endif /*CONVESIONS_H_*/
