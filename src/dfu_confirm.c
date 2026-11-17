/*
The "confirm after self-test" pattern.

With MCUboot in a swap mode, a new image uploaded as TEST run once.
If the application does not confirm it before the next reboot, MCUboot
swaps the old image back. So the application decides whether the update
is healthy: we confirm only after the sensor self-test passes.

Image management (and these APIs) only exist when a DFU overlay is used,
so everything is guarded by CONFIG_MCUBOOT_IMG_MANAGER.

*/

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if defined(CONFIG_MCUBOOT_IMG_MANAGER)
#include <zephyr/dfu/mcuboot.h>
#endif

#include "dfu_confirm.h"

LOG_MODULE_REGISTER(dfu_confirm, CONFIG_APP_LOG_LEVEL);

#if defined(CONFIG_MCUBOOT_IMG_MANAGER)
static const char *swap_type_str(int type)
{
	switch (type) {
	case BOOT_SWAP_TYPE_NONE:
		return "none (running a confirmed image)";
	case BOOT_SWAP_TYPE_TEST:
		return "test (a test image will run on next reboot)";
	case BOOT_SWAP_TYPE_PERM:
		return "perm (an image will be made permanent on next reboot)";
	case BOOT_SWAP_TYPE_REVERT:
		return "revert (THIS image is on trial and will be reverted unless confirmed)";
	case BOOT_SWAP_TYPE_FAIL:
		return "fail";
	default:
		return "unknown";
	}
}
#endif

bool dfu_revert_pending(void)
{
#if defined(CONFIG_MCUBOOT_IMG_MANAGER)
	return mcuboot_swap_type() == BOOT_SWAP_TYPE_REVERT;
#else
	return false;
#endif
}

int dfu_confirm_image(void)
{
#if defined(CONFIG_MCUBOOT_IMG_MANAGER)
	if (boot_is_img_confirmed()) {
		LOG_INF("Image already confirmed");
		return 0;
	}

	int rc = boot_write_img_confirmed();

	if (rc) {
		LOG_ERR("Confirming image failed (%d)", rc);
	} else {
		LOG_INF("Self-test passed: image CONFIRMED, it will be kept");
	}
	return rc;
#else
	LOG_INF("No image manager in this build (no DFU overlay): nothing to confirm");
	return 0;
#endif
}

void dfu_log_state(void)
{
#if defined(CONFIG_MCUBOOT_IMG_MANAGER)
	LOG_INF("Image confirmed: %s, swap type: %s", boot_is_img_confirmed() ? "yes" : "no",
		swap_type_str(mcuboot_swap_type()));
#endif
}
