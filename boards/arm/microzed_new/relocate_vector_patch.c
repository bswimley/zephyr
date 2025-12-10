#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <stdint.h>

extern char _microzed_vector_start[];

static void flush_dcache_range(uint32_t start, uint32_t size)
{
    uint32_t end = start + size;
    for (uint32_t addr = start; addr < end; addr += 32) {
        __asm volatile (
            "mcr p15, 0, %0, c7, c10, 1\n"  // Clean D-cache line by MVA
            "mcr p15, 0, %0, c7, c6, 1\n"   // Invalidate D-cache line by MVA
            : : "r"(addr) : "memory"
        );
    }
    __asm volatile("dsb sy" ::: "memory");
}


/* Address where your vector table is linked */

/* Relocates the vector table by chaning vbar to point to the new vector table.
 *
 * Since MMU is enabled, we must flush the instruction cache to ensure that
 * the new vector table is fetched fresh.
 */
void relocate_vector_table(void)
{
    uint32_t *vector_table = (uint32_t *)_microzed_vector_start;
    /* Disable interrupts if you’re in the middle of setup */
    __asm volatile ("cpsid i" : : : "memory");

   /* clean & drain any outstanding writes */
    __asm volatile("dsb sy" ::: "memory");

    /* Point VBAR at your vector table */
    __asm volatile (
        "mcr p15, 0, %0, c12, c0, 0\n"  /* Write VBAR */
        : : "r"(vector_table) : "memory"
    );
    flush_dcache_range((uint32_t)_microzed_vector_start, 0x400);  // Vector table
//     flush_dcache_range(0x00000fc0, 0x10000);  // Zephyr image region

    /* Optional: invalidate I‐cache so new vectors are fetched fresh */
    __asm volatile (
        "mov r0, #0\n"
        "mcr p15, 0, r0, c7, c5, 0\n"   /* invalidate I‐cache */
        ::: "r0", "memory"
    );

    /* ensure the new VBAR is in effect before any subsequent instruction */
    __asm volatile("isb sy" ::: "memory");

    /* Re‐enable interrupts */
    __asm volatile ("cpsie i" : : : "memory");
}

