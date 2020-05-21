/* CR pinning tests to check that WP bit in CR0 be pinned low correctly. This
 * has to be in a separate file than the CR0 high pinning because once pinned we
 * can't unpin from the guest. So we can't switch from high pinning to low
 * pinning to test within the same file / VM
 */

#include "libcflat.h"
#include "x86/desc.h"
#include "x86/processor.h"
#include "x86/vm.h"
#include "x86/msr.h"

#define USER_BASE	(1 << 24)

#define CR0_PINNED X86_CR0_WP

static void test_cr0_pinning_low(void)
{
	unsigned long long r = 0;
	ulong cr0 = read_cr0();
	int vector = 0;

	r = rdmsr(MSR_KVM_CR0_PIN_ALLOWED);
	report(r == CR0_PINNED, "[CR0] MSR_KVM_CR0_PIN_ALLOWED: %llx", r);

	cr0 &= ~CR0_PINNED;

	vector = write_cr0_checking(cr0);
	report(vector == 0, "[CR0] enable pinned bits. vector: %d", vector);

	cr0 = read_cr0();
	report((cr0 & CR0_PINNED) != CR0_PINNED,
	       "[CR0] after enabling pinned bits: %lx", cr0);

	wrmsr(MSR_KVM_CR0_PINNED_LOW, CR0_PINNED);
	r = rdmsr(MSR_KVM_CR0_PINNED_LOW);
	report(r == CR0_PINNED,
	       "[CR0] enable pinning. MSR_KVM_CR0_PINNED_LOW: %llx", r);

	vector = write_cr0_checking(cr0);
	report(vector == 0, "[CR0] write same value");

	vector = write_cr0_checking(cr0 | CR0_PINNED);
	report(vector == GP_VECTOR,
	       "[CR0] disable pinned bits. vector: %d", vector);

	cr0 = read_cr0();
	report((cr0 & CR0_PINNED) != CR0_PINNED,
	       "[CR0] pinned bits: %lx", cr0 & CR0_PINNED);
}

int main(int ac, char **av)
{
	if (!this_cpu_has(X86_FEATURE_CR_PIN)) {
		report_skip("CR pining not detected");
		return report_summary();
	}

	setup_idt();

	test_cr0_pinning_low();

	return report_summary();
}

