// SPDX-License-Identifier: GPL-3.0-only
#include "sdkconfig.h"
#include "esp_attr.h"
#include "hal/clk_gate_ll.h"

/* Ported from Time-Tick V1.0/main/platform/sdk_compat.c.
 * Vibe Mode 0.2.1 (IDF 7b9cc1ac79f) may gate REF160M on shutdown.
 * IDF 1c42e514c90 assumes this gate stays open with PVT enabled and skips
 * it in clock acquisitions. A CPU reset retains the closed gate, so the
 * first Wi-Fi PLL selection stalls modem register access. Restore the PVT
 * invariant before drivers start, without adding runtime clock references.
 * This early startup hook must run from IRAM, before flash is available.
 */
void __real_esp_clk_tree_initialize(void);
void IRAM_ATTR __wrap_esp_clk_tree_initialize(void)
{
    __real_esp_clk_tree_initialize();
#if CONFIG_IDF_TARGET_ESP32S31 && CONFIG_ESP_ENABLE_PVT
    _clk_gate_ll_ref_160m_clk_en(true);
#endif
}
