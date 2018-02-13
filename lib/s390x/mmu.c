/*
 * s390x MMU
 *
 * Copyright (c) 2017 Red Hat Inc
 *
 * Authors:
 *  David Hildenbrand <david@redhat.com>
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License version 2.
 */

#include <libcflat.h>
#include <asm/pgtable.h>
#include <asm/arch_def.h>
#include <asm/barrier.h>
#include <vmalloc.h>

void configure_dat(int enable)
{
	uint64_t mask;

	if (enable)
		mask = extract_psw_mask() | PSW_MASK_DAT;
	else
		mask = extract_psw_mask() & ~PSW_MASK_DAT;

	load_psw_mask(mask);
}

static void mmu_enable(pgd_t *pgtable)
{
	const uint64_t asce = __pa(pgtable) | ASCE_DT_REGION1 |
			      REGION_TABLE_LENGTH;

	/* set primary asce */
	lctlg(1, asce);
	assert(stctg(1) == asce);

	/* enable dat (primary == 0 set as default) */
	configure_dat(1);
}

static pteval_t *get_pte(pgd_t *pgtable, uintptr_t vaddr)
{
	pgd_t *pgd = pgd_offset(pgtable, vaddr);
	p4d_t *p4d = p4d_alloc(pgd, vaddr);
	pud_t *pud = pud_alloc(p4d, vaddr);
	pmd_t *pmd = pmd_alloc(pud, vaddr);
	pte_t *pte = pte_alloc(pmd, vaddr);

	return &pte_val(*pte);
}

phys_addr_t virt_to_pte_phys(pgd_t *pgtable, void *vaddr)
{
	return (*get_pte(pgtable, (uintptr_t)vaddr) & PAGE_MASK) +
	       ((unsigned long)vaddr & ~PAGE_MASK);
}

pteval_t *install_page(pgd_t *pgtable, phys_addr_t phys, void *vaddr)
{
	pteval_t *p_pte = get_pte(pgtable, (uintptr_t)vaddr);

	/* first flush the old entry (if we're replacing anything) */
	if (!(*p_pte & PAGE_ENTRY_I))
		ipte((uintptr_t)vaddr, p_pte);

	*p_pte = __pa(phys);
	return p_pte;
}

static void setup_identity(pgd_t *pgtable, phys_addr_t start_addr,
			   phys_addr_t end_addr)
{
	phys_addr_t cur;

	start_addr &= PAGE_MASK;
	for (cur = start_addr; true; cur += PAGE_SIZE) {
		if (start_addr < end_addr && cur >= end_addr)
			break;
		if (start_addr > end_addr && cur <= end_addr)
			break;
		install_page(pgtable, cur, __va(cur));
	}
}

void *setup_mmu(phys_addr_t phys_end){
	pgd_t *page_root;

	/* allocate a region-1 table */
	page_root = pgd_alloc_one();

	/* map all physical memory 1:1 */
	setup_identity(page_root, 0, phys_end);

	/* generate 128MB of invalid adresses at the end (for testing PGM) */
	init_alloc_vpage((void *) -(1UL << 27));
	setup_identity(page_root, -(1UL << 27), 0);

	/* finally enable DAT with the new table */
	mmu_enable(page_root);
	return page_root;
}
