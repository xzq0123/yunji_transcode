// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022-2023 DeepX, Inc. and/or its affiliates.
 * DeepX eDMA PCIe driver
 *
 * Author: Taegyun An <atg@deepx.ai>
 */

#include <linux/poll.h>

#include "dx_cdev.h"
#include "dx_lib.h"
/*
 * character device file operations for events
 */
static ssize_t char_events_read(struct file *file, char __user *buf,
		size_t count, loff_t *pos)
{
	struct dx_dma_cdev *xcdev = (struct dx_dma_cdev *)file->private_data;
	struct dx_dma_user_irq *user_irq;
	unsigned long flags;
	u32 events_user;
	int rv;

	dbg_sg("[%s] name:%s, idx:%d\n",
		__func__, xcdev->user_irq->name, xcdev->user_irq->user_idx);
	
	rv = dx_cdev_check(__func__, xcdev, 0);
	if (rv < 0)
		return rv;

	user_irq = xcdev->user_irq;
	if (!user_irq) {
		pr_info("xcdev 0x%p, user_irq NULL.\n", xcdev);
		return -EINVAL;
	}

	if (count != 4)
		return -EPROTO;

	if (*pos & 3)
		return -EPROTO;

	/*
	 * sleep until any interrupt events have occurred,
	 * or a signal arrived
	 */
	rv = wait_event_interruptible(user_irq->events_wq,
			user_irq->events_irq != 0);
	if (rv)
		pr_info("wait_event_interruptible=%d\n", rv);

	/* wait_event_interruptible() was interrupted by a signal */
	if (rv == -ERESTARTSYS)
		return -ERESTARTSYS;

	/* atomically decide which events are passed to the user */
	spin_lock_irqsave(&user_irq->events_lock, flags);
	events_user = user_irq->events_irq;
	user_irq->events_irq = 0;
	spin_unlock_irqrestore(&user_irq->events_lock, flags);

	rv = copy_to_user(buf, &events_user, 4);
	if (rv)
		pr_info("Copy to user failed but continuing\n");

	return 4;
}

static unsigned int char_events_poll(struct file *file, poll_table *wait)
{
	struct dx_dma_cdev *xcdev = (struct dx_dma_cdev *)file->private_data;
	struct dx_dma_user_irq *user_irq;
	unsigned long flags;
	unsigned int mask = 0;
	int rv;

	dbg_sg("[%s] name:%s, idx:%d\n",
		__func__, xcdev->user_irq->name, xcdev->user_irq->user_idx);

	rv = dx_cdev_check(__func__, xcdev, 0);
	if (rv < 0)
		return rv;
	user_irq = xcdev->user_irq;
	if (!user_irq) {
		pr_info("xcdev 0x%p, user_irq NULL.\n", xcdev);
		return -EINVAL;
	}

	poll_wait(file, &user_irq->events_wq, wait);

	spin_lock_irqsave(&user_irq->events_lock, flags);
	if (user_irq->events_irq)
		mask = POLLIN | POLLRDNORM;	/* readable */

	spin_unlock_irqrestore(&user_irq->events_lock, flags);

	return mask;
}

/*
 * character device file operations for the irq events
 */
static const struct file_operations events_fops = {
	.owner = THIS_MODULE,
	.open = char_open,
	.release = char_close,
	.read = char_events_read,
	.poll = char_events_poll,
};

void dx_cdev_event_init(struct dx_dma_cdev *xcdev, u8 idx)
{
	u8 s_idx = xcdev->xpdev->dw->dma_irqs;
	int pos;

	/* Single Interrupt handler */
	if (xcdev->xpdev->dw->nr_irqs == 1) {
		dbg_init("[One Handler] User IRQ is registerd about index:%d, dma irqs:%d\n", idx, s_idx);
		xcdev->user_irq = &(xcdev->xpdev->dw->irq[0].user_irqs[idx]);
	} else {
		if (check_event_id(idx)) {
			dbg_init("[Multi Handler] User IRQ is registerd about index:%d, dma irqs:%d\n", idx, s_idx);
			pos = get_pos_user_irqs(idx);
			if (pos == -1) {
				pr_err("Please Check user irqs [idx:%d, pos:%d]\n", idx, pos);
			}
			xcdev->user_irq = &(xcdev->xpdev->dw->irq[s_idx + pos].user_irq);
		} else {
			xcdev->user_irq = &(xcdev->xpdev->dw->irq[0].user_irq);
		}
	}
	cdev_init(&xcdev->cdev, &events_fops);
}
