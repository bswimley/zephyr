#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <stdint.h>

extern char _microzed_vector_start[];

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

