#ifndef BLE_ESS_H_
#define BLE_ESS_H_

#include "app_types.h"

/* Enable bluetooth and start advertising */
int ble_ess_init(void);

/* Update the charcterstic values and notify subscribed phones. */
void ble_Ess_update(const struct lunt_sample *s);

#endif /*BLE_ESS_H_*/
