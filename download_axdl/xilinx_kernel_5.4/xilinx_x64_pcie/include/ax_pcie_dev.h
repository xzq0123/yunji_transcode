/**************************************************************************************************
 *
 * Copyright (c) 2019-2023 Axera Semiconductor (Shanghai) Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor (Shanghai) Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor (Shanghai) Co., Ltd.
 *
 **************************************************************************************************/

#ifndef __AXERA_PCIE_DEV_HOST_HEADER__
#define __AXERA_PCIE_DEV_HOST_HEADER__

#include <linux/timer.h>
#include <linux/pci.h>
#include "ax_pcie_msg_transfer.h"


#define SUPPORT_PCI_NET 1

/* iATU register */
#define PCIE_ATU_UNR_REGION_CTRL1	0x00
#define PCIE_ATU_UNR_REGION_CTRL2	0x04
#define PCIE_ATU_UNR_LOWER_BASE		0x08
#define PCIE_ATU_UNR_UPPER_BASE		0x0C
#define PCIE_ATU_UNR_LOWER_LIMIT	0x10
#define PCIE_ATU_UNR_LOWER_TARGET	0x14
#define PCIE_ATU_UNR_UPPER_TARGET	0x18
#define PCIE_ATU_UNR_UPPER_LIMIT	0x20

#define PCIE_GET_ATU_INB_UNR_REG_OFFSET(region) \
		(0x3 << 20) | (((region) << 9) | BIT(8))
#define PCIE_GET_ATU_OUTB_UNR_REG_OFFSET(region) \
		(0x3 << 20) | ((region) << 9)

#define LINK_WAIT_MAX_IATU_RETRIES	5
#define LINK_WAIT_IATU			9

#define PCIE_ATU_ENABLE			BIT(31)
#define PCIE_ATU_BAR_MODE_ENABLE	BIT(30)
#define PCIE_ATU_FUNC_NUM_MATCH_EN      BIT(19)
#define PCIE_ATU_INCREASE_REGION_SIZE	BIT(13)
#define PCIE_DMA_BYPASS			(0x1 << 27)
#define PCIE_SIDEBAND_MISC_EN	(0x1 << 21)
#define PCIE0_SIDEBAND_AWMISC_OFFSET	0xC0
#define PCIE0_SIDEBAND_ARMISC_OFFSET	0xD0
#define PCIE1_SIDEBAND_AWMISC_OFFSET	0x148
#define PCIE1_SIDEBAND_ARMISC_OFFSET	0x158
#define PCIE_ATU_TYPE_MEM	0
#define	IRAM0_ADDRESS	0x04800000
#define PCIE_SPACE_SHMEM_BASE	0x51000000

/* Pipe msi register offset */
#define	PIPE_PCIE0_VEN_MSI_REG		0xD8
#define	PIPE_PCIE1_VEN_MSI_REG		0x160
#define	PIPE_PCIE0_REG_UPDATE		0xE0
#define	PIPE_PCIE1_REG_UPDATE		0x168

#define PCI_VENDOR_AX650 0x1f4b
#define PCI_DEVICE_AX650 0x0650

#define MAX_PCIE_DEVICES		0x100
#define BRIDGE_PCIE_CFG_MAX_LEN	(256/4)
#define SAVE_PCIE_CFG_MAX_LEN	(656/4)

#define	DEVICE_CHECKED_FLAG	0x1F4B
#define	DEVICE_HANDSHAKE_FLAG	0xA650
#define DEVICE_KFIFO_INIT	0xa5a5
#define PCIE_BUS_ERROR	(~0ULL)

#define MSG_PROC_ENABLE
#define SHMEM_FROM_MASTER
//#define IS_THIRD_PARTY_PLATFORM

/* slave mailbox */
#define PCIE_MASTERID	5
#define CPU0_MASTERID	0
#define PCIE_SLOT_REQ ((MAILBOX_REG_BASE + 0x308) | (PCIE_MASTERID<<4))
#define PCIE_SLOT_UNLOCK ((MAILBOX_REG_BASE + 0x30C) | (PCIE_MASTERID<<4))
#define PCIE_INT_STATS ((MAILBOX_REG_BASE + 0x300) | (PCIE_MASTERID<<4))
#define PCIE_INT_CLR ((MAILBOX_REG_BASE + 0x304) | (PCIE_MASTERID<<4))

#define PCIE_INFO 0x100
#define PCIE_DATA 0x0
#define	MAX_DATA_SIZE 32
#define REQUEST_TIMEOUT 60000	//60ms
#define MAILBOX_REQ_LIMIT_COUNT	8

#define REG32(x)	(*(volatile unsigned int *)(x))
#define MAILBOX_REG_MAP_ADDR 0x4520000
#define MAILBOX_REG_BASE (0xC000)

/* system reset register*/
#define SYSTEM_RESET_BASE_1M	0x4200000
#define SYSTEM_RESET_BASE_16M	0x4000000
#define SYSTEM_RESET_BASE_OFFSET	(0x200000)
#define CHIP_RST_SW_SET	0x50

#define PIPE_SYS_GLB_BASE	0x30000000
#define	PIPE_SYS_GLB_SIZE	0x1000

#define AXERA_DEBUG		4
#define AXERA_INFO		3
#define AXERA_ERR		2
#define AXERA_FATAL		1
#define AXERA_CURRENT_LEVEL	2
#define axera_trace(level, s, params...)	do { \
	if (level <= AXERA_CURRENT_LEVEL)	\
	printk("[%s, %d]: " s "\n", __func__, __LINE__, ##params);	\
} while (0)

enum pcie_config_reg_offset {
	CFG_VENDORID_REG = 0x0,
	CFG_COMMAND_REG = 0x04,
	CFG_CLASS_REG = 0x08,
	CFG_BAR0_REG = 0x10,
	CFG_BAR1_REG = 0x14,
	CFG_BAR2_REG = 0x18,
	CFG_BAR3_REG = 0x1c,
	CFG_BAR4_REG = 0x20,
};

enum pcie_inbound_windows {
	INBOUND_INDEX0 = 0,
	INBOUND_INDEX1,
	INBOUND_INDEX2,
	INBOUND_INDEX3,
	INBOUND_INDEX4,
	INBOUND_INDEX5,
	INBOUND_INDEX6,
	INBOUND_INDEX7,
};

enum pcie_outbound_windows {
	OUTBOUND_INDEX0 = 0,
	OUTBOUND_INDEX1,
	OUTBOUND_INDEX2,
	OUTBOUND_INDEX3,
	OUTBOUND_INDEX4,
	OUTBOUND_INDEX5,
	OUTBOUND_INDEX6,
	OUTBOUND_INDEX7,
};

struct ax_pcie_controller {

	unsigned id;

	unsigned int irq;
	/* PCI MSI interrupt vector */
	unsigned int msi_irq;

	struct list_head list;

	void __iomem *glb_base;

	unsigned long config_base;
	void __iomem *config_virt;
	unsigned long config_size;

	unsigned int win_base[6];
	unsigned int win_virt[6];
	unsigned int win_size[6];

};

enum pci_barno {
	BAR_0,
	BAR_1,
	BAR_2,
	BAR_3,
	BAR_4,
	BAR_5,
};

struct pci_bar_info {

	dma_addr_t	phys_addr;
	void		*addr;
	size_t		size;
};

struct axera_dev {
	unsigned int slot_index;
	unsigned int controller;

	struct ax_pcie_controller pcie_controller;

	struct pci_bar_info bar_info[6];
	struct pci_bar_info org_bar_info[6];
	unsigned int device_cfg_len[164];
	unsigned int bridge_cfg_len[164];

	struct ax_transfer_handle *ax_handle_table[MAX_MSG_PORTS];

	unsigned int irq;
	unsigned int irq_num;
	unsigned int device_id;
	void *bar_virt[6];
	u64 bar[6];
	u64 local_config_base_virt;

	u64 shm_base;
	u64 shm_size;
	u64 shm_end;
#ifdef SHMEM_FROM_MASTER
	u64 ob_base;
	u64 ob_size;
	void *ob_base_virt;
	void *ob_end_virt;
#endif
	void *shm_base_virt;
	void *shm_end_virt;

	unsigned int msg_checked_success;
	unsigned int kfifo_init_checked;

#ifdef IS_AX_SLAVE
	struct platform_device *pdev;
#else
	struct pci_dev *pdev;
#endif
};

struct ax_pcie_operation {
	int local_slotid;
	struct ax_pcie_controller *local_controller[2];
	unsigned int local_ctl_id;
	struct ax_pcie_controller *host_controller;

	unsigned int remote_device_number;

	void *private_data;

	unsigned int arch_type;

	int (*is_host) (void);
	unsigned int (*local_slot_number) (void);
	unsigned int (*pci_vendor_id) (struct axera_dev *axdev);

	struct axera_dev *(*slot_to_axdev) (unsigned int slot);
	int (*init_axdev) (struct axera_dev *axdev);

	unsigned int (*pcie_controller) (int bar0);

	void (*pci_config_read) (struct axera_dev *axdev, int offset,
				 u32 *val);
	void (*pci_config_write) (struct axera_dev *axdev, int offset,
				  u32 val);
	void (*pci_config_store) (struct axera_dev *axdev);
	void (*pci_config_restore) (struct axera_dev *axdev);

	int (*dma_local_irq_num) (unsigned int controller);
	int (*has_dma) (void);
	void (*start_dma_task) (void *new_task);
	void (*stop_dma_transfer) (unsigned int dma_channel);

	int (*clear_dma_read_local_irq) (unsigned int controller);
	int (*clear_dma_write_local_irq) (unsigned int controller);

	void (*enable_dma_local_irq) (unsigned int controller);
	void (*disable_dma_local_irq) (unsigned int controller);

	int (*dma_controller_init) (void);
	void (*dma_controller_exit) (void);
	int (*request_dma_resource) (irqreturn_t(*handler) (int irq, void *dev_id));
	void (*release_dma_resource) (void);

	int (*clear_msg_irq) (int fromid, int regno, int count);
	void (*start_ib_map) (u64 cpu_addr, int index, int bar);
	void (*start_ob_map) (u64 cpu_addr, u64 pci_addr, int index, u64 size);
	void (*slave_reset_map) (void);
	int (*trigger_msg_irq) (struct axera_dev *ax_dev, unsigned int irq_num);
	void (*reset_device) (struct axera_dev *ax_dev);

	int (*host_request_msg_irq) (irqreturn_t(*handler) (int irq, void *id), struct axera_dev *ax_dev);
	int (*slave_request_msg_irq) (void (*handler)
				       (int fromid, int regno, int count));
	void (*release_msg_irq) (struct axera_dev *ax_dev);
};

extern struct ax_pcie_operation *g_pcie_opt;
extern struct axera_dev *g_axera_dev_map[MAX_PCIE_DEVICES];

extern int ax_pcie_arch_init(struct ax_pcie_operation *handler);
extern unsigned int get_pcie_controller(int bar);
extern void ax_pcie_arch_exit(void);
extern void ax_pcie_dbi_write(u32 reg, u32 val);
extern void ax_pcie_dbi_read(u32 reg, u32 *val);
extern void ax_pcie_dbi_spin_lock(unsigned long *flags);
extern void ax_pcie_dbi_spin_unlock(unsigned long *flags);
extern void axera_pcie_awmisc_enable(int enable);
extern void axera_pcie_armisc_enable(int enable);
#ifndef IS_THIRD_PARTY_PLATFORM
extern void ax_pcie_spin_lock(unsigned long *flags);
extern void ax_pcie_spin_unlock(unsigned long *flags);
#endif
extern void ax_pcie_data_lock(unsigned long *flags);
extern void ax_pcie_data_unlock(unsigned long *flags);
#endif
