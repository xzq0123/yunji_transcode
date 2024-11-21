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
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/time.h>
#include <linux/time64.h>
#include <uapi/linux/time.h>
#include <linux/sched.h>
#include <linux/firmware.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include "axcl_pcie_host.h"

#define AXCL_DUMP_ADDR_SRC	(0x100000000)

static struct semaphore ioctl_sem;
static struct task_struct *heartbeat[32];
ax_pcie_msg_handle_t *port_handle[AXERA_MAX_MAP_DEV][MAX_MSG_PORTS];

static dma_addr_t phys_addr;
static void *dma_virt_addr;

static spinlock_t g_axcl_lock;
unsigned int g_heartbeat_stop;
struct device_connect_status heartbeat_status;
unsigned int port_info[AXERA_MAX_MAP_DEV][MAX_MSG_PORTS] = { 0 };
struct axcl_device_info g_pttr[AXCL_PROCESS_MAX][AXERA_MAX_MAP_DEV] = { 0 };

struct process_wait g_pro_wait[AXCL_PROCESS_MAX];

static inline u32 pcie_dev_readl(struct axera_dev *ax_dev, u32 offset)
{
	return readl(ax_dev->bar_virt[0] + offset);
}

static inline void pcie_dev_writel(struct axera_dev *ax_dev,
				   u32 offset, u32 value)
{
	writel(value, ax_dev->bar_virt[0] + offset);
}

void axcl_devices_heartbeat_status_set(unsigned int target,
				       enum heartbeat_type status)
{
	heartbeat_status.status[target] = status;
}

enum heartbeat_type axcl_devices_heartbeat_status_get(unsigned int target)
{
	return heartbeat_status.status[target];
}

static int axcl_pcie_notifier_recv(void *handle, void *buf, unsigned int length)
{
	struct ax_transfer_handle *transfer_handle =
	    (struct ax_transfer_handle *)handle;
	ax_pcie_msg_handle_t *msg_handle;

	struct ax_mem_list *mem;
	void *data;
	unsigned long flags = 0;

	axcl_trace(AXCL_DEBUG, "nortifier_recv addr 0x%lx len 0x%x.",
		   (unsigned long int)buf, length);

	msg_handle = (ax_pcie_msg_handle_t *) transfer_handle->data;
	data = kmalloc(length + sizeof(struct ax_mem_list), GFP_ATOMIC);
	if (!data) {
		axcl_trace(AXCL_ERR, "Data kmalloc failed.");
		return -1;
	}

	mem = (struct ax_mem_list *)data;
	mem->data = data + sizeof(struct ax_mem_list);

	ax_pcie_msg_data_recv(transfer_handle, mem->data, length);
	mem->data_len = length;

	spin_lock_irqsave(&g_axcl_lock, flags);
	list_add_tail(&mem->head, &msg_handle->mem_list);
	spin_unlock_irqrestore(&g_axcl_lock, flags);

	return 0;
}

int axcl_pcie_msg_send(struct ax_transfer_handle *handle, void *kbuf,
		       unsigned int count)
{
	return ax_pcie_msg_send(handle, kbuf, count);
}

int axcl_pcie_recv(ax_pcie_msg_handle_t *handle, void *buf, size_t count)
{
	struct list_head *entry, *tmp;
	struct ax_mem_list *mem = NULL;
	unsigned long flags;
	unsigned int len = 0;

	if (!handle) {
		axcl_trace(AXCL_ERR, "handle is not open.");
		return -1;
	}

	spin_lock_irqsave(&g_axcl_lock, flags);

	if (!list_empty(&handle->mem_list)) {
		list_for_each_safe(entry, tmp, &handle->mem_list) {
			mem = list_entry(entry, struct ax_mem_list, head);
			if (mem == NULL)
				goto recv_err2;
			len = mem->data_len;
			if (len > count) {
				axcl_trace(AXCL_ERR,
					   "Message len[0x%x], read len[0x%lx]",
					   len, count);
				list_del(&mem->head);
				goto recv_err1;
			}

			list_del(&mem->head);
			break;
		}

		spin_unlock_irqrestore(&g_axcl_lock, flags);

		memcpy(buf, mem->data, len);

		kfree(mem);

		axcl_trace(AXCL_DEBUG, "read success 0x%x", len);

		return len;
	}

recv_err1:
	if (mem)
		kfree(mem);
recv_err2:
	spin_unlock_irqrestore(&g_axcl_lock, flags);
	return len;
}

int axcl_pcie_recv_timeout(ax_pcie_msg_handle_t *handle, void *buf,
			   size_t count, int timeout)
{
	struct timespec64 tv_start;
	struct timespec64 tv_end;
	unsigned int runtime;
	int ret;

	ktime_get_ts64(&tv_start);
	while (1) {
		ret = axcl_pcie_recv(handle, buf, count);
		if (ret)
			break;
		if (timeout == -1) {
			continue;
		} else if (timeout == 0) {
			break;
		} else {
			ktime_get_ts64(&tv_end);
			runtime =
			    ((tv_end.tv_sec - tv_start.tv_sec) * 1000000) +
			    ((tv_end.tv_nsec - tv_start.tv_nsec) / 1000);
			if (runtime > (timeout * 1000)) {
				ret = -2;
				break;
			}
		}
	}
	return ret;
}

int axcl_heartbeat_status(struct device_heart_packet *hbeat,
			  unsigned int target, unsigned long long count)
{
	if (hbeat->device == target) {
		if (hbeat->count >= (count + 1))
			return hbeat->count;
	}
	return 0;
}

int axcl_heartbeat_recv_timeout(struct device_heart_packet *hbeat,
				unsigned int target, unsigned long long count,
				int timeout)
{
	struct timespec64 tv_start;
	struct timespec64 tv_end;
	unsigned int runtime;
	int ret;

	ktime_get_ts64(&tv_start);
	while (1) {
		ret = axcl_heartbeat_status(hbeat, target, count);
		if (ret)
			break;
		if (timeout == -1) {
			continue;
		} else {
			ktime_get_ts64(&tv_end);
			runtime =
			    ((tv_end.tv_sec - tv_start.tv_sec) * 1000000) +
			    ((tv_end.tv_nsec - tv_start.tv_nsec) / 1000);
			if (runtime > (timeout * 1000)) {
				ret = -2;
				break;
			}
		}
		msleep(1000);
	}
	return ret;
}

int send_buf_alloc(struct axera_dev *ax_dev)
{
	struct device *dev = &ax_dev->pdev->dev;

	dma_virt_addr = dma_alloc_coherent(dev, MAX_TRANSFER_SIZE,
					   &phys_addr, GFP_KERNEL);
	if (!dma_virt_addr) {
		dev_err(dev, "Failed to allocate address\n");
		return -1;
	}
	return 0;
}

void free_buf_alloc(struct axera_dev *ax_dev)
{
	struct device *dev = &ax_dev->pdev->dev;

	dma_free_coherent(dev, MAX_TRANSFER_SIZE, dma_virt_addr, phys_addr);
}

int wait_device_recv_completion(struct axera_dev *ax_dev, unsigned int flags, unsigned int timeout)
{
	int ret = -1;
	unsigned int reg = 0;
	unsigned int runtime;
	struct timespec64 tv_start = { 0 };
	struct timespec64 tv_end = { 0 };

	ktime_get_ts64(&tv_start);
	while (1) {
		reg = pcie_dev_readl(ax_dev, AX_PCIE_ENDPOINT_STATUS);
		if (reg != 0) {
			if (reg & flags) {
				pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_STATUS,
						0x0);
				smp_mb();
				ret = 0;
				break;
			} else {
				pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_STATUS,
						0x0);
				smp_mb();
				ret = -EFAULT;
				break;
			}
		}
		if (timeout == -1) {
			continue;
		} else {
			ktime_get_ts64(&tv_end);
			runtime =
			    ((tv_end.tv_sec - tv_start.tv_sec) * 1000000) +
			    ((tv_end.tv_nsec - tv_start.tv_nsec) / 1000);
			if (runtime > (timeout * 1000)) {
				return -2;
			}
		}
	}
	return ret;
}

int axcl_get_dev_boot_reason(struct axera_dev *ax_dev)
{
	int boot_reason = 0;

	boot_reason = pcie_dev_readl(ax_dev, AX_PCIE_ENDPOINT_BOOT_REASON_TYPE);
	pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_BOOT_REASON_TYPE, 0x0);
	return boot_reason;
}

int axcl_dump_data(struct axera_dev *ax_dev, unsigned long src,
			    int size)
{
	pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_LOWER_SRC_ADDR,
			lower_32_bits(src));
	pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_UPPER_SRC_ADDR,
			upper_32_bits(src));
	pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_LOWER_DST_ADDR,
			lower_32_bits(phys_addr));
	pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_UPPER_DST_ADDR,
			upper_32_bits(phys_addr));
	pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_SIZE, size);
	pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_COMMAND,
			AX_PCIE_COMMAND_WRITE);

	return wait_device_recv_completion(ax_dev, STATUS_WRITE_SUCCESS, 6000);
}

void sysdumpheader(int boot_reason)
{
	printk("\n===================sysdump Header===================\n");
	printk("nBrs: %d\n", boot_reason);
	printk("nSrc: 0x%lx\n", AXCL_DUMP_ADDR_SRC);
	printk("nSize: %d\n", sysdump_ctrl.size);
	printk("Path: %s\n", sysdump_ctrl.path);
}

int axcl_device_sysdump(struct axera_dev *ax_dev)
{
	struct file *filp;
	loff_t pos = 0;
	struct timespec64 ts;
	struct tm tm;
	char filename[AXCL_NAME_LEN];
	int boot_reason;
	int dumpsize = sysdump_ctrl.size;
	int size;
	int ret;
	unsigned long src = AXCL_DUMP_ADDR_SRC;
	ssize_t bytes_write;

	boot_reason = axcl_get_dev_boot_reason(ax_dev);
	sysdumpheader(boot_reason);
	if (!boot_reason) {
		printk("[INFO]: DEV NORMAL, SKIP DUMP\n");
		return 0;
	}

	printk("[INFO]: DEV EXCEPTION, DUMP...\n");

	ktime_get_real_ts64(&ts);
	time64_to_tm(ts.tv_sec, 0, &tm);
	snprintf(filename, AXCL_NAME_LEN, "%s/%s_%x.%04ld%02d%02d%02d%02d%02d",
		sysdump_ctrl.path, AXCL_SYSDUMP_NAME, ax_dev->slot_index, tm.tm_year + 1900,
		tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	filp = filp_open(filename, O_RDWR | O_CREAT, 0777);
	if (IS_ERR(filp)) {
		axcl_trace(AXCL_ERR, "Failed to open file %s", filename);
		return PTR_ERR(filp);
	}

	while (dumpsize) {
		size = min_t(uint, dumpsize, MAX_TRANSFER_SIZE);
		ret = axcl_dump_data(ax_dev, src, size);
		if (ret < 0) {
			printk("[STATUS]: DUMP FAILED[%d]\n", ret);
			bytes_write = ret;
			goto out;
		}

		bytes_write = kernel_write(filp, dma_virt_addr, size, &pos);
		if (bytes_write < 0) {
			printk("[STATUS]: WRITE FAILED[%ld]\n", bytes_write);
			break;
		}

		src += size;
		dumpsize -= size;
	}

out:
	filp_close(filp, NULL);
	return bytes_write;
}

static void start_devices(struct axera_dev *ax_dev)
{
	pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_FINISH_STATUS,
			BOOT_START_DEVICES);
}

static int axcl_boot_device(struct axera_dev *ax_dev, unsigned long dest,
			    int size)
{
	pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_LOWER_SRC_ADDR,
			lower_32_bits(phys_addr));
	pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_UPPER_SRC_ADDR,
			upper_32_bits(phys_addr));
	pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_LOWER_DST_ADDR,
			lower_32_bits(dest));
	pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_UPPER_DST_ADDR,
			upper_32_bits(dest));
	pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_SIZE, size);
	pcie_dev_writel(ax_dev, AX_PCIE_ENDPOINT_COMMAND, AX_PCIE_COMMAND_READ);

	return wait_device_recv_completion(ax_dev, STATUS_READ_SUCCESS, 6000);
}

void dumppacheader(PAC_HEAD_T *pacHeader)
{
	printk("\n=====================PAC Header=====================\n");
	printk("nMagic: 0x%X\n", pacHeader->nMagic);
	printk("nPacVer: %d\n", pacHeader->nPacVer);
	printk("u64PacSize: %lld\n", pacHeader->u64PacSize);
	printk("szProdName: %s\n", pacHeader->szProdName);
	printk("szProdVer: %s\n", pacHeader->szProdVer);
	printk("nFileOffset: %d\n", pacHeader->nFileOffset);
	printk("nFileCount: %d\n", pacHeader->nFileCount);
	printk("nAuth: %d\n", pacHeader->nAuth);
	printk("crc16: 0x%X\n", pacHeader->crc16);
	printk("md5: 0x%X\n", pacHeader->md5);
}

void dumpImgFileHeader(PAC_FILE_T *imgHeader)
{
	printk("\n=====================IMG Header=====================\n");
	printk("szID: %s\n", imgHeader->szID);
	printk("szType: %s\n", imgHeader->szType);
	printk("szFile: %s\n", imgHeader->szFile);
	printk("u64CodeOffset: 0x%llX\n", imgHeader->u64CodeOffset);
	printk("u64CodeSize: %lld\n", imgHeader->u64CodeSize);
	printk("tBlock.u64Base: 0x%llX\n", imgHeader->tBlock.u64Base);
	printk("tBlock.u64Size: %lld\n", imgHeader->tBlock.u64Size);
	printk("tBlock.szPartID: %s\n", imgHeader->tBlock.szPartID);
	printk("nFlag: %d\n", imgHeader->nFlag);
	printk("nSelect: %d\n", imgHeader->nSelect);
}

int axcl_load_fwfile(struct axera_dev *ax_dev, const struct firmware *firmware)
{
	int size, sent = 0;
	int filecount = 0;
	int count = 0;
	int ret = 0;
	int imgsize = 0;
	void *imgaddr;
	unsigned long dest;
	PAC_HEAD_T *pacHeader;
	PAC_FILE_T *imgHeader;

	ret = send_buf_alloc(ax_dev);
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "send buf alloc failed,");
		return -1;
	}

	pacHeader = (PAC_HEAD_T *) firmware->data;
	if (!pacHeader) {
		axcl_trace(AXCL_ERR, "firmware data is NULL.");
		ret = -1;
		goto out;
	}

	printk("\n############### Dev %x Firmware loading ##############\n",
	       ax_dev->slot_index);
	dumppacheader(pacHeader);
	filecount = pacHeader->nFileCount;
	while (count < filecount) {
		if (sysdump_ctrl.debug) {
			if (count == 1) {
				ret = axcl_device_sysdump(ax_dev);
				if (ret > 0) {
					printk("[STATUS]: SUCCESS\n");
				}
			}
		}

		imgHeader =
		    (PAC_FILE_T *) ((firmware->data + pacHeader->nFileOffset) +
				    (sizeof(PAC_FILE_T) * count));

		dumpImgFileHeader(imgHeader);
		imgsize = imgHeader->u64CodeSize;
		dest = imgHeader->tBlock.u64Base;
		imgaddr = (void *)firmware->data + imgHeader->u64CodeOffset;
		sent = 0;

		while (imgsize) {
			size = min_t(uint, imgsize, MAX_TRANSFER_SIZE);
			memcpy(dma_virt_addr, imgaddr + sent, size);
			ret = axcl_boot_device(ax_dev, dest, size);
			if (ret < 0) {
				printk("[STATUS]: FAILED[%d]\n", ret);
				goto out;
			}

			dest += size;
			sent += size;
			imgsize -= size;
		}
		printk("[STATUS]: SUCCESS\n");
		count++;
	}

	if (count == filecount) {
		start_devices(ax_dev);
	}

out:
	free_buf_alloc(ax_dev);
	return ret;
}

int axcl_firmware_load(struct axera_dev *ax_dev)
{
	const struct firmware *firmware;
	char filename[AXCL_NAME_LEN];
	int ret;

	snprintf(filename, AXCL_NAME_LEN, AXCL_PATH_NAME);
	ret = request_firmware(&firmware, filename, &ax_dev->pdev->dev);
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "Patch file not found %s.", filename);
		return ret;
	}

	ret = axcl_load_fwfile(ax_dev, firmware);
	release_firmware(firmware);
	return ret;
}

int heartbeat_recv_thread(void *pArg)
{
	unsigned int target = *(unsigned int *)pArg;
	unsigned long long count = 0;
	unsigned int timeout = 50000;
	struct device_heart_packet *hbeat;
	struct axera_dev *axdev;
	int i;
	int ret;

	axcl_trace(AXCL_DEBUG, "target 0x%x thread running", target);

	axdev = g_pcie_opt->slot_to_axdev(target);
	if (!axdev) {
		axcl_trace(AXCL_ERR, "Get axdev is failed");
		return -1;
	}

	hbeat = (struct device_heart_packet *)axdev->shm_base_virt;

	do {
		ret =
		    axcl_heartbeat_recv_timeout(hbeat, target, count, timeout);
		if (ret <= 0) {
			axcl_trace(AXCL_ERR, "device %x: dead!", target);
			axcl_devices_heartbeat_status_set(target,
							  AXCL_HEARTBEAT_DEAD);
			for (i = 0; i < AXCL_PROCESS_MAX; i++) {
				if (g_pro_wait[i].pid != 0) {
					atomic_inc(&g_pro_wait[i].event);
					wake_up_interruptible(&g_pro_wait
							      [i].wait);
				}
			}
		}
		if ((0 < hbeat->interval)
		    && (hbeat->interval < AXCL_RECV_TIMEOUT)) {
			timeout = hbeat->interval;
			axcl_trace(AXCL_DEBUG, "timeout: %d", timeout);
		}
		count = ret;
		axcl_devices_heartbeat_status_set(target, AXCL_HEARTBEAT_ALIVE);
	} while (!g_heartbeat_stop);
	return 0;
}

int axcl_pcie_port_manage(struct axcl_device_info *devinfo)
{
	struct ax_transfer_handle *handle;
	struct axcl_device_info rdevinfo;
	unsigned int target = devinfo->device;
	unsigned int pid = devinfo->pid;
	unsigned int port = AXCL_NOTIFY_PORT;
	unsigned int pro;
	int count = 0;
	int ret;
	int i;

	axcl_trace(AXCL_DEBUG, "pid = %d, cpid = %d, ctgid = %d, cname = %s",
			pid, current->pid, current->tgid, current->comm);

	if (!pid) {
		axcl_trace(AXCL_ERR, "current process is NULL.");
		return -1;
	}

	for (i = 0; i < AXCL_PROCESS_MAX; i++) {
		if (g_pttr[i][target].pid == pid) {
			memcpy(devinfo, &g_pttr[i][target],
			       sizeof(struct axcl_device_info));
			return 0;
		}
	}

	for (i = 0; i < AXCL_PROCESS_MAX; i++) {
		if (g_pttr[i][target].pid == 0) {
			pro = i;
			break;
		}
	}

	if (i == AXCL_PROCESS_MAX) {
		axcl_trace(AXCL_ERR,
			   "The number of processes exceeds the limit: %d", i);
		return -1;
	}

	for (i = AXCL_BASE_PORT; i < MAX_MSG_PORTS; i++) {
		if (port_info[target][i] == i)
			continue;

		if (count == AXCL_MAX_PORT)
			break;
		axcl_trace(AXCL_DEBUG, "alloc port: 0x%x", i);
		port_info[target][i] = i;
		devinfo->ports[count] = i;
		g_pttr[pro][target].pid = pid;
		g_pttr[pro][target].ports[count] = i;
		count++;
	}

	devinfo->port_num = count;
	g_pttr[pro][target].port_num = count;
	g_pttr[pro][target].device = target;

	handle =
	    (struct ax_transfer_handle *)port_handle[target][port]->pci_handle;
	ret =
	    axcl_pcie_msg_send(handle, (void *)devinfo,
			       sizeof(struct axcl_device_info));
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "Send create port msg info failed: %d.",
			   ret);
		return -1;
	}

	ret =
	    axcl_pcie_recv_timeout(port_handle[target][port], (void *)&rdevinfo,
				   sizeof(struct axcl_device_info),
				   AXCL_RECV_TIMEOUT);
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "Recv port ack timeout.");
		return -1;
	}

	if (rdevinfo.cmd == AXCL_PORT_CREATE_FAIL) {
		axcl_trace(AXCL_ERR, "slave port create failed.");
		for (i = 0; i < devinfo->port_num; i++) {
			port_info[target][devinfo->ports[i]] = 0;
		}
		g_pttr[pro][target].pid = 0;
		ret = -1;
	}

	devinfo->cmd = rdevinfo.cmd;
	g_pttr[pro][target].cmd = rdevinfo.cmd;

	return ret;
}

ax_pcie_msg_handle_t *axcl_pcie_port_open(unsigned int target,
					  unsigned int port)
{
	ax_pcie_msg_handle_t *handle = NULL;
	unsigned long data;

	if (in_interrupt()) {
		handle = kmalloc(sizeof(ax_pcie_msg_handle_t), GFP_ATOMIC);
	} else {
		handle = kmalloc(sizeof(ax_pcie_msg_handle_t), GFP_KERNEL);
	}

	if (NULL == handle) {
		axcl_trace(AXCL_ERR, "Can not open target[0x%x:0x%x],"
			   " kmalloc for handler failed!", target, port);
		return NULL;
	}

	data = (unsigned long)ax_pcie_open(target, port);
	if (data) {
		handle->pci_handle = data;
		return handle;
	}

	kfree(handle);
	return NULL;
}

static void axcl_setopt_recv_pci(ax_pcie_msg_handle_t *handle)
{
	struct ax_transfer_handle *transfer_handle =
	    (struct ax_transfer_handle *)handle->pci_handle;
	transfer_handle->msg_notifier_recvfrom = &axcl_pcie_notifier_recv;
	transfer_handle->data = (unsigned long)handle;
}

int axcl_pcie_host_port_open(unsigned int target, unsigned int port)
{
	ax_pcie_msg_handle_t *msg_handle;

	msg_handle = axcl_pcie_port_open(target, port);
	if (msg_handle) {
		INIT_LIST_HEAD(&msg_handle->mem_list);
		axcl_setopt_recv_pci(msg_handle);
		port_handle[target][port] = msg_handle;
		axcl_trace(AXCL_DEBUG,
			   "open success target/port/handle[0x%x][0x%x][0x%lx]",
			   target, port, (unsigned long)msg_handle);
	} else {
		axcl_trace(AXCL_ERR, "open fail target/port[0x%x][0x%x]",
			   target, port);
		return -1;
	}
	return 0;
}

static void axcl_get_devices(struct device_list_t *devlist)
{
	int i = 0;

	devlist->type = 0;
	for (i = 0; i < g_pcie_opt->remote_device_number; i++) {
		devlist->devices[i] = g_axera_dev_map[i]->slot_index;
	}
	devlist->num = i;
}

static int axcl_pcie_open(struct inode *inode, struct file *file)
{
	int i;

	axcl_trace(AXCL_DEBUG, "axcl pcie open pid = %d.", current->pid);

	for (i = 0; i < AXCL_PROCESS_MAX; i++) {
		if (g_pro_wait[i].pid == 0) {
			g_pro_wait[i].pid = current->pid;
			break;
		}
	}
	if (i == AXCL_PROCESS_MAX) {
		axcl_trace(AXCL_ERR, "Not free process id.");
		return -1;
	}

	axcl_trace(AXCL_DEBUG, "axcl pcie open end: %d", i);

	file->private_data = &g_pro_wait[i];
	return 0;
}

static void axcl_clean_port_info(int pro, int dev, unsigned int pid)
{
	int i;

	for (i = 0; i < g_pttr[pro][dev].port_num; i++) {
		port_info[dev][g_pttr[pro][dev].ports[i]] = 0;
	}
	for (i = 0; i < AXCL_PROCESS_MAX; i++) {
		if (g_pro_wait[i].pid == pid) {
			g_pro_wait[i].pid = 0;
			break;
		}
	}
	memset(&g_pttr[pro][dev], 0, sizeof(struct axcl_device_info));
}

static int axcl_wake_up_poll(void)
{
	int i;

	for (i = 0; i < AXCL_PROCESS_MAX; i++) {
		if (g_pro_wait[i].pid == current->pid) {
			atomic_inc(&g_pro_wait[i].event);
			wake_up_interruptible(&g_pro_wait[i].wait);
			axcl_trace(AXCL_DEBUG, "close i = %d, pid = %d", i,
				   current->pid);
			break;
		}
	}

	if (i == AXCL_PROCESS_MAX) {
		axcl_trace(AXCL_ERR, "Not found current pid %d in array", current->pid);
		for (i = 0; i < AXCL_PROCESS_MAX; i++) {
			axcl_trace(AXCL_ERR, "g_pro_wait[%d].pid = %d", i, g_pro_wait[i].pid);
		}
		return -1;
	}
	return 0;
}

static int axcl_pcie_release(struct inode *inode, struct file *file)
{
	struct axcl_device_info devinfo;
	struct ax_transfer_handle *handle;
	unsigned int target;
	unsigned int pid = current->tgid;
	unsigned int port = AXCL_NOTIFY_PORT;
	int i, j;
	int ret;

	axcl_trace(AXCL_DEBUG, "current pid = %d, current tgid = %d, current name = %s",
				current->pid, current->tgid, current->comm);
	for (i = 0; i < AXCL_PROCESS_MAX; i++) {
		for (j = 0; j < AXERA_MAX_MAP_DEV; j++) {
			if (pid == g_pttr[i][j].pid) {
				target = g_pttr[i][j].device;
				axcl_trace(AXCL_DEBUG,
					   "axcl pcie release target: 0x%x",
					   target);
				memcpy(&devinfo, &g_pttr[i][j],
				       sizeof(struct axcl_device_info));
				devinfo.cmd = AXCL_PORT_DESTROY;
				handle = (struct ax_transfer_handle *)
				    port_handle[target][port]->pci_handle;
				ret =
				    axcl_pcie_msg_send(handle, (void *)&devinfo,
						       sizeof(struct
							      axcl_device_info));
				if (ret < 0) {
					axcl_clean_port_info(i, j, pid);
					axcl_trace(AXCL_ERR,
						   "Send create port msg info failed: %d",
						   ret);
					return -1;
				}
				axcl_clean_port_info(i, j, pid);
			}
		}
	}

	return 0;
}

static long axcl_pcie_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	int ret = 0;
	struct axcl_device_info devinfo;
	struct device_list_t devlist;

	if (down_interruptible(&ioctl_sem)) {
		axcl_trace(AXCL_ERR, "acquire handle sem failed!");
		return -1;
	}

	switch (cmd) {
	case IOC_AXCL_DEVICE_LIST:
		axcl_trace(AXCL_DEBUG, "IOC_AXCL_DEVICE_LIST.");
		axcl_get_devices(&devlist);
		if (copy_to_user
		    ((void *)arg, (void *)&devlist,
		     sizeof(struct device_list_t))) {
			printk("copy to usr space failed!\n");
			ret = -1;
			goto out;
		}
		break;
	case IOC_AXCL_PORT_MANAGE:
		axcl_trace(AXCL_DEBUG, "IOC_AXCL_PORT_MANAGE.");
		if (copy_from_user
		    ((void *)&devinfo, (void *)arg,
		     sizeof(struct axcl_device_info))) {
			printk("Get parameter from usr space failed!\n");
			ret = -1;
			goto out;
		}
		ret = axcl_pcie_port_manage(&devinfo);
		if (ret < 0) {
			axcl_trace(AXCL_ERR, "axcl pcie req port failed.");
			goto out;
		} else {
			if (copy_to_user
			    ((void *)arg, (void *)&devinfo,
			     sizeof(struct axcl_device_info))) {
				printk("copy to usr space failed!\n");
				ret = -1;
				goto out;
			}
		}
		break;
	case IOC_AXCL_CONN_STATUS:
		axcl_trace(AXCL_DEBUG, "IOC_AXCL_CONN_STATUS.");
		if (copy_to_user
		    ((void *)arg, (void *)&heartbeat_status,
		     sizeof(struct device_connect_status))) {
			printk("copy to usr space failed!\n");
			ret = -1;
			goto out;
		}
		break;
	case IOC_AXCL_WAKEUP_POLL:
		axcl_trace(AXCL_DEBUG, "IOC_AXCL_WAKEUP_STATUS.");
		ret = axcl_wake_up_poll();
		if (ret < 0) {
			axcl_trace(AXCL_ERR, "AXCL wake up poll failed.");
			goto out;
		}
		break;
	case IOC_AXCL_DEVICE_RESET:
		axcl_trace(AXCL_DEBUG, "IOC_AXCL_DEVICE_RESET.");
		if (copy_from_user
		    ((void *)&devinfo, (void *)arg,
		     sizeof(struct axcl_device_info))) {
			printk("Get parameter from usr space failed!\n");
			ret = -1;
			goto out;
		}
		host_reset_device(devinfo.device);
		break;
	case IOC_AXCL_DEVICE_BOOT:
		ret =
		    axcl_firmware_load(g_pcie_opt->
				       slot_to_axdev(devinfo.device));
		if (ret < 0) {
			axcl_trace(AXCL_ERR, "Device %x firmware load failed.",
				   devinfo.device);
			axcl_devices_heartbeat_status_set(devinfo.device,
							  AXCL_HEARTBEAT_DEAD);
		}
		break;
	default:
		printk("warning not defined cmd.\n");
		break;
	}

out:
	up(&ioctl_sem);
	return ret;
}

static ssize_t axcl_pcie_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *f_pos)
{
	return 0;
}

static ssize_t axcl_pcie_read(struct file *file, char __user *buf,
			      size_t count, loff_t *f_pos)
{
	return 0;
}

static unsigned int axcl_pcie_poll(struct file *file,
				   struct poll_table_struct *table)
{
	struct process_wait *process_wait =
	    (struct process_wait *)file->private_data;

	poll_wait(file, &process_wait->wait, table);

	if (atomic_read(&process_wait->event) > 0) {
		atomic_dec(&process_wait->event);
		return POLLIN | POLLRDNORM;
	}
	return 0;
}

static struct file_operations axcl_pcie_fops = {
	.owner = THIS_MODULE,
	.open = axcl_pcie_open,
	.release = axcl_pcie_release,
	.unlocked_ioctl = axcl_pcie_ioctl,
	.write = axcl_pcie_write,
	.read = axcl_pcie_read,
	.poll = axcl_pcie_poll,
};

static struct miscdevice axcl_usrdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &axcl_pcie_fops,
	.name = "axcl_host"
};

int axcl_timestamp_sync(struct axera_dev *ax_dev)
{
	struct timespec64 new_ts;
	struct timezone new_tz;

	/* gettimeofday */
	ktime_get_real_ts64(&new_ts);
	memcpy(&new_tz, &sys_tz, sizeof(sys_tz));
	memcpy(ax_dev->shm_base_virt + 0x10, &new_ts,
	       sizeof(struct timespec64));
	*(volatile unsigned int *)ax_dev->shm_base_virt = 0x22222222;

	return 0;
}

int axcl_common_prot_create(unsigned int target)
{
	int ret;

	/* create notify port */
	ret = axcl_pcie_host_port_open(target, AXCL_NOTIFY_PORT);
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "axcl pcie notify port open failed.");
		return -1;
	}
	/* create heartbeat port */
	ret = axcl_pcie_host_port_open(target, AXCL_HEARTBEAT_PORT);
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "axcl pcie heartbeat port open failed.");
		return -1;
	}
	/* create proc port */
	ret = axcl_pcie_host_port_open(target, AXCL_PROC_PORT);
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "axcl pcie notify port open failed.\n");
		return -1;
	}
	return 0;
}

static int __init axcl_pcie_host_init(void)
{
	int i, ret;
	unsigned int target;
	axcl_trace(AXCL_DEBUG, "axcl pcie host init.");

	sema_init(&ioctl_sem, 1);
	spin_lock_init(&g_axcl_lock);

	for (i = 0; i < AXERA_MAX_MAP_DEV; i++) {
		heartbeat_status.status[i] = -1;
	}

	/* 1. firmware loading */
	for (i = 0; i < g_pcie_opt->remote_device_number; i++) {
		target = g_axera_dev_map[i]->slot_index;
		ret = axcl_firmware_load(g_axera_dev_map[i]);
		if (ret < 0) {
			axcl_trace(AXCL_ERR, "Device %x firmware load failed.",
				   target);
			axcl_devices_heartbeat_status_set(target,
							  AXCL_HEARTBEAT_DEAD);
		}
	}

	/* 2. create port */
	for (i = 0; i < g_pcie_opt->remote_device_number; i++) {
		target = g_axera_dev_map[i]->slot_index;
		if (axcl_devices_heartbeat_status_get(target) ==
		    AXCL_HEARTBEAT_DEAD)
			continue;

		ret = axcl_common_prot_create(target);
		if (ret < 0) {
			axcl_devices_heartbeat_status_set(target,
							  AXCL_HEARTBEAT_DEAD);
		}
	}

	/* 3. rc and ep handshake */
	for (i = 0; i < g_pcie_opt->remote_device_number; i++) {
		target = g_axera_dev_map[i]->slot_index;
		if (axcl_devices_heartbeat_status_get(target) ==
		    AXCL_HEARTBEAT_DEAD)
			continue;
		axcl_trace(AXCL_ERR, "axcl wait dev %x handshake...", target);
		ret = ax_pcie_msg_check_remote(target);
		if (ret < 0) {
			axcl_trace(AXCL_ERR,
				   "axcl pcie check remote device %x failed",
				   target);
			axcl_devices_heartbeat_status_set(target,
							  AXCL_HEARTBEAT_DEAD);
		}
		axcl_devices_heartbeat_status_set(target, AXCL_HEARTBEAT_ALIVE);
	}

	/* 4. timestamp sync */
	for (i = 0; i < g_pcie_opt->remote_device_number; i++) {
		target = g_axera_dev_map[i]->slot_index;
		if (axcl_devices_heartbeat_status_get(target) ==
		    AXCL_HEARTBEAT_DEAD)
			continue;
		axcl_timestamp_sync(g_axera_dev_map[i]);
	}

	axcl_pcie_proc_create();

	/* 5. create heartbeat recv thread */
	for (i = 0; i < AXCL_PROCESS_MAX; i++) {
		g_pro_wait[i].pid = 0;
		atomic_set(&g_pro_wait[i].event, 0);
		init_waitqueue_head(&g_pro_wait[i].wait);
	}

	for (i = 0; i < g_pcie_opt->remote_device_number; i++) {
		target = g_axera_dev_map[i]->slot_index;
		if (axcl_devices_heartbeat_status_get(target) ==
		    AXCL_HEARTBEAT_DEAD)
			continue;

		heartbeat[i] =
		    kthread_create(heartbeat_recv_thread,
				   &g_axera_dev_map[i]->slot_index,
				   "heartbeat_kthd");
		if (IS_ERR(heartbeat[i]))
			return PTR_ERR(heartbeat[i]);
		wake_up_process(heartbeat[i]);
	}

	misc_register(&axcl_usrdev);

	return 0;
}

static void __exit axcl_pcie_host_exit(void)
{
	int i;
	unsigned int target;

	axcl_pcie_proc_remove();
	g_heartbeat_stop = 1;
	for (i = 0; i < g_pcie_opt->remote_device_number; i++) {
		target = g_axera_dev_map[i]->slot_index;
		kthread_stop(heartbeat[i]);
		host_reset_device(target);
	}

	misc_deregister(&axcl_usrdev);
}

module_init(axcl_pcie_host_init);
module_exit(axcl_pcie_host_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Axera");
