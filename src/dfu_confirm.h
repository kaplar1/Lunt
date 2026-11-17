#ifndef DFU_CONFIRM_H_
#define DFU_CONFIRM_H_

#include <stdbool.h>

/* True if this image is running as a TEST image: MCUboot will swap back to
the orevious image on the next reboot unless we confirm. */
bool dfu_revert_pending(void);

/* Mark the running image as good, so MCUboot keeps it permanently. */
int dfu_confirm_image(void);

/* Log the bootloader's view of the running image. */
void dfu_log_state(void);

#endif /* DFU_CONFIRM_H_*/
