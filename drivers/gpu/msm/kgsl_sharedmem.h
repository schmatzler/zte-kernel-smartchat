/* Copyright (c) 2002,2007-2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef __KGSL_SHAREDMEM_H
#define __KGSL_SHAREDMEM_H

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include "kgsl_mmu.h"
#include <linux/slab.h>
#include <linux/kmemleak.h>

struct kgsl_device;
struct kgsl_process_private;

#define KGSL_CACHE_OP_INV       0x01
#define KGSL_CACHE_OP_FLUSH     0x02
#define KGSL_CACHE_OP_CLEAN     0x03

/** Set if the memdesc describes cached memory */
#define KGSL_MEMFLAGS_CACHED    0x00000001

extern struct kgsl_memdesc_ops kgsl_page_alloc_ops;

int kgsl_sharedmem_page_alloc(struct kgsl_memdesc *memdesc,
			   struct kgsl_pagetable *pagetable, size_t size);

int kgsl_sharedmem_page_alloc_user(struct kgsl_memdesc *memdesc,
				struct kgsl_pagetable *pagetable,
				size_t size, int flags);

int kgsl_sharedmem_alloc_coherent(struct kgsl_memdesc *memdesc, size_t size);

int kgsl_sharedmem_ebimem_user(struct kgsl_memdesc *memdesc,
			     struct kgsl_pagetable *pagetable,
			     size_t size, int flags);

int kgsl_sharedmem_ebimem(struct kgsl_memdesc *memdesc,
			struct kgsl_pagetable *pagetable,
			size_t size);

void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc);

int kgsl_sharedmem_readl(const struct kgsl_memdesc *memdesc,
			uint32_t *dst,
			unsigned int offsetbytes);

int kgsl_sharedmem_writel(const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes,
			uint32_t src);

int kgsl_sharedmem_set(const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes, unsigned int value,
			unsigned int sizebytes);

void kgsl_cache_range_op(struct kgsl_memdesc *memdesc, int op);

void kgsl_process_init_sysfs(struct kgsl_process_private *private);
void kgsl_process_uninit_sysfs(struct kgsl_process_private *private);

int kgsl_sharedmem_init_sysfs(void);
void kgsl_sharedmem_uninit_sysfs(void);

static inline unsigned int kgsl_get_sg_pa(struct scatterlist *sg)
{
	/*
	 * Try sg_dma_address first to support ion carveout
	 * regions which do not work with sg_phys().
	 */
	unsigned int pa = sg_dma_address(sg);
	if (pa == 0)
		pa = sg_phys(sg);
	return pa;
}

int
kgsl_sharedmem_map_vma(struct vm_area_struct *vma,
			const struct kgsl_memdesc *memdesc);

/*
 * For relatively small sglists, it is preferable to use kzalloc
 * rather than going down the vmalloc rat hole.  If the size of
 * the sglist is < PAGE_SIZE use kzalloc otherwise fallback to
 * vmalloc
 */

static inline void *kgsl_sg_alloc(unsigned int sglen)
{
	if ((sglen * sizeof(struct scatterlist)) <  PAGE_SIZE)
		return kzalloc(sglen * sizeof(struct scatterlist), GFP_KERNEL);
	else
		return vmalloc(sglen * sizeof(struct scatterlist));
}

static inline void kgsl_sg_free(void *ptr, unsigned int sglen)
{
	if ((sglen * sizeof(struct scatterlist)) < PAGE_SIZE)
		kfree(ptr);
	else
		vfree(ptr);
}

static inline int
memdesc_sg_phys(struct kgsl_memdesc *memdesc,
		unsigned int physaddr, unsigned int size)
{
	memdesc->sg = kgsl_sg_alloc(1);

	kmemleak_not_leak(memdesc->sg);

	memdesc->sglen = 1;
	sg_init_table(memdesc->sg, 1);
	memdesc->sg[0].length = size;
	memdesc->sg[0].offset = 0;
	memdesc->sg[0].dma_address = physaddr;
	return 0;
}

static inline int
kgsl_allocate(struct kgsl_memdesc *memdesc,
		struct kgsl_pagetable *pagetable, size_t size)
{
	if (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE)
		return kgsl_sharedmem_ebimem(memdesc, pagetable, size);
	return kgsl_sharedmem_page_alloc(memdesc, pagetable, size);
}

static inline int
kgsl_allocate_user(struct kgsl_memdesc *memdesc,
		struct kgsl_pagetable *pagetable,
		size_t size, unsigned int flags)
{
	if (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE)
		return kgsl_sharedmem_ebimem_user(memdesc, pagetable, size,
						  flags);
	return kgsl_sharedmem_page_alloc_user(memdesc, pagetable, size, flags);
}

static inline int
kgsl_allocate_contiguous(struct kgsl_memdesc *memdesc, size_t size)
{
	int ret  = kgsl_sharedmem_alloc_coherent(memdesc, size);
	if (!ret && (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE))
		memdesc->gpuaddr = memdesc->physaddr;
	return ret;
}

#endif /* __KGSL_SHAREDMEM_H */
