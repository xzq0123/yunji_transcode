/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <asm/io.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>
#include <linux/version.h>
#include <asm/cacheflush.h>

#if defined(CONFIG_ARM64)
extern void ax_dcache_clean_inval_poc(unsigned long start, unsigned long end);
extern void ax_dcache_inval_poc(unsigned long start, unsigned long end);
#endif

#define USE_DMA_ALLOC

static struct miscdevice ax_mmb_miscdev;
static u64 test_dma_mask = 0x4ffffffff;

#define AX_IOC_PCIe_BASE           'H'
#define PCIE_DMA_GET_PHY_BASE    _IOW(AX_IOC_PCIe_BASE, 3, unsigned int)
#define AX_IOC_PCIe_ALLOC_MEMORY   _IOW(AX_IOC_PCIe_BASE, 4, unsigned int)
#define AX_IOC_PCIe_ALLOC_MEMCACHED   _IOW(AX_IOC_PCIe_BASE, 5, unsigned int)
#define AX_IOC_PCIe_FLUSH_CACHED	_IOW(AX_IOC_PCIe_BASE, 6, struct ax_mem_info)
#define AX_IOC_PCIe_INVALID_CACHED	_IOW(AX_IOC_PCIe_BASE, 7, struct ax_mem_info)

#define PCIe_DEBUG  4
#define PCIe_INFO   3
#define PCIe_ERR    2
#define PCIe_FATAL  1
#define PCIe_CURR_LEVEL 2
#define PCIe_PRINT(level, s, params...) do { \
	if (level <= PCIe_CURR_LEVEL)  \
	printk("[%s, %d]: " s "\n", __func__, __LINE__, ##params); \
} while (0)

struct ax_mmb_buf {
	void *ax_mmb_buf;
	int mem_cached;
	dma_addr_t dma_phy_addr;
	unsigned long dma_buf_size;
};

struct ax_mem_info {
	uint64_t phy;
	uint64_t vir;
	uint64_t size;
};

static int ax_mmb_open(struct inode *inode, struct file *fp)
{
	fp->private_data = 0;

	PCIe_PRINT(PCIe_DEBUG, "open mmb(MultiMedia buffer) dev");

	return 0;
}

static int ax_mmb_release(struct inode *inode, struct file *fp)
{
	struct ax_mmb_buf *ax_mmb = (void *)fp->private_data;
	PCIe_PRINT(PCIe_DEBUG, "release mmb");

	if (ax_mmb) {
#ifdef USE_DMA_ALLOC
		dma_free_coherent(ax_mmb_miscdev.this_device,
				  ax_mmb->dma_buf_size, ax_mmb->ax_mmb_buf,
				  ax_mmb->dma_phy_addr);
#else
		ClearPageReserved(virt_to_page(ax_mmb->ax_mmb_buf));
#endif
		kfree(ax_mmb);
	}

	return 0;
}

static int ax_mmb_alloc_mem(struct ax_mmb_buf *ax_mmb, unsigned long arg)
{
	long ret;
	unsigned long dma_buf_size;

	ret = copy_from_user((void *)&dma_buf_size, (void *)arg, sizeof(long));
	if (ret != 0) {
		PCIe_PRINT(PCIe_ERR, "copy_from_user fail");
		return EINVAL;
	}
	PCIe_PRINT(PCIe_DEBUG,
		   "ax_mmb_ioctl: alloc physical memory, size:%ld",
		   dma_buf_size);
	ax_mmb->dma_buf_size = dma_buf_size;
#ifdef USE_DMA_ALLOC
	ax_mmb->ax_mmb_buf =
	    dma_alloc_coherent(ax_mmb_miscdev.this_device, dma_buf_size,
			       (dma_addr_t *) (&ax_mmb->dma_phy_addr),
			       GFP_KERNEL);
	if (ax_mmb->ax_mmb_buf <= 0) {
		PCIe_PRINT(PCIe_ERR, "dma_alloc_coherent failed");
		return -1;
	} else {
		PCIe_PRINT(PCIe_INFO,
			   "ax_mmb_buf:%lx, dma_phy_addr: %llx",
			   (long)ax_mmb->ax_mmb_buf, ax_mmb->dma_phy_addr);
		memset(ax_mmb->ax_mmb_buf, 0, dma_buf_size);
		memcpy(ax_mmb->ax_mmb_buf, "abcdefghijk", 12);
	}
#else
	ax_mmb->ax_mmb_buf = (unsigned char *)kmalloc(dma_buf_size, GFP_KERNEL);
	if (ax_mmb->ax_mmb_buf == NULL) {
		PCIe_PRINT(PCIe_ERR, "alloc mmb buffer failed");
		return -1;
	}
	SetPageReserved(virt_to_page(ax_mmb->ax_mmb_buf));
	ax_mmb->dma_phy_buf = virt_to_phys(ax_mmb->ax_mmb_buf);
	PCIe_PRINT(PCIe_INFO, "ax_mmb_buf:%lx, dma_phy_addr: %lx",
		   (long)ax_mmb->ax_mmb_buf, ax_mmb->dma_phy_buf);
	memcpy(ax_mmb->ax_mmb_buf, "0123456789", 11);
#endif
	return 0;
}

static int ax_mmb_alloc_mem_nocached(struct file *fp, unsigned long arg)
{
	struct ax_mmb_buf *ax_mmb;
	int ret;

	ax_mmb = kmalloc(sizeof(struct ax_mmb_buf), GFP_KERNEL);
	if (!ax_mmb) {
		PCIe_PRINT(PCIe_ERR, "ax_mmb kmalloc failed");
		return EINVAL;
	}
	ax_mmb->mem_cached = 0;
	ret = ax_mmb_alloc_mem(ax_mmb, arg);
	if (ret) {
		kfree(ax_mmb);
	}
	fp->private_data = (void *)ax_mmb;
	return ret;
}

static int ax_mmb_alloc_mem_cached(struct file *fp, unsigned long arg)
{
	struct ax_mmb_buf *ax_mmb;
	int ret;

	ax_mmb = kmalloc(sizeof(struct ax_mmb_buf), GFP_KERNEL);
	if (!ax_mmb) {
		PCIe_PRINT(PCIe_ERR, "ax_mmb kmalloc failed");
		return EINVAL;
	}
	ax_mmb->mem_cached = 1;
	ret = ax_mmb_alloc_mem(ax_mmb, arg);
	if (ret) {
		kfree(ax_mmb);
	}
	fp->private_data = (void *)ax_mmb;
	return ret;
}

static void _ax_mmb_invalidate_dcache_area(unsigned long addr, unsigned long size)
{
#if defined(CONFIG_ARM64)
	ax_dcache_inval_poc(addr, addr + size);
#endif
}

static int ax_mmb_invalidate_dcache_area(unsigned long long phyaddr, unsigned long long size)
{
	void *kvirt = NULL;

	kvirt = (void *)ioremap_cache(phyaddr, size);
	if (kvirt) {
		_ax_mmb_invalidate_dcache_area((unsigned long)kvirt, size);
		iounmap(kvirt);
	} else {
		PCIe_PRINT(PCIe_ERR, "invalidate cache failed,phyaddr=0x%llx,size=%lld.\n", phyaddr, size);
		return -1;
	}

	return 0;
}

static void ax_mmb_flush_dcache_area(unsigned long kvirt, unsigned long length)
{
#if defined(CONFIG_ARM64)
	ax_dcache_clean_inval_poc(kvirt, kvirt + length);
#endif
}

static long ax_mmb_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct ax_mmb_buf *ax_mmb;
	dma_addr_t dma_phy_addr;
	struct ax_mem_info meminfo;

	switch (cmd) {
	case PCIE_DMA_GET_PHY_BASE:
		PCIe_PRINT(PCIe_DEBUG,
			   "ax_mmb_ioctl: get video buffer base phy addr");
		ax_mmb = (void *)fp->private_data;
		dma_phy_addr = ax_mmb->dma_phy_addr;
		if (copy_to_user
		    ((void *)arg, (void *)&dma_phy_addr, sizeof(long))) {
			PCIe_PRINT(PCIe_ERR, "copy data to usr space failed");
			return -1;
		}
		break;
	case AX_IOC_PCIe_ALLOC_MEMCACHED:
		PCIe_PRINT(PCIe_DEBUG, "ax mmb alloc mem cached");
		ret = ax_mmb_alloc_mem_cached(fp, arg);
		if (ret) {
			PCIe_PRINT(PCIe_ERR,
				   "ax mmb alloc mem nocached failed");
			return -1;
		}
		break;
	case AX_IOC_PCIe_ALLOC_MEMORY:
		PCIe_PRINT(PCIe_DEBUG, "ax mmb alloc mem nocached");
		ret = ax_mmb_alloc_mem_nocached(fp, arg);
		if (ret) {
			PCIe_PRINT(PCIe_ERR, "ax mmb alloc mem cached failed");
		}
		break;
	case AX_IOC_PCIe_INVALID_CACHED:
		if (copy_from_user
		    ((void *)&meminfo, (void *)arg,
		     sizeof(struct ax_mem_info))) {
			PCIe_PRINT(PCIe_ERR, "copy data to usr space failed.");
			return -1;
		}
		PCIe_PRINT(PCIe_DEBUG,
			   "invalid: meminfo.phy = 0x%llx, meminfo.vir = 0x%llx, meminfo.size = 0x%llx",
			   meminfo.phy, meminfo.vir, meminfo.size);
		ret = ax_mmb_invalidate_dcache_area(meminfo.phy, meminfo.size);
		break;
	case AX_IOC_PCIe_FLUSH_CACHED:
		if (copy_from_user
		    ((void *)&meminfo, (void *)arg,
		     sizeof(struct ax_mem_info))) {
			PCIe_PRINT(PCIe_ERR, "copy data to usr space failed.");
			return -1;
		}
		PCIe_PRINT(PCIe_DEBUG,
			   "flush: meminfo.phy = 0x%llx, meminfo.vir = 0x%llx, meminfo.size = 0x%llx",
			   meminfo.phy, meminfo.vir, meminfo.size);
		ax_mmb_flush_dcache_area(meminfo.vir, meminfo.size);
		break;
	default:
		PCIe_PRINT(PCIe_INFO, "unknown ioctl cmd");
		return -ENOIOCTLCMD;
	}

	return ret;
}

pgprot_t ax_mmb_mmap_pgprot_cached(pgprot_t prot)
{
#if defined(CONFIG_ARM64)
	return __pgprot(pgprot_val(prot)
			| PTE_VALID | PTE_DIRTY | PTE_AF);
#elif defined(CONFIG_X86)
	return pgprot_writecombine(prot);
#else
	return __pgprot(pgprot_val(prot) | L_PTE_PRESENT
			| L_PTE_YOUNG | L_PTE_DIRTY | L_PTE_MT_DEV_CACHED);
#endif

}

static int ax_mmb_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ax_mmb_buf *ax_mmb = (void *)file->private_data;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pfn_start =
	    (ax_mmb->dma_phy_addr >> PAGE_SHIFT) + vma->vm_pgoff;
	unsigned long virt_start = (unsigned long)(ax_mmb->ax_mmb_buf) + offset;
	unsigned long size = vma->vm_end - vma->vm_start;
	int ret = 0;

	PCIe_PRINT(PCIe_DEBUG,
		   "ax_mmb_mmap: phy: 0x%lx, offset: 0x%lx, size: 0x%lx",
		   pfn_start << PAGE_SHIFT, offset, size);

	if (ax_mmb->mem_cached) {
		vma->vm_page_prot =
		    ax_mmb_mmap_pgprot_cached(vma->vm_page_prot);
	} else {
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	}

	ret =
	    remap_pfn_range(vma, vma->vm_start, pfn_start, size,
			    vma->vm_page_prot);
	if (ret)
		PCIe_PRINT(PCIe_ERR,
			   "%s: remap_pfn_range failed at [0x%lx  0x%lx]",
			   __func__, vma->vm_start, vma->vm_end);
	else
		PCIe_PRINT(PCIe_DEBUG, "%s: map 0x%lx to 0x%lx, size: 0x%lx",
			   __func__, virt_start, vma->vm_start, size);

	return ret;
}

static struct file_operations ax_mmb_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ax_mmb_ioctl,
	.open = ax_mmb_open,
	.release = ax_mmb_release,
	.mmap = ax_mmb_mmap,
};

static struct miscdevice ax_mmb_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ax_mmb_dev",
	.fops = &ax_mmb_fops,
};

static int __init ax_mmb_init(void)
{
	int ret;

	PCIe_PRINT(PCIe_DEBUG, "axera mmb(MultiMedia buffer) init");

	ret = misc_register(&ax_mmb_miscdev);
	if (ret) {
		PCIe_PRINT(PCIe_ERR,
			   "Register axera MultiMedia Buffer Misc failed!");
		return -1;
	}

	/* configure dev->archdata.dma_ops = &swiotlb_dma_ops;
	 * or this dev's dma_ops is NULL, dma_alloc_coherent will fail */
	of_dma_configure(ax_mmb_miscdev.this_device, NULL, 1);
	//arch_setup_dma_ops(ax_mmb_miscdev.this_device, 0, 0, NULL,1);
	ax_mmb_miscdev.this_device->dma_mask = &test_dma_mask;
	ax_mmb_miscdev.this_device->coherent_dma_mask = test_dma_mask;

	return 0;
}

static void __exit ax_mmb_exit(void)
{
	PCIe_PRINT(PCIe_DEBUG, "axera mmb exit");

	misc_deregister(&ax_mmb_miscdev);
}

module_init(ax_mmb_init);
module_exit(ax_mmb_exit);

MODULE_AUTHOR("AXERA");
MODULE_DESCRIPTION("AXERA AX650 MultiMedia Buffer");
MODULE_LICENSE("GPL");
