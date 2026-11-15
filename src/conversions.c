#include <stdio.h>

#include "conversions.h"

int32_t sv_to_centi(int32_t val1, int32_t val2)
{
	/*val2 is in millionths: divide by 10000 to get hundreths. */
	return (val1 * 100) + (val2 / 10000);
}

int32_t kpa_sv_to_deci_pa(int32_t val1, int32_t val2)
{
	/*1 kPa  =10000 deci-Pa; val2 is millionts of a kPa = 0.01 deci-Pa.*/
	return (val1 * 10000) + (val2 / 100);
}

int format_centi(char *buf, size_t len, int32_t centi)
{
	const char *sign = (centi < 0) ? "-" : "";
	uint32_t abs_val = (centi < 0) ? (uint32_t)(-(int64_t)centi) : (uint32_t)centi;

	return snprintf(buf, len, "%s%u.%02u", sign, (unsigned int)(abs_val / 100U),
			(unsigned int)(abs_val % 100U));
}

bool lunt_values_plausible(int32_t temp_centi_c, int32_t hum_centi_pct, int32_t press_deci_pa)
{
	/*BME280 operating range: -40..85 degC, 0..100 %RH, 300..1100 hPa*/

	if (temp_centi_c < -4000 || temp_centi_c > 8500) {
		return false;
	}
	if (hum_centi_pct < 0 || hum_centi_pct > 10000) {
		return false;
	}
	if (press_deci_pa < 300000 || press_deci_pa > 1100000) {
		return false;
	}
	return true;
}
