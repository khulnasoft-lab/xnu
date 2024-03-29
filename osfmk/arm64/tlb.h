/*
 * Copyright (c) 2019-2022 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#pragma once

#include <arm64/proc_reg.h>
#include <machine/atomic.h>

#define tlbi_addr(x) ((((x) >> 12) & TLBI_ADDR_MASK) << TLBI_ADDR_SHIFT)
#define tlbi_asid(x) (((uintptr_t)(x) & TLBI_ASID_MASK) << TLBI_ASID_SHIFT)

#if __ARM_KERNEL_PROTECT__
/*
 * __ARM_KERNEL_PROTECT__ adds two complications to TLB management:
 *
 * 1. As each pmap has two ASIDs, every TLB operation that targets an ASID must
 *   target both ASIDs for the pmap that owns the target ASID.
 *
 * 2. Any TLB operation targeting the kernel_pmap ASID (ASID 0) must target all
 *   ASIDs (as kernel_pmap mappings may be referenced while using an ASID that
 *   belongs to another pmap).  We expect these routines to be called with the
 *   EL0 ASID for the target; not the EL1 ASID.
 */
#endif /* __ARM_KERNEL_PROTECT__ */

static inline void
sync_tlb_flush(void)
{
#if HAS_FEAT_XS
	asm volatile ("dsb ishnxs":::"memory");
#else
	__builtin_arm_dsb(DSB_ISH);
#endif /* HAS_FEAT_XS */
	__builtin_arm_isb(ISB_SY);
}

static inline void
sync_tlb_flush_local(void)
{
#if HAS_FEAT_XS
	asm volatile ("dsb nshnxs":::"memory");
#else
	__builtin_arm_dsb(DSB_NSH);
#endif /* HAS_FEAT_XS */
	__builtin_arm_isb(ISB_SY);
}

#if   HAS_FEAT_XS

static inline void
sync_tlb_flush_strong(void)
{
	__builtin_arm_dsb(DSB_ISH);
	__builtin_arm_isb(ISB_SY);
}

#endif //


static inline void
arm64_sync_tlb(bool strong __unused)
{
	sync_tlb_flush();
}

// flush_mmu_tlb: full TLB flush on all cores
static inline void
flush_mmu_tlb_async(void)
{
	asm volatile ("tlbi vmalle1is");
}

static inline void
flush_mmu_tlb(void)
{
	flush_mmu_tlb_async();
#if HAS_FEAT_XS
	/* Full flush is always treated as "strong" when there is a HW-level distinction. */
	sync_tlb_flush_strong();
#else
	sync_tlb_flush();
#endif /* HAS_FEAT_XS */
}

// flush_core_tlb: full TLB flush on local core only
static inline void
flush_core_tlb_async(void)
{
#if HAS_FEAT_XS
	asm volatile ("tlbi vmalle1nxs");
#else
	asm volatile ("tlbi vmalle1");
#endif /* HAS_FEAT_XS */
}

static inline void
flush_core_tlb(void)
{
	flush_core_tlb_async();
	sync_tlb_flush_local();
}

// flush_mmu_tlb_allentries_async: flush entries that map VA range, all ASIDS, all cores
// start and end are in units of 4K pages.
static inline void
flush_mmu_tlb_allentries_async(uint64_t start, uint64_t end, uint64_t pmap_page_size,
    bool last_level_only, bool strong __unused)
{
#if __ARM_16K_PG__
	if (pmap_page_size == 16384) {
		start = start & ~0x3ULL;

		/*
		 * The code below is not necessarily correct.  From an overview of
		 * the client code, the expected contract for TLB flushes is that
		 * we will expand from an "address, length" pair to "start address,
		 * end address" in the course of a TLB flush.  This suggests that
		 * a flush for "X, X+4" is actually only asking for a flush of a
		 * single 16KB page.  At the same time, we'd like to be prepared
		 * for bad inputs (X, X+3), so add 3 and then truncate the 4KB page
		 * number to a 16KB page boundary.  This should deal correctly with
		 * unaligned inputs.
		 *
		 * If our expecations about client behavior are wrong however, this
		 * will lead to occasional TLB corruption on platforms with 16KB
		 * pages.
		 */
		end = (end + 0x3ULL) & ~0x3ULL;
	}
#endif // __ARM_16K_PG__
	if (last_level_only) {
		for (; start < end; start += (pmap_page_size / 4096)) {
#if HAS_FEAT_XS
			if (__probable(!strong)) {
				asm volatile ("tlbi vaale1isnxs, %0" : : "r"(start));
			} else
#endif /* HAS_FEAT_XS */
			{
				asm volatile ("tlbi vaale1is, %0" : : "r"(start));
			}
		}
	} else {
		for (; start < end; start += (pmap_page_size / 4096)) {
#if HAS_FEAT_XS
			if (__probable(!strong)) {
				asm volatile ("tlbi vaae1isnxs, %0" : : "r"(start));
			} else
#endif /* HAS_FEAT_XS */
			{
				asm volatile ("tlbi vaae1is, %0" : : "r"(start));
			}
		}
	}
}

static inline void
flush_mmu_tlb_allentries(uint64_t start, uint64_t end, uint64_t pmap_page_size, bool last_level_only, bool strong)
{
	flush_mmu_tlb_allentries_async(start, end, pmap_page_size, last_level_only, strong);
	arm64_sync_tlb(strong);
}

// flush_mmu_tlb_entries: flush TLB entries that map a VA range and ASID, all cores
// start and end must have the ASID in the high 16 bits, with the VA in units of 4K in the lowest bits
// Will also flush global entries that match the VA range
static inline void
flush_mmu_tlb_entries_async(uint64_t start, uint64_t end, uint64_t pmap_page_size,
    bool last_level_only, bool strong __unused)
{
#if __ARM_16K_PG__
	if (pmap_page_size == 16384) {
		start = start & ~0x3ULL;

		/*
		 * The code below is not necessarily correct.  From an overview of
		 * the client code, the expected contract for TLB flushes is that
		 * we will expand from an "address, length" pair to "start address,
		 * end address" in the course of a TLB flush.  This suggests that
		 * a flush for "X, X+4" is actually only asking for a flush of a
		 * single 16KB page.  At the same time, we'd like to be prepared
		 * for bad inputs (X, X+3), so add 3 and then truncate the 4KB page
		 * number to a 16KB page boundary.  This should deal correctly with
		 * unaligned inputs.
		 *
		 * If our expecations about client behavior are wrong however, this
		 * will lead to occasional TLB corruption on platforms with 16KB
		 * pages.
		 */
		end = (end + 0x3ULL) & ~0x3ULL;
	}
#endif // __ARM_16K_PG__
#if __ARM_KERNEL_PROTECT__
	uint64_t asid = start >> TLBI_ASID_SHIFT;
	/*
	 * If we are flushing ASID 0, this is a kernel operation.  With this
	 * ASID scheme, this means we should flush all ASIDs.
	 */
	if (asid == 0) {
		if (last_level_only) {
			for (; start < end; start += (pmap_page_size / 4096)) {
				asm volatile ("tlbi vaale1is, %0" : : "r"(start));
			}
		} else {
			for (; start < end; start += (pmap_page_size / 4096)) {
				asm volatile ("tlbi vaae1is, %0" : : "r"(start));
			}
		}
		return;
	}
	start = start | (1ULL << TLBI_ASID_SHIFT);
	end = end | (1ULL << TLBI_ASID_SHIFT);
	if (last_level_only) {
		for (; start < end; start += (pmap_page_size / 4096)) {
			start = start & ~(1ULL << TLBI_ASID_SHIFT);
			asm volatile ("tlbi vale1is, %0" : : "r"(start));
			start = start | (1ULL << TLBI_ASID_SHIFT);
			asm volatile ("tlbi vale1is, %0" : : "r"(start));
		}
	} else {
		for (; start < end; start += (pmap_page_size / 4096)) {
			start = start & ~(1ULL << TLBI_ASID_SHIFT);
			asm volatile ("tlbi vae1is, %0" : : "r"(start));
			start = start | (1ULL << TLBI_ASID_SHIFT);
			asm volatile ("tlbi vae1is, %0" : : "r"(start));
		}
	}
#else
	if (last_level_only) {
		for (; start < end; start += (pmap_page_size / 4096)) {
#if HAS_FEAT_XS
			if (__probable(!strong)) {
				asm volatile ("tlbi vale1isnxs, %0" : : "r"(start));
			} else
#endif /* HAS_FEAT_XS */
			{
				asm volatile ("tlbi vale1is, %0" : : "r"(start));
			}
		}
	} else {
		for (; start < end; start += (pmap_page_size / 4096)) {
#if HAS_FEAT_XS
			if (__probable(!strong)) {
				asm volatile ("tlbi vae1isnxs, %0" : : "r"(start));
			} else
#endif /* HAS_FEAT_XS */
			{
				asm volatile ("tlbi vae1is, %0" : : "r"(start));
			}
		}
	}
#endif /* __ARM_KERNEL_PROTECT__ */
}

static inline void
flush_mmu_tlb_entries(uint64_t start, uint64_t end, uint64_t pmap_page_size, bool last_level_only, bool strong)
{
	flush_mmu_tlb_entries_async(start, end, pmap_page_size, last_level_only, strong);
	arm64_sync_tlb(strong);
}

// flush_mmu_tlb_asid: flush all entries that match an ASID, on all cores
// ASID must be in high 16 bits of argument
// Will not flush global entries
static inline void
flush_mmu_tlb_asid_async(uint64_t val, bool strong __unused)
{
#if __ARM_KERNEL_PROTECT__
	/*
	 * If we are flushing ASID 0, this is a kernel operation.  With this
	 * ASID scheme, this means we should flush all ASIDs.
	 */
	uint64_t asid = val >> TLBI_ASID_SHIFT;
	if (asid == 0) {
		asm volatile ("tlbi vmalle1is");
		return;
	}
	val = val & ~(1ULL << TLBI_ASID_SHIFT);
	asm volatile ("tlbi aside1is, %0" : : "r"(val));
	val = val | (1ULL << TLBI_ASID_SHIFT);
#endif /* __ARM_KERNEL_PROTECT__ */
#if HAS_FEAT_XS
	if (__probable(!strong)) {
		asm volatile ("tlbi aside1isnxs, %0" : : "r"(val));
	} else
#endif /* HAS_FEAT_XS */
	{
		asm volatile ("tlbi aside1is, %0" : : "r"(val));
	}
}

static inline void
flush_mmu_tlb_asid(uint64_t val, bool strong)
{
	flush_mmu_tlb_asid_async(val, strong);
	arm64_sync_tlb(strong);
}

// flush_core_tlb_asid: flush all entries that match an ASID, local core only
// ASID must be in high 16 bits of argument
// Will not flush global entries
static inline void
flush_core_tlb_asid_async(uint64_t val)
{
#if __ARM_KERNEL_PROTECT__
	/*
	 * If we are flushing ASID 0, this is a kernel operation.  With this
	 * ASID scheme, this means we should flush all ASIDs.
	 */
	uint64_t asid = val >> TLBI_ASID_SHIFT;
	if (asid == 0) {
		asm volatile ("tlbi vmalle1");
		return;
	}
	val = val & ~(1ULL << TLBI_ASID_SHIFT);
	asm volatile ("tlbi aside1, %0" : : "r"(val));
	val = val | (1ULL << TLBI_ASID_SHIFT);
#endif /* __ARM_KERNEL_PROTECT__ */
#if HAS_FEAT_XS
	asm volatile ("tlbi aside1nxs, %0" : : "r"(val));
#else
	asm volatile ("tlbi aside1, %0" : : "r"(val));
#endif /* HAS_FEAT_XS */
}

static inline void
flush_core_tlb_asid(uint64_t val)
{
	flush_core_tlb_asid_async(val);
	sync_tlb_flush_local();
}

#if __ARM_RANGE_TLBI__
#if __ARM_KERNEL_PROTECT__
	#error __ARM_RANGE_TLBI__ + __ARM_KERNEL_PROTECT__ is not currently supported
#endif

#define ARM64_TLB_RANGE_MIN_PAGES 2
#define ARM64_TLB_RANGE_MAX_PAGES (1ULL << 21)
#define rtlbi_addr(x, shift) (((x) >> (shift)) & RTLBI_ADDR_MASK)
#define rtlbi_scale(x) ((uint64_t)(x) << RTLBI_SCALE_SHIFT)
#define rtlbi_num(x) ((uint64_t)(x) << RTLBI_NUM_SHIFT)

/**
 * Given the number of pages to invalidate, generate the correct parameter to
 * pass to any of the TLBI by range methods.
 */
static inline uint64_t
generate_rtlbi_param(ppnum_t npages, uint32_t asid, vm_offset_t va, uint64_t pmap_page_shift)
{
	assert(npages > 1);
	/**
	 * Per the armv8.4 RTLBI extension spec, the range encoded in the rtlbi register operand is defined by:
	 * BaseADDR <= VA < BaseADDR+((NUM+1)*2^(5*SCALE+1) * Translation_Granule_Size)
	 */
	unsigned order = (unsigned)(sizeof(npages) * 8) - (unsigned)__builtin_clz(npages - 1) - 1;
	unsigned scale = ((order ? order : 1) - 1) / 5;
	unsigned granule = 1 << ((5 * scale) + 1);
	unsigned num = (((npages + granule - 1) & ~(granule - 1)) / granule) - 1;
	return tlbi_asid(asid) | RTLBI_TG(pmap_page_shift) | rtlbi_scale(scale) | rtlbi_num(num) | rtlbi_addr(va, pmap_page_shift);
}

// flush_mmu_tlb_range: flush TLB entries that map a VA range using a single instruction
// The argument should be encoded according to generate_rtlbi_param().
// Follows the same ASID matching behavior as flush_mmu_tlb_entries()
static inline void
flush_mmu_tlb_range_async(uint64_t val, bool last_level_only, bool strong __unused)
{
	if (last_level_only) {
#if HAS_FEAT_XS
		if (__probable(!strong)) {
			asm volatile ("tlbi rvale1isnxs, %0" : : "r"(val));
		} else
#endif /* HAS_FEAT_XS */
		{
			asm volatile ("tlbi rvale1is, %0" : : "r"(val));
		}
	} else {
#if HAS_FEAT_XS
		if (__probable(!strong)) {
			asm volatile ("tlbi rvae1isnxs, %0" : : "r"(val));
		} else
#endif /* HAS_FEAT_XS */
		{
			asm volatile ("tlbi rvae1is, %0" : : "r"(val));
		}
	}
}

static inline void
flush_mmu_tlb_range(uint64_t val, bool last_level_only, bool strong)
{
	flush_mmu_tlb_range_async(val, last_level_only, strong);
	arm64_sync_tlb(strong);
}

// flush_mmu_tlb_allrange: flush TLB entries that map a VA range using a single instruction
// The argument should be encoded according to generate_rtlbi_param().
// Follows the same ASID matching behavior as flush_mmu_tlb_allentries()
static inline void
flush_mmu_tlb_allrange_async(uint64_t val, bool last_level_only, bool strong __unused)
{
	if (last_level_only) {
#if HAS_FEAT_XS
		if (__probable(!strong)) {
			asm volatile ("tlbi rvaale1isnxs, %0" : : "r"(val));
		} else
#endif /* HAS_FEAT_XS */
		{
			asm volatile ("tlbi rvaale1is, %0" : : "r"(val));
		}
	} else {
#if HAS_FEAT_XS
		if (__probable(!strong)) {
			asm volatile ("tlbi rvaae1isnxs, %0" : : "r"(val));
		} else
#endif /* HAS_FEAT_XS */
		{
			asm volatile ("tlbi rvaae1is, %0" : : "r"(val));
		}
	}
}

static inline void
flush_mmu_tlb_allrange(uint64_t val, bool last_level_only, bool strong)
{
	flush_mmu_tlb_allrange_async(val, last_level_only, strong);
	arm64_sync_tlb(strong);
}

// flush_core_tlb_allrange: flush TLB entries that map a VA range using a single instruction, local core only
// The argument should be encoded according to generate_rtlbi_param().
// Follows the same ASID matching behavior as flush_mmu_tlb_allentries()
static inline void
flush_core_tlb_allrange_async(uint64_t val)
{
#if HAS_FEAT_XS
	asm volatile ("tlbi rvaae1nxs, %0" : : "r"(val));
#else
	asm volatile ("tlbi rvaae1, %0" : : "r"(val));
#endif /* HAS_FEAT_XS */
}

static inline void
flush_core_tlb_allrange(uint64_t val)
{
	flush_core_tlb_allrange_async(val);
	sync_tlb_flush_local();
}

#endif // __ARM_RANGE_TLBI__

