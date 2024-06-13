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
#include <linux/timer.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sizes.h>

#include "../include/ax_pcie_dev.h"
#include "../include/ax_pcie_msg_ursdev.h"
#include "../include/ax_pcie_proc.h"
#include "../include/ax_pcie_msg_transfer.h"

#define DRV_MODULE_NAME			"ax-pcie-dev-host"

struct axera_dev *g_axera_dev_map[MAX_PCIE_DEVICES] = { 0 };

EXPORT_SYMBOL(g_axera_dev_map);

struct ax_pcie_operation *g_pcie_opt;
EXPORT_SYMBOL(g_pcie_opt);

#ifdef SHMEM_FROM_MASTER
unsigned int shm_size;
module_param(shm_size, uint, S_IRUGO);

unsigned long int shm_phys_addr;
module_param(shm_phys_addr, ulong, S_IRUGO);
#endif

static struct pci_device_id ax_pcie_tbl[] = {
	{PCI_VENDOR_AX650, PCI_DEVICE_AX650,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0,}

};

MODULE_DEVICE_TABLE(pci, ax_pcie_tbl);

static int is_host(void)
{
	return 1;
}

static struct axera_dev *alloc_axeradev(void)
{
	struct axera_dev *dev;
	int size;

	size = sizeof(*dev);
	dev = kmalloc(size, GFP_KERNEL);
	if (!dev) {
		axera_trace(AXERA_ERR, "Alloc axera_dev failed!");
		return NULL;
	}

	memset(dev, 0, size);

	return dev;
}

static struct axera_dev *slot_to_axdev(unsigned int slot)
{
	int i;
	for (i = 0; i < g_pcie_opt->remote_device_number; i++) {
		if (slot == g_axera_dev_map[i]->slot_index) {
			return g_axera_dev_map[i];
		}
	}

	return NULL;
}

static void ax_pcie_config_read(struct axera_dev *ax_dev, int offset,
				u32 *addr)
{
	struct pci_dev *pdev;

	if (ax_dev && ax_dev->pdev) {
		pdev = ax_dev->pdev;
		pci_read_config_dword(pdev, offset, addr);
	}
}

static void ax_pcie_config_write(struct axera_dev *ax_dev, int offset,
				 u32 value)
{
	struct pci_dev *pdev;

	if (ax_dev && ax_dev->pdev) {
		pdev = ax_dev->pdev;
		pci_write_config_dword(pdev, offset, value);
	}
}

static void ax_pcie_config_store(struct axera_dev *ax_dev)
{
	unsigned int i;
	unsigned int offset;

	if (ax_dev && ax_dev->pdev) {
		for (i = 0; i < BRIDGE_PCIE_CFG_MAX_LEN; i++) {
			offset = (i << 2);
			pci_read_config_dword(ax_dev->pdev->bus->self, offset, &(ax_dev->bridge_cfg_len[i]));
		}

		for (i = 0; i < SAVE_PCIE_CFG_MAX_LEN; i++) {
			offset = (i << 2);
			g_pcie_opt->pci_config_read(ax_dev, offset, &(ax_dev->device_cfg_len[i]));
		}
	}
}

static void ax_pcie_config_restore(struct axera_dev *ax_dev)
{
	unsigned int i;
	unsigned int offset;

	if (ax_dev && ax_dev->pdev) {
		for (i = 0; i < BRIDGE_PCIE_CFG_MAX_LEN; i++) {
			offset = (i << 2);
			pci_write_config_dword(ax_dev->pdev->bus->self, offset, ax_dev->bridge_cfg_len[i]);
		}

		for (i = 0; i < SAVE_PCIE_CFG_MAX_LEN; i++) {
			offset = (i << 2);
			g_pcie_opt->pci_config_write(ax_dev, offset, ax_dev->device_cfg_len[i]);
		}
	}

}

static int ax_get_slot_index(struct pci_dev *pdev)
{
	int slot_index;
	slot_index = pdev->bus->number;
	axera_trace(AXERA_ERR, "busnr:%d,devfn:%d,slot:%d.",
		    pdev->bus->number, pdev->devfn, slot_index);

	return slot_index;
}

#ifdef SHMEM_FROM_MASTER
static int ax_pcie_alloc_shmem_space(struct axera_dev *ax_dev)
{
	void __iomem *space;
	size_t alignment = 0xffff;
	dma_addr_t org_phys_addr;
	dma_addr_t phys_addr;
	u64 offset;
	struct pci_dev *pdev = ax_dev->pdev;
	int size = SZ_8M;

	if (shm_size) {
		size = shm_size;
	}

	if (shm_phys_addr) {
		if (shm_phys_addr & 0xffff) {
			axera_trace(AXERA_ERR,
				    "Invalid address, address needs 64K alignment");
		} else {
			phys_addr = shm_phys_addr;
			space = devm_ioremap(&pdev->dev, shm_phys_addr, size);
			if (!space) {
				dev_err(&pdev->dev, "shm_phys_addr ioremap fail\n");
				return -1;
			}
		}
	}

	if (!shm_phys_addr) {
		space = dma_alloc_coherent(&pdev->dev, size + alignment, &org_phys_addr, GFP_KERNEL | __GFP_ZERO);
		if (!space) {
			dev_err(&pdev->dev, "failed to allocate mem space\n");
			return -2;
		}
		phys_addr = (org_phys_addr + alignment) & ~alignment;
		offset = phys_addr - org_phys_addr;
		space = space + offset;
	}

	ax_dev->ob_base = phys_addr;
	ax_dev->ob_size = size / 2;
	ax_dev->ob_base_virt = space;
	ax_dev->ob_end_virt = space + ax_dev->ob_size;
	axera_trace(AXERA_DEBUG, "phys_addr = %llx\n", ax_dev->ob_base);
	axera_trace(AXERA_DEBUG, "space = %p\n", space);
	axera_trace(AXERA_DEBUG, "ob_base_virt = %p\n", ax_dev->ob_base_virt);
	return 0;
}
#endif

static int ax_pcie_dev_probe(struct pci_dev *pdev,
			     const struct pci_device_id *pci_id)
{
	int ret = 0;
	int irq_num = -1;
	enum pci_barno bar;
	void __iomem *base;
#ifdef SHMEM_FROM_MASTER
	size_t alignment = 0xffff;
#endif
	struct axera_dev *ax_dev;
	irqreturn_t(*host_handler) (int irq, void *id);

	axera_trace(AXERA_INFO, "PCI device[%d] probed!",
		    g_pcie_opt->remote_device_number);

	dma_set_mask(&pdev->dev, DMA_BIT_MASK(64));
	dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64));

	ret = pci_enable_device(pdev);
	if (ret) {
		axera_trace(AXERA_ERR, "Enable pci device failed,errno=%d",
			    ret);
		return ret;
	}

	ret = pci_request_regions(pdev, DRV_MODULE_NAME);
	if (ret) {
		axera_trace(AXERA_ERR, "Request pci region failed,errno=%d",
			    ret);
		goto pci_request_regions_err;
	}

	pci_set_master(pdev);

#if SUPPORT_PCI_NET
	irq_num = pci_alloc_irq_vectors(pdev, 2, 32, PCI_IRQ_MSI);
#else
	irq_num = pci_alloc_irq_vectors(pdev, 1, 32, PCI_IRQ_MSI);
#endif
	if (irq_num < 0) {
		axera_trace(AXERA_ERR, "Failed to get MSI interrupts.");
		goto pci_alloc_irq_vectors_err;
	}

	ax_dev = alloc_axeradev();
	if (!ax_dev) {
		axera_trace(AXERA_ERR, "Alloc ax-device[the %dth one] failed!",
			    g_pcie_opt->remote_device_number);
		ret = -ENOMEM;
		goto alloc_axdev_err;
	}

	ax_dev->irq_num = irq_num;
	ax_dev->pdev = pdev;

	for (bar = BAR_0; bar <= BAR_5; bar++) {
		if (pci_resource_flags(pdev, bar) & IORESOURCE_MEM) {
			ax_dev->bar[bar] = pci_resource_start(pdev, bar);
			base = pci_ioremap_bar(pdev, bar);
			if (!base) {
				axera_trace(AXERA_ERR, "Failed to read BAR%d",
					    bar);
				goto bar_ioremap_err;
			}
			ax_dev->bar_info[bar].size = pci_resource_len(pdev, bar);
			ax_dev->bar_virt[bar] = base;
		}
	}

#ifdef SHMEM_FROM_MASTER
	ret = ax_pcie_alloc_shmem_space(ax_dev);
	if (ret < 0) {
		axera_trace(AXERA_ERR, "alloc shmem space fail in master");
		goto init_axdev_err;
	}
#endif

#if SUPPORT_PCI_NET
	ax_dev->shm_size = ax_dev->bar_info[BAR_0].size / 2;
#else
	ax_dev->shm_size = ax_dev->bar_info[BAR_0].size;
#endif
	ax_dev->slot_index = ax_get_slot_index(pdev);
	ax_dev->device_id = g_pcie_opt->pci_vendor_id(ax_dev);

	axera_trace(AXERA_INFO, "axdev# [slot:0x%x] [bar0:0x%llx] "
		    "[bar1:0x%llx] [bar4:0x%llx].", ax_dev->slot_index,
		    ax_dev->bar[0], ax_dev->bar[1], ax_dev->bar[4]);

	pci_set_drvdata(pdev, ax_dev);

	if (g_axera_dev_map[g_pcie_opt->remote_device_number]) {
		axera_trace(AXERA_ERR, "error: device[%d] already exists!",
			    g_pcie_opt->remote_device_number);
		ret = -1;
		goto alloc_shmem_err;
	}

	if (MAX_PCIE_DEVICES <= g_pcie_opt->remote_device_number) {
		axera_trace(AXERA_ERR, "error: too many devices!");
		ret = -1;
		goto alloc_shmem_err;
	}

	g_axera_dev_map[g_pcie_opt->remote_device_number] = ax_dev;
	g_pcie_opt->pci_config_store(ax_dev);
	g_pcie_opt->remote_device_number++;

	axera_trace(AXERA_INFO, "axera: host remote device number = %d",
		    g_pcie_opt->remote_device_number);

	host_handler = host_message_irq_handler;
	if (g_pcie_opt->host_request_msg_irq(host_handler, ax_dev)) {
			axera_trace(AXERA_ERR, "Host request msg irq failed!");
			ret = -1;
			goto request_msg_irq_err;
	}

	/* return success */
	return 0;

request_msg_irq_err:
alloc_shmem_err:
#ifdef SHMEM_FROM_MASTER
	if (shm_phys_addr) {
		devm_iounmap(&pdev->dev, ax_dev->ob_base_virt);
	} else {
		dma_free_coherent(&pdev->dev, ax_dev->ob_size * 2 + alignment,
				ax_dev->ob_base_virt, ax_dev->ob_base);
	}
init_axdev_err:
#endif
	for (bar = BAR_0; bar <= BAR_5; bar++) {
		if (ax_dev->bar_virt[bar])
			pci_iounmap(pdev, ax_dev->bar_virt[bar]);
	}
bar_ioremap_err:
	kfree(ax_dev);
alloc_axdev_err:
	pci_free_irq_vectors(pdev);
pci_alloc_irq_vectors_err:
	pci_release_regions(pdev);
pci_request_regions_err:
	pci_disable_device(pdev);

	return ret;
}

static void ax_pcie_dev_remove(struct pci_dev *pdev)
{
	enum pci_barno bar;
	int i;
	struct axera_dev *ax_dev = pci_get_drvdata(pdev);

	if (!ax_dev) {
		axera_trace(AXERA_ERR, "device is not non exist");
		return;
	}

	for (bar = BAR_0; bar <= BAR_5; bar++) {
		if (ax_dev->bar_virt[bar])
			pci_iounmap(pdev, ax_dev->bar_virt[bar]);
	}

	g_pcie_opt->release_msg_irq(ax_dev);
	pci_free_irq_vectors(pdev);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
	for (i = 0; i < g_pcie_opt->remote_device_number; i++) {
		if (ax_dev == g_axera_dev_map[i]) {
			g_axera_dev_map[i] = g_axera_dev_map[g_pcie_opt->remote_device_number - 1];
			g_axera_dev_map[g_pcie_opt->remote_device_number - 1] = NULL;
		}
	}

	kfree(ax_dev);
	g_pcie_opt->remote_device_number--;

	return;
}

static struct pci_driver ax_pcie_dev_driver = {
	.name = "ax_pcie_dev_host",
	.id_table = ax_pcie_tbl,
	.probe = ax_pcie_dev_probe,
	.remove = ax_pcie_dev_remove,
};

static int axdev_host_init(struct ax_pcie_operation *opt)
{
	struct ax_pcie_operation *pci_opt = opt;

	if (NULL == pci_opt) {
		axera_trace(AXERA_ERR,
			    "axdev host init failed, ax_pcie_operation is NULL!");
		return -1;
	}

	pci_opt->is_host = is_host;
	pci_opt->slot_to_axdev = slot_to_axdev;
	pci_opt->pci_config_read = ax_pcie_config_read;
	pci_opt->pci_config_write = ax_pcie_config_write;
	pci_opt->pci_config_store = ax_pcie_config_store;
	pci_opt->pci_config_restore = ax_pcie_config_restore;
	pci_opt->remote_device_number = 0;

	return 0;
}

static int __init ax_pcie_host_init(void)
{
	int ret = 0;

	g_pcie_opt = kmalloc(sizeof(struct ax_pcie_operation), GFP_KERNEL);
	if (!g_pcie_opt) {
		axera_trace(AXERA_ERR, "alloc ax_pcie_operation failed!");
		ret = -1;
	}

	ret = axdev_host_init(g_pcie_opt);
	if (ret) {
		axera_trace(AXERA_ERR, "axdev_host_init failed!");
		goto host_init_err;
	}

	ret = ax_pcie_arch_init(g_pcie_opt);
	if (ret) {
		axera_trace(AXERA_ERR, "pci arch init failed!");
		goto host_init_err;
	}

	ret = pci_register_driver(&ax_pcie_dev_driver);
	if (ret) {
		axera_trace(AXERA_ERR, "pci driver register failed!");
		goto pci_register_drv_err;
	}
#ifdef MSG_PROC_ENABLE
	ret = ax_pcie_proc_init("host_transfer_info");
	if (ret) {
		axera_trace(AXERA_ERR, "init proc failed!");
		goto pcie_proc_init_err;
	}
#endif
	return 0;

#ifdef MSG_PROC_ENABLE
pcie_proc_init_err:
	pci_unregister_driver(&ax_pcie_dev_driver);
#endif
pci_register_drv_err:
	ax_pcie_arch_exit();
host_init_err:
	if (g_pcie_opt)
		kfree(g_pcie_opt);

	return ret;
}

static void __exit ax_pcie_host_exit(void)
{
#ifdef MSG_PROC_ENABLE
	ax_pcie_proc_exit("host_transfer_info");
#endif

	pci_unregister_driver(&ax_pcie_dev_driver);
	ax_pcie_arch_exit();
	if (g_pcie_opt)
		kfree(g_pcie_opt);
}

module_init(ax_pcie_host_init);
module_exit(ax_pcie_host_exit);

MODULE_AUTHOR("Axera");
MODULE_DESCRIPTION("Axera host dev");
MODULE_LICENSE("GPL");
