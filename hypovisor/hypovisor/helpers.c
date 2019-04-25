#include "common.h"
#include "vmx.h"
#include "msr.h"

void fill_guest_selector_data(	__in PVOID gdt_base,__in ULONG reg, __in USHORT selector)
{
	SEGMENT_SELECTOR seg_selector = { 0 };
	ULONG            uAccessRights;

	get_segment_descriptor(&seg_selector, selector, gdt_base);
	uAccessRights = ((PUCHAR)& seg_selector.ATTRIBUTES)[0] + (((PUCHAR)& seg_selector.ATTRIBUTES)[1] << 12);

	if (!selector)
		uAccessRights |= 0x10000;

	__vmx_vmwrite(GUEST_ES_SELECTOR + reg * 2, selector);
	__vmx_vmwrite(GUEST_ES_LIMIT + reg * 2, seg_selector.LIMIT);
	__vmx_vmwrite(GUEST_ES_AR_BYTES + reg * 2, uAccessRights);
	__vmx_vmwrite(GUEST_ES_BASE + reg * 2, seg_selector.BASE);
}


BOOLEAN get_segment_descriptor(IN PSEGMENT_SELECTOR seg_selector, IN USHORT selector, IN PUCHAR gdt_base)
{
	PSEGMENT_DESCRIPTOR seg_desc;

	if (!seg_selector)
		return FALSE;

	if (selector & 0x4) {
		return FALSE;
	}

	seg_desc = (PSEGMENT_DESCRIPTOR)((PUCHAR)gdt_base + (selector & ~0x7));

	seg_selector->SEL = selector;
	seg_selector->BASE = seg_desc->BASE0 | seg_desc->BASE1 << 16 | seg_desc->BASE2 << 24;
	seg_selector->LIMIT = seg_desc->LIMIT0 | (seg_desc->LIMIT1ATTR1 & 0xf) << 16;
	seg_selector->ATTRIBUTES.UCHARs = seg_desc->ATTR0 | (seg_desc->LIMIT1ATTR1 & 0xf0) << 4;

	if (!(seg_desc->ATTR0 & 0x10)) { // LA_ACCESSED
		ULONG64 tmp;
		// this is a TSS or callgate etc, save the base high part
		tmp = (*(PULONG64)((PUCHAR)seg_desc + 8));
		seg_selector->BASE = (seg_selector->BASE & 0xffffffff) | (tmp << 32);
	}

	if (seg_selector->ATTRIBUTES.Fields.G) {
		// 4096-bit granularity is enabled for this segment, scale the limit
		seg_selector->LIMIT = (seg_selector->LIMIT << 12) + 0xfff;
	}

	return TRUE;
}

ULONG AdjustControls(IN ULONG ctl, IN ULONG msr)
{
	MSR value = { 0 };

	value.Content = __readmsr(msr);
	ctl &= value.High;     /* bit == 0 in high word ==> must be zero */
	ctl |= value.Low;      /* bit == 1 in low word  ==> must be one  */
	return ctl;
}