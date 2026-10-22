// SPDX-License-Identifier: GPL-3.0-only
#include "sdkconfig.h"
#include "esp_attr.h"
#include "hal/clk_gate_ll.h"

/* Vibe Mode 0.2.1 uses IDF 7b9cc1ac79f, which may gate REF160M on
 * shutdown. IDF 1c42e514c90 assumes this gate remains open with PVT enabled
 * and omits it from subsequent clock acquisitions. CPU-only restart retains
 * the closed gate, so the first Wi-Fi PLL selection stalls modem APB access.
 * Restore the PVT invariant at clock-tree startup, before the RTOS/drivers.
 * This does not add clock references or change the runtime power policy.
 */
void __real_esp_clk_tree_initialize(void);
void IRAM_ATTR __wrap_esp_clk_tree_initialize(void)
{
    __real_esp_clk_tree_initialize();
#if CONFIG_IDF_TARGET_ESP32S31 && CONFIG_ESP_ENABLE_PVT
    _clk_gate_ll_ref_160m_clk_en(true);
#endif
}
