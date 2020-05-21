/* CR pinning tests. Not including pinning CR0 WP low, that lives in
 * cr_pin_low.c. This file tests that CR pinning prevents the guest VM from
 * flipping bit pinned via MSRs in control registers. For CR4 we pin UMIP and
 * SMEP bits high and SMAP low, and verify we can't toggle them after pinning
 */

#include "libcflat.h"
#include "x86/desc.h"
#include "x86/processor.h"
#include "x86/vm.h"
#include "x86/msr.h"

#define USER_BASE	(1 << 24)

#define CR0_PINNED X86_CR0_WP
#define CR4_SOME_PINNED (X86_CR4_UMIP | X86_CR4_SMEP)
#define CR4_ALL_PINNED (CR4_SOME_PINNED | X86_CR4_SMAP | X86_CR4_FSGSBASE)

static void test_cr0_pinning(void)
{
	unsigned long long r = 0;
	ulong cr0 = read_cr0();
	int vector = 0;

	r = rdmsr(MSR_KVM_CR0_PIN_ALLOWED);
	report(r == CR0_PINNED, "[CR0] MSR_KVM_CR0_PIN_ALLOWED: %llx", r);

	cr0 |= CR0_PINNED;

	vector = write_cr0_checking(cr0);
	report(vector == 0, "[CR0] enable pinned bits. vector: %d", vector);

	cr0 = read_cr0();
	report((cr0 & CR0_PINNED) == CR0_PINNED,
	       "[CR0] after enabling pinned bits: %lx", cr0);

	wrmsr(MSR_KVM_CR0_PINNED_HIGH, CR0_PINNED);
	r = rdmsr(MSR_KVM_CR0_PINNED_HIGH);
	report(r == CR0_PINNED,
	       "[CR0] enable pinning. MSR_KVM_CR0_PINNED_HIGH: %llx", r);

	vector = write_cr0_checking(cr0);
	report(vector == 0, "[CR0] write same value");

	vector = write_cr0_checking(cr0 & ~CR0_PINNED);
	report(vector == GP_VECTOR,
	       "[CR0] disable pinned bits. vector: %d", vector);

	cr0 = read_cr0();
	report((cr0 & CR0_PINNED) == CR0_PINNED,
	       "[CR0] pinned bits: %lx", cr0 & CR0_PINNED);
}

static void test_cr4_pin_allowed(void)
{
	unsigned long long r = 0;

	r = rdmsr(MSR_KVM_CR4_PIN_ALLOWED);
	report(r == CR4_ALL_PINNED, "[CR4] MSR_KVM_CR4_PIN_ALLOWED: %llx", r);
}

static void test_cr4_pinning_some_umip_smep_pinned(void)
{
	unsigned long long r = 0;
	ulong cr4 = read_cr4();
	int vector = 0;

	cr4 |= CR4_SOME_PINNED;

	vector = write_cr4_checking(cr4);
	report(vector == 0,
	       "[CR4 SOME] enable pinned bits. vector: %d", vector);

	wrmsr(MSR_KVM_CR4_PINNED_HIGH, CR4_SOME_PINNED);
	r = rdmsr(MSR_KVM_CR4_PINNED_HIGH);
	report(r == CR4_SOME_PINNED,
	       "[CR4 SOME] enable pinning. MSR_KVM_CR4_PINNED_HIGH: %llx", r);

	cr4 = read_cr4();
	report((cr4 & CR4_SOME_PINNED) == CR4_SOME_PINNED,
	       "[CR4 SOME] after enabling pinned bits: %lx", cr4);
	report(1, "[CR4 SOME] cr4: 0x%08lx", cr4);

	vector = write_cr4_checking(cr4);
	report(vector == 0, "[CR4 SOME] write same value");

	vector = write_cr4_checking(cr4 & ~CR4_SOME_PINNED);
	report(vector == GP_VECTOR,
	       "[CR4 SOME] disable pinned bits. vector: %d", vector);

	cr4 = read_cr4();
	report((cr4 & CR4_SOME_PINNED) == CR4_SOME_PINNED,
	       "[CR4 SOME] pinned bits: %lx", cr4 & CR4_SOME_PINNED);

	vector = write_cr4_checking(cr4 & ~X86_CR4_SMEP);
	report(vector == GP_VECTOR,
	       "[CR4 SOME] disable single pinned bit. vector: %d", vector);

	cr4 = read_cr4();
	report((cr4 & CR4_SOME_PINNED) == CR4_SOME_PINNED,
	       "[CR4 SOME] pinned bits: %lx", cr4 & CR4_SOME_PINNED);
}

static void test_cr4_pinning_all_umip_smep_high_smap_low_pinned(void)
{
	unsigned long long r = 0;
	ulong cr4 = read_cr4();
	int vector = 0;

	cr4 |= CR4_SOME_PINNED;
	cr4 &= ~X86_CR4_SMAP;

	report((cr4 & CR4_ALL_PINNED) == CR4_SOME_PINNED,
	       "[CR4 ALL] CHECK: %lx", cr4);

	vector = write_cr4_checking(cr4);
	report(vector == 0, "[CR4 ALL] write bits to cr4. vector: %d", vector);

	cr4 = read_cr4();
	report((cr4 & CR4_ALL_PINNED) == CR4_SOME_PINNED,
	       "[CR4 ALL] after enabling pinned bits: %lx", cr4);

	wrmsr(MSR_KVM_CR4_PINNED_LOW, X86_CR4_SMAP);
	r = rdmsr(MSR_KVM_CR4_PINNED_LOW);
	report(r == X86_CR4_SMAP,
	       "[CR4 ALL] enable pinning. MSR_KVM_CR4_PINNED_LOW: %llx", r);

	vector = write_cr4_checking(cr4);
	report(vector == 0, "[CR4 ALL] write same value");

	cr4 &= ~CR4_SOME_PINNED;
	cr4 |= X86_CR4_SMAP;

	vector = write_cr4_checking(cr4);
	report(vector == GP_VECTOR,
	       "[CR4 ALL] disable pinned bits. vector: %d", vector);

	cr4 = read_cr4();
	report((cr4 & CR4_ALL_PINNED) == CR4_SOME_PINNED,
	       "[CR4 ALL] pinned bits: %lx", cr4 & CR4_ALL_PINNED);

	vector = write_cr4_checking(cr4 | X86_CR4_SMAP);
	report(vector == GP_VECTOR,
	       "[CR4 ALL] enable pinned low bit. vector: %d", vector);

	cr4 = read_cr4();
	report((cr4 & CR4_ALL_PINNED) == CR4_SOME_PINNED,
	       "[CR4 ALL] pinned bits: %lx", cr4 & CR4_ALL_PINNED);

	vector = write_cr4_checking(cr4 & ~X86_CR4_SMEP);
	report(vector == GP_VECTOR,
	       "[CR4 ALL] disable pinned high bit. vector: %d", vector);

	cr4 = read_cr4();
	report((cr4 & CR4_ALL_PINNED) == CR4_SOME_PINNED,
	       "[CR4 ALL] pinned bits: %lx", cr4 & CR4_ALL_PINNED);

	vector = write_cr4_checking((cr4 & ~X86_CR4_SMEP) | X86_CR4_SMAP);
	report(vector == GP_VECTOR,
	       "[CR4 ALL] disable pinned high bit enable pinned low. vector: %d",
	       vector);

	cr4 = read_cr4();
	report((cr4 & CR4_ALL_PINNED) == CR4_SOME_PINNED,
	       "[CR4 ALL] pinned bits: %lx", cr4 & CR4_ALL_PINNED);
}

static void test_cr4_pinning(void)
{
	test_cr4_pin_allowed();

	if (!this_cpu_has(X86_FEATURE_UMIP)) {
		printf("UMIP not available\n");
		return;
	}

	if (!this_cpu_has(X86_FEATURE_SMEP)) {
		printf("SMEP not available\n");
		return;
	}

	test_cr4_pinning_some_umip_smep_pinned();

	if (!this_cpu_has(X86_FEATURE_SMAP)) {
		printf("SMAP not enabled\n");
		return;
	}

	test_cr4_pinning_all_umip_smep_high_smap_low_pinned();
}

int main(int ac, char **av)
{
	unsigned long i;

	if (!this_cpu_has(X86_FEATURE_CR_PIN)) {
		report_skip("CR pining not detected");
		return report_summary();
	}

	setup_idt();
	setup_vm();

	// Map first 16MB as supervisor pages
	for (i = 0; i < USER_BASE; i += PAGE_SIZE)
		*get_pte(phys_to_virt(read_cr3()), phys_to_virt(i)) &= ~PT_USER_MASK;

	test_cr0_pinning();
	test_cr4_pinning();

	return report_summary();
}

