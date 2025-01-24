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
#include <linux/seq_file.h>
#include <linux/delay.h>
#include "axcl_pcie_host.h"

#define AXCL_DUMP_ADDR_SRC	(0x100000000)

static DEFINE_MUTEX(ioctl_mutex);
static struct task_struct *heartbeat[32];
ax_pcie_msg_handle_t *port_handle[AXERA_MAX_MAP_DEV][MAX_MSG_PORTS];

static dma_addr_t phys_addr;
static void *dma_virt_addr;

static spinlock_t g_axcl_lock;
struct device_connect_status heartbeat_status;
unsigned int port_info[AXERA_MAX_MAP_DEV][MAX_MSG_PORTS] = { 0 };

static struct axcl_handle_t *axcl_handle;

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

int wait_device_recv_completion(struct axera_dev *ax_dev, unsigned int flags,
				unsigned int timeout)
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

int axcl_dump_data(struct axera_dev *ax_dev, unsigned long src, int size)
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
	time64_t local_time;
	unsigned long minutes_west;
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

	printk("[INFO]: DEV %x EXCEPTION, DUMP...\n", ax_dev->slot_index);

	ktime_get_real_ts64(&ts);
	minutes_west = sys_tz.tz_minuteswest;
	local_time = ts.tv_sec + (minutes_west * 60 * -1);
	time64_to_tm(local_time, 0, &tm);
	snprintf(filename, AXCL_NAME_LEN, "%s/%s_%x.%04ld%02d%02d%02d%02d%02d",
		 sysdump_ctrl.path, AXCL_SYSDUMP_NAME, ax_dev->slot_index,
		 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
		 tm.tm_min, tm.tm_sec);

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
	struct list_head *entry, *tmp;
	struct list_head *deventry, *devtmp;
	struct device_handle_t *dev_handle = NULL;
	struct process_info_t *process_handle = NULL;
	int ret;

	axcl_trace(AXCL_DEBUG, "target 0x%x thread running", target);

	axdev = g_pcie_opt->slot_to_axdev(target);
	if (!axdev) {
		axcl_trace(AXCL_ERR, "Get axdev is failed");
		return -1;
	}

	hbeat = (struct device_heart_packet *)axdev->shm_base_virt;

	do {
		if (kthread_should_stop()) {
			break;
		}

		ret =
		    axcl_heartbeat_recv_timeout(hbeat, target, count, timeout);
		if (ret <= 0) {
			axcl_trace(AXCL_ERR, "device %x: dead!", target);
			axcl_devices_heartbeat_status_set(target,
							  AXCL_HEARTBEAT_DEAD);

			mutex_lock(&ioctl_mutex);
			if (axcl_handle) {
				if (!list_empty(&axcl_handle->process_list)) {
					list_for_each_safe(entry, tmp, &axcl_handle->process_list) {
						process_handle = list_entry(entry, struct process_info_t, head);
						if (process_handle == NULL)
							continue;
						if (!list_empty(&process_handle->dev_list)) {
							list_for_each_safe(deventry, devtmp, &process_handle->dev_list) {
								dev_handle = list_entry(deventry, struct device_handle_t, head);
								if (dev_handle == NULL)
									continue;
								if (dev_handle->device == target) {
									atomic_inc(&process_handle->event);
									wake_up_interruptible(&process_handle->wait);
								}
							}
						}
					}
				}
			}
			mutex_unlock(&ioctl_mutex);
		}
		if ((0 < hbeat->interval)
		    && (hbeat->interval < AXCL_RECV_TIMEOUT)) {
			timeout = hbeat->interval;
			axcl_trace(AXCL_DEBUG, "timeout: %d", timeout);
		}
		count = ret;
		axcl_devices_heartbeat_status_set(target, AXCL_HEARTBEAT_ALIVE);
	} while (1);

	return 0;
}

int axcl_pcie_port_manage(struct file *file, struct axcl_device_info *devinfo)
{
	struct ax_transfer_handle *handle;
	struct axcl_device_info rdevinfo;
	struct device_handle_t *dev_handle = NULL;
	struct process_info_t *process_handle = file->private_data;
	struct list_head *entry, *tmp;
	unsigned int target = devinfo->device;
	unsigned int pid = devinfo->pid;
	unsigned int port = AXCL_NOTIFY_PORT;
	int count = 0;
	int ret;
	int i;

	axcl_trace(AXCL_DEBUG, "pid = %d, cpid = %d, ctgid = %d, cname = %s",
		   pid, current->pid, current->tgid, current->comm);

	if (!pid) {
		axcl_trace(AXCL_ERR, "current process is NULL.");
		return -1;
	}

	if (!process_handle) {
		axcl_trace(AXCL_ERR, "process handler is NULL");
		return -1;
	}

	if (!list_empty(&process_handle->dev_list)) {
		list_for_each_safe(entry, tmp, &process_handle->dev_list) {
			dev_handle = list_entry(entry, struct device_handle_t, head);
			if (dev_handle->device == target) {
				devinfo->cmd = dev_handle->cmd;
				devinfo->port_num = dev_handle->port_num;
				memcpy(devinfo->ports, dev_handle->ports,
				       devinfo->port_num * sizeof(int));
				return 0;
			}
		}
	}

	dev_handle = kmalloc(sizeof(struct device_handle_t), GFP_KERNEL);
	if (NULL == dev_handle) {
		axcl_trace(AXCL_ERR,
			   "kmalloc for device handler failed, target = %x",
			   target);
		return -1;
	}
	INIT_LIST_HEAD(&dev_handle->head);

	for (i = AXCL_BASE_PORT; i < MAX_MSG_PORTS; i++) {
		if (port_info[target][i] == i)
			continue;

		if (count == AXCL_MAX_PORT)
			break;

		port_info[target][i] = i;
		devinfo->ports[count] = i;
		dev_handle->ports[count] = i;

		axcl_trace(AXCL_INFO,
			   "alloc port:  i: 0x%x, target: %d, pid: %d", i,
			   target, pid);
		count++;
	}

	devinfo->port_num = count;
	dev_handle->device = target;
	dev_handle->port_num = count;

	handle =
	    (struct ax_transfer_handle *)port_handle[target][port]->pci_handle;
	ret =
	    axcl_pcie_msg_send(handle, (void *)devinfo,
			       sizeof(struct axcl_device_info));
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "Send create port msg info failed: %d.",
			   ret);
		kfree(dev_handle);
		return -1;
	}

	ret =
	    axcl_pcie_recv_timeout(port_handle[target][port], (void *)&rdevinfo,
				   sizeof(struct axcl_device_info),
				   AXCL_RECV_TIMEOUT);
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "Recv port ack timeout.");
		kfree(dev_handle);
		return -1;
	}

	if (rdevinfo.cmd == AXCL_PORT_CREATE_FAIL) {
		axcl_trace(AXCL_ERR, "slave port create failed.");
		for (i = 0; i < devinfo->port_num; i++) {
			port_info[target][devinfo->ports[i]] = 0;
		}
		devinfo->cmd = rdevinfo.cmd;
		kfree(dev_handle);
		return -1;
	}

	devinfo->cmd = rdevinfo.cmd;
	dev_handle->cmd = rdevinfo.cmd;

	list_add_tail(&dev_handle->head, &process_handle->dev_list);

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

static void axcl_get_bus_info(struct axcl_bus_info_t *businfo)
{
	unsigned int i;
	for (i = 0; i < g_pcie_opt->remote_device_number; ++i) {
		if (g_axera_dev_map[i]->slot_index == businfo->device) {
			businfo->domain = g_axera_dev_map[i]->domain;
			businfo->slot = g_axera_dev_map[i]->dev_slot;
			businfo->func = g_axera_dev_map[i]->dev_func;
			break;
		}
	}
}

static int axcl_get_pid_info(struct axcl_pid_info_t *pidinfo)
{
	struct process_info_t *process_handle = NULL;
	struct device_handle_t *dev_handle = NULL;
	struct list_head *entry, *tmp;
	struct list_head *deventry, *devtmp;
	unsigned int target = pidinfo->device;
	unsigned int count = 0;
	unsigned long pidaddr = pidinfo->pid;
	unsigned int pidnum = pidinfo->num;
	int *pidbuf;

	if (!pidnum) {
		axcl_trace(AXCL_ERR, "No device pid");
		return 0;
	}

	if (!pidinfo->pid) {
		axcl_trace(AXCL_ERR, "User pid addr is NULL");
		return -1;
	}

	pidbuf = vmalloc(sizeof(int) * pidnum);
	if (!pidbuf) {
		axcl_trace(AXCL_ERR, "Pid buf malloc failed");
		return -1;
	}
	memset(pidbuf, 0, sizeof(int) * pidnum);

	if (axcl_handle) {
		if (!list_empty(&axcl_handle->process_list)) {
			list_for_each_safe(entry, tmp, &axcl_handle->process_list) {
				process_handle = list_entry(entry, struct process_info_t, head);
				if (process_handle == NULL)
					continue;
				if (!list_empty(&process_handle->dev_list)) {
					list_for_each_safe(deventry, devtmp, &process_handle->dev_list) {
						dev_handle = list_entry(deventry, struct device_handle_t, head);
						if (dev_handle == NULL)
							continue;
						if (dev_handle->device == target) {
							if (count >= pidnum)
								break;
							pidbuf[count] = process_handle->pid;
							count++;
							break;
						}
					}
				}
			}
		}
	}

	if (copy_to_user((void *)pidaddr, pidbuf, sizeof(int) * pidnum)) {
		axcl_trace(AXCL_ERR, "IOC_AXCL_PID_INFO copy to usr space failed!");
		vfree(pidbuf);
		return -1;
	}
	vfree(pidbuf);
	return 0;
}

static void axcl_get_pid_num(struct axcl_pid_num_t *pidnum)
{
	struct process_info_t *process_handle = NULL;
	struct device_handle_t *dev_handle = NULL;
	struct list_head *entry, *tmp;
	struct list_head *deventry, *devtmp;
	unsigned int target = pidnum->device;
	unsigned int count = 0;

	if (axcl_handle) {
		if (!list_empty(&axcl_handle->process_list)) {
			list_for_each_safe(entry, tmp, &axcl_handle->process_list) {
				process_handle = list_entry(entry, struct process_info_t, head);
				if (process_handle == NULL)
					continue;
				if (!list_empty(&process_handle->dev_list)) {
					list_for_each_safe(deventry, devtmp, &process_handle->dev_list) {
						dev_handle = list_entry(deventry, struct device_handle_t, head);
						if (dev_handle == NULL)
							continue;
						if (dev_handle->device == target) {
							count++;
							break;
						}
					}
				}
			}
		}
	}
	pidnum->num = count;
}

static int axcl_pcie_open(struct inode *inode, struct file *file)
{
	struct process_info_t *process_handle = NULL;
	struct list_head *entry, *tmp;

	axcl_trace(AXCL_DEBUG, "axcl pcie open pid = %d.", current->pid);

	if (!axcl_handle) {
		axcl_trace(AXCL_ERR, "axcl handler is NULL");
		return -1;
	}

	mutex_lock(&ioctl_mutex);
	if (!list_empty(&axcl_handle->process_list)) {
		list_for_each_safe(entry, tmp, &axcl_handle->process_list) {
			process_handle = list_entry(entry, struct process_info_t, head);
			if (process_handle) {
				if (process_handle->pid == current->pid) {
					file->private_data = process_handle;
					mutex_unlock(&ioctl_mutex);
					return 0;
				}
			}
		}
	}

	process_handle = kmalloc(sizeof(struct process_info_t), GFP_KERNEL);
	if (NULL == process_handle) {
		axcl_trace(AXCL_ERR,
			   "kmalloc for process handler failed, pid = %d",
			   current->pid);
		mutex_unlock(&ioctl_mutex);
		return -1;
	}

	INIT_LIST_HEAD(&process_handle->head);
	INIT_LIST_HEAD(&process_handle->dev_list);
	atomic_set(&process_handle->event, 0);
	init_waitqueue_head(&process_handle->wait);
	process_handle->pid = current->pid;

	list_add_tail(&process_handle->head, &axcl_handle->process_list);
	file->private_data = process_handle;

	mutex_unlock(&ioctl_mutex);
	return 0;
}

static void axcl_clean_port_info(int dev, struct device_handle_t *dev_handle)
{
	int i;

	for (i = 0; i < dev_handle->port_num; i++) {
		port_info[dev][dev_handle->ports[i]] = 0;
	}
}

static int axcl_wake_up_poll(struct file *file)
{
	struct process_info_t *process_handle = file->private_data;

	if (!process_handle) {
		axcl_trace(AXCL_ERR, "wake up process handler is NULL");
		return -1;
	}

	atomic_inc(&process_handle->event);
	wake_up_interruptible(&process_handle->wait);

	return 0;
}

static int axcl_pcie_release(struct inode *inode, struct file *file)
{
	struct list_head *entry, *tmp;
	struct axcl_device_info devinfo;
	struct ax_transfer_handle *handle;
	struct process_info_t *process_handle = file->private_data;
	struct device_handle_t *dev_handle = NULL;
	unsigned int target;
	unsigned int port = AXCL_NOTIFY_PORT;
	int ret;

	axcl_trace(AXCL_DEBUG,
		   "current pid = %d, current tgid = %d, current name = %s",
		   current->pid, current->tgid, current->comm);

	if (!process_handle) {
		axcl_trace(AXCL_ERR, "release: process handle is NULL");
		return -1;
	}

	mutex_lock(&ioctl_mutex);
	if (!list_empty(&process_handle->dev_list)) {
		list_for_each_safe(entry, tmp, &process_handle->dev_list) {
			dev_handle = list_entry(entry, struct device_handle_t, head);
			if (dev_handle == NULL)
				continue;
			target = dev_handle->device;
			axcl_trace(AXCL_DEBUG, "axcl pcie release target: 0x%x",
				   target);
			devinfo.device = dev_handle->device;
			devinfo.cmd = AXCL_PORT_DESTROY;
			devinfo.port_num = dev_handle->port_num;
			memcpy(devinfo.ports, dev_handle->ports,
			       dev_handle->port_num * sizeof(int));
			handle =
			    (struct ax_transfer_handle *)
			    port_handle[target][port]->pci_handle;
			ret =
			    axcl_pcie_msg_send(handle, (void *)&devinfo,
					       sizeof(struct axcl_device_info));
			if (ret < 0) {
				axcl_trace(AXCL_ERR,
					   "Send dev %x destroy port msg info failed: %d",
					   target, ret);
			}
			axcl_clean_port_info(target, dev_handle);
			list_del(&dev_handle->head);
			kfree(dev_handle);
		}
	}

	list_del(&process_handle->head);
	kfree(process_handle);

	mutex_unlock(&ioctl_mutex);
	return 0;
}

static long axcl_pcie_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	int ret = 0;
	struct axcl_device_info devinfo;
	struct device_list_t devlist;
	struct axcl_bus_info_t businfo;
	struct axcl_pid_info_t pidinfo;
	struct axcl_pid_num_t pidnum;

	mutex_lock(&ioctl_mutex);
	switch (cmd) {
	case IOC_AXCL_DEVICE_LIST:
		axcl_trace(AXCL_DEBUG, "IOC_AXCL_DEVICE_LIST.");
		axcl_get_devices(&devlist);
		if (copy_to_user
		    ((void *)arg, (void *)&devlist,
		     sizeof(struct device_list_t))) {
			axcl_trace(AXCL_ERR,
				   "IOC_AXCL_DEVICE_LIST copy to usr space failed!");
			ret = -1;
			goto out;
		}
		break;
	case IOC_AXCL_PORT_MANAGE:
		axcl_trace(AXCL_DEBUG, "IOC_AXCL_PORT_MANAGE.");
		if (copy_from_user
		    ((void *)&devinfo, (void *)arg,
		     sizeof(struct axcl_device_info))) {
			axcl_trace(AXCL_ERR,
				   "IOC_AXCL_PORT_MANAGE Get parameter from usr space failed!");
			ret = -1;
			goto out;
		}
		ret = axcl_pcie_port_manage(file, &devinfo);
		if (ret < 0) {
			axcl_trace(AXCL_ERR, "axcl pcie req port failed.");
			goto out;
		} else {
			if (copy_to_user
			    ((void *)arg, (void *)&devinfo,
			     sizeof(struct axcl_device_info))) {
				axcl_trace(AXCL_ERR,
					   "IOC_AXCL_PORT_MANAGE copy to usr space failed!");
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
			axcl_trace(AXCL_ERR,
				   "IOC_AXCL_CONN_STATUS copy to usr space failed!");
			ret = -1;
			goto out;
		}
		break;
	case IOC_AXCL_WAKEUP_POLL:
		axcl_trace(AXCL_DEBUG, "IOC_AXCL_WAKEUP_STATUS.");
		ret = axcl_wake_up_poll(file);
		if (ret < 0) {
			axcl_trace(AXCL_ERR,
				   "IOC_AXCL_WAKEUP_STATUS AXCL wake up poll failed.");
			goto out;
		}
		break;
	case IOC_AXCL_DEVICE_RESET:
		axcl_trace(AXCL_DEBUG, "IOC_AXCL_DEVICE_RESET.");
		if (copy_from_user
		    ((void *)&devinfo, (void *)arg,
		     sizeof(struct axcl_device_info))) {
			axcl_trace(AXCL_ERR,
				   "IOC_AXCL_DEVICE_RESET Get parameter from usr space failed!");
			ret = -1;
			goto out;
		}
		host_reset_device(devinfo.device);
		break;
	case IOC_AXCL_DEVICE_BOOT:
		ret =
		    axcl_firmware_load(g_pcie_opt->slot_to_axdev
				       (devinfo.device));
		if (ret < 0) {
			axcl_trace(AXCL_ERR,
				   "IOC_AXCL_DEVICE_BOOT Device %x firmware load failed.",
				   devinfo.device);
			axcl_devices_heartbeat_status_set(devinfo.device,
							  AXCL_HEARTBEAT_DEAD);
		}
		break;
	case IOC_AXCL_BUS_INFO:
		axcl_trace(AXCL_DEBUG, "IOC_AXCL_BUS_INFO.");
		if (copy_from_user
		    ((void *)&businfo, (void *)arg,
		     sizeof(struct axcl_bus_info_t))) {
			axcl_trace(AXCL_ERR,
				   "IOC_AXCL_BUS_INFO Get parameter from usr space failed!");
			ret = -1;
			goto out;
		}

		axcl_get_bus_info(&businfo);

		if (copy_to_user
		    ((void *)arg, (void *)&businfo,
		     sizeof(struct axcl_bus_info_t))) {
			axcl_trace(AXCL_ERR,
				   "IOC_AXCL_BUS_INFO copy to usr space failed!");
			ret = -1;
			goto out;
		}
		break;
	case IOC_AXCL_PID_NUM:
		if (copy_from_user
		    ((void *)&pidnum, (void *)arg,
		     sizeof(struct axcl_pid_num_t))) {
			axcl_trace(AXCL_ERR,
				   "IOC_AXCL_PID_NUM Get parameter from usr space failed!");
			ret = -1;
			goto out;
		}
		axcl_get_pid_num(&pidnum);
		if (copy_to_user
		    ((void *)arg, (void *)&pidnum,
		     sizeof(struct axcl_pid_num_t))) {
			axcl_trace(AXCL_ERR,
				   "IOC_AXCL_PID_NUM copy to usr space failed!");
			ret = -1;
			goto out;
		}
		break;
	case IOC_AXCL_PID_INFO:
		if (copy_from_user
		    ((void *)&pidinfo, (void *)arg,
		     sizeof(struct axcl_pid_info_t))) {
			axcl_trace(AXCL_ERR,
				   "IOC_AXCL_PID_INFO Get parameter from usr space failed!");
			ret = -1;
			goto out;
		}

		axcl_trace(AXCL_INFO, "IOC_AXCL_PID_INFO target = 0x%x",
			   pidinfo.device);

		ret = axcl_get_pid_info(&pidinfo);
		if (ret < 0) {
			axcl_trace(AXCL_ERR, "axcl get pid info failed");
		}
		break;
	default:
		axcl_trace(AXCL_ERR, "warning not defined cmd.");
		break;
	}

out:
	mutex_unlock(&ioctl_mutex);
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
	struct process_info_t *process_handle = file->private_data;

	if (!process_handle) {
		axcl_trace(AXCL_ERR, "process handle is NULL");
		return -1;
	}

	mutex_lock(&ioctl_mutex);
	poll_wait(file, &process_handle->wait, table);

	if (atomic_read(&process_handle->event) > 0) {
		atomic_dec(&process_handle->event);
		mutex_unlock(&ioctl_mutex);
		return POLLIN | POLLRDNORM;
	}
	mutex_unlock(&ioctl_mutex);
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
	return 0;
}

static int __init axcl_pcie_host_init(void)
{
	int i, ret;
	unsigned int target;
	axcl_trace(AXCL_DEBUG, "axcl pcie host init.");

	mutex_init(&ioctl_mutex);
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

	/* 5. create heartbeat recv thread */
	axcl_handle = kmalloc(sizeof(struct axcl_handle_t), GFP_KERNEL);
	if (NULL == axcl_handle) {
		axcl_trace(AXCL_ERR, "kmalloc for axcl handler failed");
		return -1;
	}
	INIT_LIST_HEAD(&axcl_handle->process_list);

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

static void axcl_del_mem_list(void)
{
	struct list_head *entry, *tmp;
	struct list_head *deventry, *devtmp;
	struct process_info_t *process_handle = NULL;
	struct device_handle_t *dev_handle = NULL;

	mutex_lock(&ioctl_mutex);
	if (!axcl_handle) {
		axcl_trace(AXCL_ERR, "axcl handle is NULL");
		mutex_unlock(&ioctl_mutex);
		return;
	}

	/* mem list empty means no data is comming */
	if (!list_empty(&axcl_handle->process_list)) {
		list_for_each_safe(entry, tmp, &axcl_handle->process_list) {
			process_handle = list_entry(entry, struct process_info_t, head);
			if (process_handle == NULL)
				continue;
			if (!list_empty(&process_handle->dev_list)) {
				list_for_each_safe(deventry, devtmp, &process_handle->dev_list) {
					dev_handle = list_entry(deventry, struct device_handle_t, head);
					if (dev_handle == NULL)
						continue;
					list_del(&dev_handle->head);
					kfree(dev_handle);
				}
			}
			list_del(&process_handle->head);
			kfree(process_handle);
		}
	}
	kfree(axcl_handle);
	mutex_unlock(&ioctl_mutex);
}

static void __exit axcl_pcie_host_exit(void)
{
	int i;
	unsigned int target;

	for (i = 0; i < g_pcie_opt->remote_device_number; i++) {
		target = g_axera_dev_map[i]->slot_index;
		if (heartbeat[i]) {
			kthread_stop(heartbeat[i]);
		}
		host_reset_device(target);
	}

	axcl_del_mem_list();

	misc_deregister(&axcl_usrdev);
}

module_init(axcl_pcie_host_init);
module_exit(axcl_pcie_host_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Axera");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
