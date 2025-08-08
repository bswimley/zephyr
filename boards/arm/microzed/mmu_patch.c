#include <zephyr/init.h>
#include <zephyr/kernel.h>



static int board_mmu_patch_init(void)
{
	uint32_t vbar = 0;

    // Set the vector base address register to point to the vector table
    // This is necessary for the ARM architecture to handle exceptions correctly

	__set_VBAR(vbar);
    return 0;
}

#include <string.h>

/*
 * GCC can detect if memcpy is passed a NULL argument, however one of
 * the cases of relocate_vector_table() it is valid to pass NULL, so we
 * suppress the warning for this case.  We need to do this before
 * string.h is included to get the declaration of memcpy.
 */
TOOLCHAIN_DISABLE_WARNING(TOOLCHAIN_WARNING_NONNULL)

#define VECTOR_ADDRESS 0

extern char _microzed_vector_start[];
extern char _microzed_vector_end[];

void relocate_vector_table(void)
{
#if defined(CONFIG_XIP) && (CONFIG_FLASH_BASE_ADDRESS != 0) ||                                     \
	!defined(CONFIG_XIP) && (CONFIG_SRAM_BASE_ADDRESS != 0)
	write_sctlr(read_sctlr() & ~HIVECS);
	size_t vector_size = (size_t)_microzed_vector_end - (size_t)_microzed_vector_start;
	(void)memcpy(VECTOR_ADDRESS, _microzed_vector_start, vector_size);
#endif
}

SYS_INIT(board_mmu_patch_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
