// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022-2023 DeepX, Inc. and/or its affiliates.
 * DeepX eDMA PCIe driver
 *
 * Author: Taegyun An <atg@deepx.ai>
 */

#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>

#include <linux/scatterlist.h>
#include <asm/cacheflush.h>
#include <linux/delay.h>

#include "dx_sgdma_cdev.h"
#include "dx_lib.h"
#include "dw-edma-thread.h"
#include "dw-edma-v0-core.h"
#include "dx_util.h"

#define IOCTL_PRINT 1

int dx_sgdma_cdev_open(struct inode *inode, struct file *filp);
int dx_sgdma_cdev_release(struct inode *inode, struct file *filp);
ssize_t dx_sgdma_cdev_read(struct file *filp, char __user *buf, size_t count, loff_t *pos);
ssize_t dx_sgdma_cdev_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos);
long dx_sgdma_cdev_ioctl(struct file *filp, unsigned int cmd, unsigned long data);
static loff_t dx_sgdma_cdev_llseek(struct file *file, loff_t off, int whence);

/*
 * character device file operations for SG DMA engine
 */
static loff_t dx_sgdma_cdev_llseek(struct file *file, loff_t off, int whence)
{
	loff_t newpos = 0;

	switch (whence) {
	case 0: /* SEEK_SET */
		newpos = off;
		break;
	case 1: /* SEEK_CUR */
		newpos = file->f_pos + off;
		break;
	case 2: /* SEEK_END, @TODO should work from end of address space */
		newpos = UINT_MAX + off;
		break;
	default: /* can't happen */
		return -EINVAL;
	}
	if (newpos < 0)
		return -EINVAL;
	file->f_pos = newpos;
	dbg_sg("%s: pos=0x%llx\n", __func__, (signed long long)newpos);

#if 0
	pr_err("0x%p, off %lld, whence %d -> pos %lld.\n",
		file, (signed long long)off, whence, (signed long long)off);
#endif

	return newpos;
}

static void char_sgdma_unmap_user_buf(struct dx_dma_io_cb *cb, bool write)
{
	int i;

	// sg_free_table(&cb->sgt);

	if (!cb->pages || !cb->pages_nr)
		return;

	for (i = 0; i < cb->pages_nr; i++) {
		if (cb->pages[i]) {
			if (!write)
				set_page_dirty_lock(cb->pages[i]);
			put_page(cb->pages[i]);
		} else
			break;
	}

	if (i != cb->pages_nr)
		pr_info("sgl pages %d/%u.\n", i, cb->pages_nr);

	kfree(cb->pages);
	cb->pages = NULL;
}

static int char_sgdma_map_user_buf_to_sgl(struct dx_dma_io_cb *cb, bool write, int dev_n, int dma_n)
{
	struct sg_table *sgt = &cb->sgt;
	unsigned long len = cb->len;
	void __user *buf = cb->buf;
	struct scatterlist *sg;
	unsigned int pages_nr = (((unsigned long)buf + len + PAGE_SIZE - 1) -
				 ((unsigned long)buf & PAGE_MASK))
				>> PAGE_SHIFT;
	int i;
	int rv;

	if (pages_nr == 0)
		return -EINVAL;

	dx_pcie_start_profile(PCIE_SG_TABLE_ALLOC_T, 0, dev_n, dma_n, write);
	if (sg_alloc_table(sgt, pages_nr, GFP_KERNEL)) {
		pr_err("sgl OOM.\n");
		return -ENOMEM;
	}
	dx_pcie_end_profile(PCIE_SG_TABLE_ALLOC_T, 0, dev_n, dma_n, write);

	cb->pages = kcalloc(pages_nr, sizeof(struct page *), GFP_KERNEL);
	if (!cb->pages) {
		pr_err("pages OOM.\n");
		rv = -ENOMEM;
		goto err_out;
	}
	dx_pcie_start_profile(PCIE_USER_PG_TO_PHY_MAPPING_T, 0, dev_n, dma_n, write);

	/* get physical pages from user pages */
	rv = get_user_pages_fast((unsigned long)buf, pages_nr, 1/* write */,
				cb->pages);

	/* No pages were pinned */
	if (rv < 0) {
		pr_err("unable to pin down %u user pages, %d.\n",
			pages_nr, rv);
		goto err_out;
	}
	/* Less pages pinned than wanted */
	if (rv != pages_nr) {
		pr_err("unable to pin down all %u user pages, %d.\n",
			pages_nr, rv);
		cb->pages_nr = rv;
		rv = -EFAULT;
		goto err_out;
	}

	for (i = 1; i < pages_nr; i++) {
		if (cb->pages[i - 1] == cb->pages[i]) {
			pr_err("duplicate pages, %d, %d.\n",
				i - 1, i);
			rv = -EFAULT;
			cb->pages_nr = pages_nr;
			goto err_out;
		}
	}

	sg = sgt->sgl;
	for (i = 0; i < pages_nr; i++, sg = sg_next(sg)) {
		unsigned int offset = offset_in_page(buf);
		unsigned int nbytes =
			min_t(unsigned int, PAGE_SIZE - offset, len);

		flush_dcache_page(cb->pages[i]);
		sg_set_page(sg, cb->pages[i], nbytes, offset);

		buf += nbytes;
		len -= nbytes;

		// pr_info("SG#%d Address:0x%llx, Page:%x, nbytes:%x\n",
		// 	i, sg_dma_address(sg), sg->page_link, nbytes);
	}
	dx_pcie_end_profile(PCIE_USER_PG_TO_PHY_MAPPING_T, 0, dev_n, dma_n, write);

	if (len) {
		pr_err("Invalid user buffer length. Cannot map to sgl\n");
		return -EINVAL;
	}
	cb->pages_nr = pages_nr;

	return 0;

err_out:
	char_sgdma_unmap_user_buf(cb, write);

	return rv;
}

/* when open() is called */
int dx_sgdma_cdev_open(struct inode *inode, struct file *filp) {
	struct dx_dma_cdev *xcdev;
	struct dw_edma *dw;

	char_open(inode, filp);
	xcdev = (struct dx_dma_cdev *)filp->private_data;
	dw = xcdev->xpdev->dw;
	xcdev->f_count++;

	dbg_sg("[%s][%s][%s] Device_#%d NPU_#%d called!(count:%d)\n", __func__,
		!xcdev->write ? "W" : "R",
		xcdev->sys_device->kobj.name,
		dw->idx, xcdev->npu_id,
		xcdev->f_count);
	/* TODO - Check device busy */

	/* DMA Channel allocation */
	if (xcdev->write) {
		dw_edma_dma_allocation(dw->rd_dma_id, xcdev->npu_id, &dw->wr_dma_chan[xcdev->npu_id]);
	} else {
		dw_edma_dma_allocation(dw->wr_dma_id, xcdev->npu_id, &dw->rd_dma_chan[xcdev->npu_id]);
	}

	return 0;
}

/* when last close() is called */
int dx_sgdma_cdev_release(struct inode *inode, struct file *filp) {
	struct dx_dma_cdev *xcdev = (struct dx_dma_cdev *)filp->private_data;
	struct dw_edma *dw = xcdev->xpdev->dw;
	xcdev->f_count--;

	dbg_sg("[%s][%s][%s] Device_#%d NPU_#%d called!(count:%d)\n", __func__,
		!xcdev->write ? "W" : "R",
		xcdev->sys_device->kobj.name,
		dw->idx, xcdev->npu_id,
		xcdev->f_count);

	// ret = dw_edma_get(dw->idx, xcdev->npu_id);
	if (xcdev->f_count == 0) {
		if (xcdev->write)
			dw_edma_dma_deallocation(&dw->wr_dma_chan[xcdev->npu_id]);
		else
			dw_edma_dma_deallocation(&dw->rd_dma_chan[xcdev->npu_id]);
	}

	return 0;
}

long dx_sgdma_cdev_ioctl(struct file *filp, unsigned int cmd, unsigned long data) {
	switch (cmd) {
	case IOCTL_PRINT:
		printk(KERN_ALERT "[%s] IOCTL_PRINT called!", __func__);
		break;
	default:
		printk(KERN_ALERT "[%s] unknown command!", __func__);
		break;
	}

	return 0;
}

/* Copy userspace buffer to kernel buffer  */
ssize_t dx_sgdma_cdev_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos) {
	struct dx_dma_cdev *xcdev = (struct dx_dma_cdev *)filp->private_data;
	struct dx_dma_io_cb cb;
	struct dw_edma *dw = xcdev->xpdev->dw;
	size_t ret;
	int rv;

	dx_pcie_start_profile(PCIE_KERNEL_EXEC_T, count, dw->idx, xcdev->npu_id, 1);

	dbg_sg("[W] Dev#%d, file 0x%p, priv 0x%p, buf 0x%p,%llu, pos 0x%llx, npu_id:%d\n",
		dw->idx, filp, filp->private_data, buf, (u64)count, (u64)*pos, xcdev->npu_id);

	if (!dw) {
		pr_err("[%s] priv pointer open error!(NULL)\n", __func__);
		return 0;
	}

	/*Check transfer align - TODO*/

	memset(&cb, 0, sizeof(struct dx_dma_io_cb));
	cb.buf = (char __user *)buf;
	cb.len = count;
	cb.ep_addr = (u64)*pos;
	cb.write = true;
	cb.dma_ch_id = dw->wr_dma_id;
	cb.npu_id = xcdev->npu_id;

	// dw_iatu_desc_region_check(dw, cb.ep_addr, cb.len);

	rv = char_sgdma_map_user_buf_to_sgl(&cb, cb.write, dw->idx, xcdev->npu_id);
	if (rv < 0)
		return rv;

	/*Transfer*/
	rv = dw_edma_run(&cb, dw->rd_dma_chan[xcdev->npu_id], dw->idx, 0);

	char_sgdma_unmap_user_buf(&cb, cb.write);

	/*check result*/
	if (rv == 0) {
		if(cb.result)
			ret = -ERESTARTSYS;
		else
			ret = count;
	} else {
		ret = rv;
	}

	dx_pcie_end_profile(PCIE_KERNEL_EXEC_T, count, dw->idx, xcdev->npu_id, 1);
	return ret;
}

/* copy kernel buffer to userspace buffer, saved by write */
ssize_t dx_sgdma_cdev_read(struct file *filp, char __user *buf, size_t count, loff_t *pos) {
	struct dx_dma_cdev *xcdev = (struct dx_dma_cdev *)filp->private_data;
	struct dx_dma_io_cb cb;
	struct dw_edma *dw = xcdev->xpdev->dw;
	const char __user *ubuf = buf;
	size_t ret;
	int rv;

	dx_pcie_start_profile(PCIE_KERNEL_EXEC_T, count, dw->idx, xcdev->npu_id, 0);

	dbg_sg("[R] Dev#%d file 0x%p, priv 0x%p, buf 0x%p,%llu, pos 0x%llx, npu_id:%d\n",
		dw->idx, filp, filp->private_data, ubuf, (u64)count, (u64)*pos, xcdev->npu_id);

	/*Check transfer align - TODO*/

	memset(&cb, 0, sizeof(struct dx_dma_io_cb));
	cb.buf = (char __user *)ubuf;
	cb.len = count;
	cb.ep_addr = (u64)*pos;
	cb.write = false;
	cb.dma_ch_id = dw->rd_dma_id;
	cb.npu_id = xcdev->npu_id;

	rv = char_sgdma_map_user_buf_to_sgl(&cb, cb.write, dw->idx, xcdev->npu_id);
	if (rv < 0)
		return rv;

	/*Transfer*/
	rv = dw_edma_run(&cb, dw->wr_dma_chan[xcdev->npu_id], dw->idx, 1);

	char_sgdma_unmap_user_buf(&cb, cb.write);

	/*check result*/
	if (rv == 0) {
		if(cb.result)
			ret = -ERESTARTSYS;
		else
			ret = count;
	} else {
		ret = rv;
	}

	dx_pcie_end_profile(PCIE_KERNEL_EXEC_T, count, dw->idx, xcdev->npu_id, 0);
	return ret;
}

static const struct file_operations sgdma_fops = {
  .owner           = THIS_MODULE,
  .read            = dx_sgdma_cdev_read,
  .write           = dx_sgdma_cdev_write,
  .open            = dx_sgdma_cdev_open,
  .release         = dx_sgdma_cdev_release,
  .unlocked_ioctl  = dx_sgdma_cdev_ioctl,
  .llseek          = dx_sgdma_cdev_llseek,
};

void dx_cdev_sgdma_init(struct dx_dma_cdev *xcdev)
{
	cdev_init(&xcdev->cdev, &sgdma_fops);
}
