/**************************************************************************************************
 *
 * Copyright (c) 2019-2023 Axera Semiconductor (Shanghai) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Shanghai) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Shanghai) Co., Ltd.
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
#include <linux/dma-mapping.h>
#include <linux/of_device.h>

#define USE_DMA_ALLOC

static struct miscdevice ax_mmb_miscdev;
static u64 test_dma_mask = 0x4ffffffff;

#define AX_IOC_PCIe_BASE           'H'
#define PCIE_DMA_GET_PHY_BASE    _IOW(AX_IOC_PCIe_BASE, 3, unsigned int)
#define AX_IOC_PCIe_ALLOC_MEMORY   _IOW(AX_IOC_PCIe_BASE, 4, unsigned int)

#define PCIe_DEBUG  4
#define PCIe_INFO   3
#define PCIe_ERR    2
#define PCIe_FATAL  1
#define PCIe_CURR_LEVEL 3
#define PCIe_PRINT(level, s, params...) do { \
	if (level <= PCIe_CURR_LEVEL)  \
	printk("[%s, %d]: " s "", __func__, __LINE__, ##params); \
} while (0)

struct ax_mmb_buf {
	void *ax_mmb_buf;
	dma_addr_t dma_phy_addr;
	unsigned long dma_buf_size;
};

static int ax_mmb_open(struct inode *inode, struct file *fp)
{
	fp->private_data = 0;

	PCIe_PRINT(PCIe_DEBUG, "open mmb(MultiMedia buffer) dev\n");

	return 0;
}

static int ax_mmb_release(struct inode *inode, struct file *fp)
{
	struct ax_mmb_buf *ax_mmb = (void *)fp->private_data;
	PCIe_PRINT(PCIe_DEBUG, "release mmb\n");

	if (ax_mmb) {
#ifdef USE_DMA_ALLOC
		dma_free_coherent(ax_mmb_miscdev.this_device, ax_mmb->dma_buf_size,
				  ax_mmb->ax_mmb_buf, ax_mmb->dma_phy_addr);
#else
		ClearPageReserved(virt_to_page(ax_mmb->ax_mmb_buf));
#endif
		kfree(ax_mmb);
	}

	return 0;
}

static long ax_mmb_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct ax_mmb_buf *ax_mmb;
	dma_addr_t dma_phy_addr;
	unsigned long dma_buf_size;

	switch (cmd) {
	case PCIE_DMA_GET_PHY_BASE:
		PCIe_PRINT(PCIe_DEBUG,
			   "ax_mmb_ioctl: get video buffer base phy addr\n");
		ax_mmb = (void *)fp->private_data;
		dma_phy_addr = ax_mmb->dma_phy_addr;
		if (copy_to_user
		    ((void *)arg, (void *)&dma_phy_addr, sizeof(long))) {
			PCIe_PRINT(PCIe_ERR,
				   "copy data to usr space failed.\n");
			return -1;
		}
		break;
	case AX_IOC_PCIe_ALLOC_MEMORY:
		ret =
		    copy_from_user((void *)&dma_buf_size, (void *)arg,
				   sizeof(long));
		if (ret != 0) {
			PCIe_PRINT(PCIe_ERR, "copy_from_user fail\n");
			return EINVAL;
		}
		PCIe_PRINT(PCIe_DEBUG,
			   "ax_mmb_ioctl: alloc physical memory, size:%ld\n",
			   dma_buf_size);
		ax_mmb = kmalloc(sizeof(struct ax_mmb_buf), GFP_KERNEL);
		if (!ax_mmb) {
			printk("ax_mmb kmalloc failed\n");
			return EINVAL;
		}
		fp->private_data = (void *)ax_mmb;
		ax_mmb->dma_buf_size = dma_buf_size;
#ifdef USE_DMA_ALLOC
		ax_mmb->ax_mmb_buf =
		    dma_alloc_coherent(ax_mmb_miscdev.this_device, dma_buf_size,
				       (dma_addr_t *)(&ax_mmb->dma_phy_addr), GFP_KERNEL);
		if (ax_mmb->ax_mmb_buf <= 0) {
			PCIe_PRINT(PCIe_ERR, "dma_alloc_coherent failed\n");
			kfree(ax_mmb);
			return -1;
		} else {
			PCIe_PRINT(PCIe_INFO,
				   "ax_mmb_buf:%lx, dma_phy_addr: %llx\n",
				   (long)ax_mmb->ax_mmb_buf, ax_mmb->dma_phy_addr);
			memset(ax_mmb->ax_mmb_buf, 0, dma_buf_size);
			memcpy(ax_mmb->ax_mmb_buf, "abcdefghijk", 12);
		}
#else
		ax_mmb->ax_mmb_buf = (unsigned char *)kmalloc(dma_buf_size, GFP_KERNEL);
		if (ax_mmb->ax_mmb_buf == NULL) {
			PCIe_PRINT(PCIe_ERR, "alloc mmb buffer failed\n");
			kfree(ax_mmb);
			return -1;
		}
		SetPageReserved(virt_to_page(ax_mmb->ax_mmb_buf));
		ax_mmb->dma_phy_buf = virt_to_phys(ax_mmb->ax_mmb_buf);
		PCIe_PRINT(PCIe_INFO, "ax_mmb_buf:%lx, dma_phy_addr: %lx\n",
			   (long)ax_mmb->ax_mmb_buf, ax_mmb->dma_phy_buf);
		memcpy(ax_mmb->ax_mmb_buf, "0123456789", 11);
#endif
		break;

	default:
		PCIe_PRINT(PCIe_INFO, "unknown ioctl cmd\n");
		return -ENOIOCTLCMD;
	}

	return ret;
}

static int ax_mmb_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ax_mmb_buf *ax_mmb = (void *)file->private_data;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pfn_start = (ax_mmb->dma_phy_addr >> PAGE_SHIFT) + vma->vm_pgoff;
	unsigned long virt_start = (unsigned long)(ax_mmb->ax_mmb_buf) + offset;
	unsigned long size = vma->vm_end - vma->vm_start;
	int ret = 0;

	PCIe_PRINT(PCIe_DEBUG,
		   "ax_mmb_mmap: phy: 0x%lx, offset: 0x%lx, size: 0x%lx\n",
		   pfn_start << PAGE_SHIFT, offset, size);

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	ret =
	    remap_pfn_range(vma, vma->vm_start, pfn_start, size,
			    vma->vm_page_prot);
	if (ret)
		PCIe_PRINT(PCIe_ERR,
			   "%s: remap_pfn_range failed at [0x%lx  0x%lx]\n",
			   __func__, vma->vm_start, vma->vm_end);
	else
		PCIe_PRINT(PCIe_DEBUG, "%s: map 0x%lx to 0x%lx, size: 0x%lx\n",
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

	PCIe_PRINT(PCIe_DEBUG, "axera mmb(MultiMedia buffer) init\n");

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
	PCIe_PRINT(PCIe_DEBUG, "axera mmb exit\n");

	misc_deregister(&ax_mmb_miscdev);
}

module_init(ax_mmb_init);
module_exit(ax_mmb_exit);

MODULE_AUTHOR("AXERA");
MODULE_DESCRIPTION("AXERA AX650 MultiMedia Buffer");
MODULE_LICENSE("GPL");
