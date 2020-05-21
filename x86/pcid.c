/* Basic PCID & INVPCID functionality test */

#include "libcflat.h"
#include "processor.h"
#include "desc.h"

struct invpcid_desc {
    unsigned long pcid : 12;
    unsigned long rsv  : 52;
    unsigned long addr : 64;
};

static int invpcid_checking(unsigned long type, void *desc)
{
    asm volatile (ASM_TRY("1f")
                  ".byte 0x66,0x0f,0x38,0x82,0x18 \n\t" /* invpcid (%rax), %rbx */
                  "1:" : : "a" (desc), "b" (type));
    return exception_vector();
}

static void test_cpuid_consistency(int pcid_enabled, int invpcid_enabled)
{
    int passed = !(!pcid_enabled && invpcid_enabled);
    report(passed, "CPUID consistency");
}

static void test_pcid_enabled(void)
{
    int passed = 0;
    ulong cr0 = read_cr0(), cr3 = read_cr3(), cr4 = read_cr4();

    /* try setting CR4.PCIDE, no exception expected */
    if (write_cr4_checking(cr4 | X86_CR4_PCIDE) != 0)
        goto report;

    /* try clearing CR0.PG when CR4.PCIDE=1, #GP expected */
    if (write_cr0_checking(cr0 & ~X86_CR0_PG) != GP_VECTOR)
        goto report;

    write_cr4(cr4);

    /* try setting CR4.PCIDE when CR3[11:0] != 0 , #GP expected */
    write_cr3(cr3 | 0x001);
    if (write_cr4_checking(cr4 | X86_CR4_PCIDE) != GP_VECTOR)
        goto report;
    write_cr3(cr3);

    passed = 1;

report:
    report(passed, "Test on PCID when enabled");
}

static void test_pcid_disabled(void)
{
    int passed = 0;
    ulong cr4 = read_cr4();

    /* try setting CR4.PCIDE, #GP expected */
    if (write_cr4_checking(cr4 | X86_CR4_PCIDE) != GP_VECTOR)
        goto report;

    passed = 1;

report:
    report(passed, "Test on PCID when disabled");
}

static void test_invpcid_enabled(void)
{
    int passed = 0;
    ulong cr4 = read_cr4();
    struct invpcid_desc desc;
    desc.rsv = 0;

    /* try executing invpcid when CR4.PCIDE=0, desc.pcid=0 and type=1
     * no exception expected
     */
    desc.pcid = 0;
    if (invpcid_checking(1, &desc) != 0)
        goto report;

    /* try executing invpcid when CR4.PCIDE=0, desc.pcid=1 and type=1
     * #GP expected
     */
    desc.pcid = 1;
    if (invpcid_checking(1, &desc) != GP_VECTOR)
        goto report;

    if (write_cr4_checking(cr4 | X86_CR4_PCIDE) != 0)
        goto report;

    /* try executing invpcid when CR4.PCIDE=1
     * no exception expected
     */
    desc.pcid = 10;
    if (invpcid_checking(2, &desc) != 0)
        goto report;

    passed = 1;

report:
    report(passed, "Test on INVPCID when enabled");
}

static void test_invpcid_disabled(void)
{
    int passed = 0;
    struct invpcid_desc desc;

    /* try executing invpcid, #UD expected */
    if (invpcid_checking(2, &desc) != UD_VECTOR)
        goto report;

    passed = 1;

report:
    report(passed, "Test on INVPCID when disabled");
}

int main(int ac, char **av)
{
    int pcid_enabled = 0, invpcid_enabled = 0;

    if (this_cpu_has(X86_FEATURE_PCID))
        pcid_enabled = 1;
    if (this_cpu_has(X86_FEATURE_INVPCID))
        invpcid_enabled = 1;

    test_cpuid_consistency(pcid_enabled, invpcid_enabled);

    if (pcid_enabled)
        test_pcid_enabled();
    else
        test_pcid_disabled();

    if (invpcid_enabled)
        test_invpcid_enabled();
    else
        test_invpcid_disabled();

    return report_summary();
}
