
#include <zephyr/arch/cpu.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

#include <cmsis_core.h>
#include <zephyr/arch/arm/mmu/arm_mmu.h>
#include "soc_reset_hook.h"

/* System Level Control Registers (SLCR) */
#define SLCR_UNLOCK     0x0008
#define SLCR_UNLOCK_KEY 0xdf0d

void soc_reset_hook(void)
{

	/*
	 * When coming out of u-boot rather than downloading the Zephyr binary
	 * via JTAG, a few things modified by u-boot have to be re-set to a
	 * suitable default value for Zephyr to run, namely:
	 *
	 * - u-boot places the exception vectors somewhere in RAM and then
	 *   lets the VBAR register point to them. Zephyr uses the default
	 *   vector table location at address zero (and maybe at some later
	 *   time alternatively the HIVECS position). If VBAR isn't reset
	 *   to zero, the system crashes during the first context switch when
	 *   SVC is invoked.
	 * - u-boot sets the following bits in the SCTLR register:
	 *   - [I] ICache enable
	 *   - [C] DCache enable
	 *   - [Z] Branch prediction enable
	 *   - [A] Enforce strict alignment enable
	 *   [I] and [C] will be enabled during the MMU init -> disable them
	 *   until then. [Z] is probably not harmful. [A] will cause a crash
	 *   as early as z_mem_manage_init when an unaligned access is performed
	 *   -> clear [A].
	 */
	uint32_t vbar = 0;

	__set_VBAR(vbar);
	uint32_t sctlr = __get_SCTLR();

	sctlr &= ~SCTLR_I_Msk;
	sctlr &= ~SCTLR_C_Msk;
	sctlr &= ~SCTLR_A_Msk;
	__set_SCTLR(sctlr);

#if DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(slcr))
	mm_reg_t addr = DT_REG_ADDR(DT_NODELABEL(slcr));

	/* Unlock System Level Control Registers (SLCR) */
	sys_write32(SLCR_UNLOCK_KEY, addr + SLCR_UNLOCK);
#endif
}
