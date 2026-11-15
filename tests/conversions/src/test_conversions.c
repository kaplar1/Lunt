/*
Characterisation tests for the conversion helpers. These pin down the exact behavior, including the
awkward cases (negative fractions, rounding toward zero), so a later "cleanup" cannot silently
change it.
*/

#include <string.h>
#include <zephyr/ztest.h>

#include "conversions.h"

ZTEST_SUITE(conversions, NULL, NULL, NULL, NULL, NULL);

ZTEST(conversions, test_temp_positive)
{
	zassert_equal(sv_to_centi(23, 450000), 2345);
}

ZTEST(conversions, test_temp_negative_fraction_only)
{
	/* -0.5 degC is val1 = 0, val2 = -500000 */
	zassert_equal(sv_to_centi(0, -500000), -50);
}

ZTEST(conversions, test_temp_negative)
{
	zassert_equal(sv_to_centi(-1, -500000), -150);
}

ZTEST(conversions, test_truncates_toward_zero)
{
	/* 0.009999 -> 0.00, not rounded up */
	zassert_equal(sv_to_centi(0, 9999), 0);
	zassert_equal(sv_to_centi(0, -9999), 0);
}

ZTEST(conversions, test_pressure_standard_atmosphere)
{
	/* 101.325 kPa = 101325.0 Pa = 1013250 deci-Pa */
	zassert_equal(kpa_sv_to_deci_pa(101, 325000), 1013250);
}

ZTEST(conversions, test_format_centi)
{
	char buf[16];

	format_centi(buf, sizeof(buf), 2345);
	zassert_str_equal(buf, "23.45");

	format_centi(buf, sizeof(buf), -5);
	zassert_str_equal(buf, "-0.05");

	format_centi(buf, sizeof(buf), 0);
	zassert_str_equal(buf, "0.00");

	format_centi(buf, sizeof(buf), -150);
	zassert_str_equal(buf, "-1.50");
}

ZTEST(conversions, test_plausibility_bounds)
{
	zassert_true(lunt_values_plausible(2200, 4500, 1013250));
	zassert_true(lunt_values_plausible(-4000, 0, 300000));
	zassert_true(lunt_values_plausible(8500, 10000, 1100000));

	zassert_false(lunt_values_plausible(-4001, 4500, 1013250));
	zassert_false(lunt_values_plausible(2200, 10001, 1013250));
	zassert_false(lunt_values_plausible(2200, 4500, 299999));
	/* All zeros: the classic "sensor not really there" reading */
	zassert_false(lunt_values_plausible(0, 0, 0));
}
