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
#include <linux/fs_struct.h>
#include <linux/file.h>
#include <linux/seq_file.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/version.h>
#include "axcl_pcie_host.h"

struct proc_dir_entry *axcl_proc;
struct proc_dir_entry *axcl_devs_proc;
struct proc_dir_entry **axcl_bus_proc;

#define AXCL_DEV_PROC_PATH  "/proc/ax_proc"
#define AXCL_PROC_NAME "ax_proc/axcl"
#define AXCL_DEVS_PROC_NAME "devices"
#define AXCL_DEV_PROC_SIZE	SZ_64K

static struct axcl_proc_file **proc_file;
static int filenum[MAX_DEV_NUMBER];

static int axcl_proc_info_write(unsigned int target, char *proc_name,
				char *buf, int count)
{
	int ret;
	char path_name[50];
	unsigned int port = AXCL_PROC_PORT;
	struct ax_transfer_handle *handle;
	struct axcl_proc_info proc_info;

	sprintf(path_name, "%s%s", AXCL_DEV_PROC_PATH, proc_name);

	proc_info.cmd = AXCL_WRITE_PROC;
	proc_info.size = count;
	memcpy(proc_info.data, buf, count);
	memcpy(proc_info.name, path_name, sizeof(path_name));

	handle =
	    (struct ax_transfer_handle *)port_handle[target][port]->pci_handle;
	ret = axcl_pcie_msg_send(handle, (void *)&proc_info, sizeof(proc_info));
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "Send create port msg info failed: %d",
			   ret);
		return -1;
	}

	memset(&proc_info, 0, sizeof(proc_info));
	ret =
	    axcl_pcie_recv_timeout(port_handle[target][port],
				   (void *)&proc_info, sizeof(proc_info),
				   AXCL_RECV_TIMEOUT);
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "Recv port ack timeout.");
		return -1;
	}

	if (proc_info.cmd == AXCL_WRITE_PROC_FAIL) {
		printk("axcl write dev %x proc failed\n", target);
		return -1;
	}
	return count;
}

static int axcl_proc_info_read(unsigned int target, char *proc_name,
			       char *buf, int count)
{
	int ret;
	char path_name[50];
	unsigned int port = AXCL_PROC_PORT;
	struct ax_transfer_handle *handle;
	struct axcl_proc_info proc_info;

	sprintf(path_name, "%s%s", AXCL_DEV_PROC_PATH, proc_name);

	proc_info.cmd = AXCL_READ_PROC;
	memcpy(proc_info.name, path_name, sizeof(path_name));

	handle =
	    (struct ax_transfer_handle *)port_handle[target][port]->pci_handle;
	ret = axcl_pcie_msg_send(handle, (void *)&proc_info, sizeof(proc_info));
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "Send create port msg info failed: %d",
			   ret);
		return -1;
	}

	ret =
	    axcl_pcie_recv_timeout(port_handle[target][port], (void *)buf,
				   count, AXCL_RECV_TIMEOUT);
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "Recv port ack timeout.");
		return -1;
	}

	if (((unsigned int *)buf)[0] == 0xffffffff) {
		return -1;
	}

	return 0;
}

static int axcl_proc_show(struct seq_file *m, void *v)
{
	int ret;
	char *buf;
	size_t size;
	int count = AXCL_DEV_PROC_SIZE;
	unsigned int target = (unsigned long)m->private;
	char *f_name;
	char *p_buf;
	const struct file *file = m->file;
	const struct path *path = &file->f_path;
	char delim[4];
	char *p;

	size = seq_get_buf(m, &p_buf);
	p = d_path(path, p_buf, size);

	snprintf(delim, sizeof(unsigned int), "%x", target);
	f_name = strstr(p, delim);
	f_name += strlen(delim);

	buf = vmalloc(count);
	if (!buf) {
		axcl_trace(AXCL_ERR, "vmalloc failed.");
		return -1;
	}

	memset(buf, 0, count);
	ret = axcl_proc_info_read(target, f_name, buf, count);
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "axcl get %s proc info failed", f_name);
		vfree(buf);
		return -1;
	}

	seq_printf(m, "%s\n", buf);
	vfree(buf);

	return 0;
}

static int axcl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, axcl_proc_show, ax_pde_data(inode));
}

static ssize_t axcl_proc_write(struct file *file, const char __user *buffer,
			       size_t count, loff_t *pos)
{
	char buf[20];
	unsigned int target = (unsigned long)ax_pde_data(file_inode(file));
	const struct path *path = &file->f_path;
	char delim[4];
	char *f_name;
	char p_buf[256];
	char *p;

	if (count > sizeof(buf)) {
		axcl_trace(AXCL_ERR, "axcl proc write data size too large\n");
		return -1;
	}

	if (copy_from_user(buf, buffer, count)) {
		axcl_trace(AXCL_ERR, "copy from user failed\n");
		return -1;
	}
	p = d_path(path, p_buf, sizeof(p_buf));

	snprintf(delim, sizeof(unsigned int), "%x", target);
	f_name = strstr(p, delim);
	f_name += strlen(delim);

	return axcl_proc_info_write(target, f_name, buf, count);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0))
static struct file_operations axcl_proc_fops = {
	.owner = THIS_MODULE,
	.open = axcl_proc_open,
	.release = single_release,
	.write = axcl_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
};
#else
static const struct proc_ops axcl_proc_fops = {
	.proc_open = axcl_proc_open,
	.proc_read = seq_read,
	.proc_write = axcl_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#endif

struct proc_dir_entry *find_parent_by_name(struct proc_dir_entry *bus_entry,
					   int num, char *name,
					   unsigned int target)
{
	int i;
	struct proc_dir_entry *entry;
	struct proc_dir_entry *pentry;
	char *parent_name;

	parent_name = strrchr(name, '/');
	parent_name += strlen("/");

	for (i = 0; i < filenum[target]; i++) {
		if (strcmp(proc_file[num][i].name, parent_name) == 0) {
			if (proc_file[num][i].entry != NULL) {
				return proc_file[num][i].entry;
			}
			if (strcmp(proc_file[num][i].parent, "") != 0) {
				pentry =
				    find_parent_by_name(bus_entry, num,
							proc_file[num][i].
							parent, target);
				entry =
				    proc_mkdir(proc_file[num][i].name, pentry);
				proc_file[num][i].entry = entry;
				break;
			} else {
				entry =
				    proc_mkdir(proc_file[num][i].name,
					       bus_entry);
				proc_file[num][i].entry = entry;
				break;
			}
		}
	}

	if (i == filenum[target]) {
		axcl_trace(AXCL_ERR, "No fond %s file in proc_file\n", parent_name);
		return NULL;
	}
	return entry;
}

void axcl_proc_file_create(struct proc_dir_entry *bus_entry, int num,
			   unsigned int target)
{
	int i;
	struct proc_dir_entry *entry;
	struct proc_dir_entry *pentry;

	for (i = 0; i < filenum[target]; i++) {
		if (strcmp(proc_file[num][i].parent, "") != 0
		    && proc_file[num][i].dir == DT_REG) {
			pentry =
			    find_parent_by_name(bus_entry, num,
						proc_file[num][i].parent,
						target);
			entry =
			    proc_create_data(proc_file[num][i].name, 0444,
					     pentry, &axcl_proc_fops,
					     (void *)(unsigned long)target);
		} else {
			if (proc_file[num][i].dir == DT_REG) {
				entry =
				    proc_create_data(proc_file[num][i].name,
						     0444, bus_entry,
						     &axcl_proc_fops,
						     (void *)(unsigned long)target);
			}
		}
	}
}

int axcl_get_device_proc_filenum(struct axcl_proc_info *proc_info,
				 unsigned int target)
{
	int ret;
	unsigned int port = AXCL_PROC_PORT;
	struct ax_transfer_handle *handle;

	proc_info->cmd = AXCL_GET_RPOC_FILE_NUM;

	handle =
	    (struct ax_transfer_handle *)port_handle[target][port]->pci_handle;
	ret =
	    axcl_pcie_msg_send(handle, (void *)proc_info,
			       sizeof(struct axcl_proc_info));
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "Send create port msg info failed: %d",
			   ret);
		return -1;
	}

	memset(proc_info, 0, sizeof(struct axcl_proc_info));
	ret =
	    axcl_pcie_recv_timeout(port_handle[target][port], (void *)proc_info,
				   sizeof(struct axcl_proc_info),
				   AXCL_RECV_TIMEOUT);
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "Recv port ack timeout.");
		return -1;
	}

	return 0;
}

int axcl_get_device_proc_files(struct axcl_proc_info *proc_info, int num,
			       unsigned int target)
{
	int ret;
	int size;
	unsigned int port = AXCL_PROC_PORT;
	struct ax_transfer_handle *handle;

	proc_info->cmd = AXCL_GET_PROC_FILE;

	handle =
	    (struct ax_transfer_handle *)port_handle[target][port]->pci_handle;
	ret =
	    axcl_pcie_msg_send(handle, (void *)proc_info,
			       sizeof(struct axcl_proc_info));
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "Send create port msg info failed: %d",
			   ret);
		return -1;
	}

	proc_file[num] =
	    (struct axcl_proc_file *)kmalloc(filenum[target] *
					     sizeof(struct axcl_proc_file),
					     GFP_ATOMIC);
	if (!proc_file[num]) {
		printk("kmalloc proc file failed\n");
		return -1;
	}

	size = filenum[target] * sizeof(struct axcl_proc_file);

	memset(proc_file[num], 0, size);
	ret = axcl_pcie_recv_timeout(port_handle[target][port], proc_file[num],
				     size, AXCL_RECV_TIMEOUT);
	if (ret < 0) {
		axcl_trace(AXCL_ERR, "Recv port ack timeout.");
		kfree(proc_file[num]);
		return -1;
	}
	return 0;
}

int axcl_pcie_proc_create(void)
{
	int i, ret;
	char busname[5];
	unsigned int target;
	struct axcl_proc_info proc_info;

	axcl_proc = proc_mkdir(AXCL_PROC_NAME, NULL);
	if (!axcl_proc) {
		axcl_trace(AXCL_ERR, "Create axcl proc node failed\n");
		return -1;
	}

	axcl_devs_proc = proc_mkdir(AXCL_DEVS_PROC_NAME, axcl_proc);
	if (!axcl_devs_proc) {
		axcl_trace(AXCL_ERR, "Create axcl devs proc node failed\n");
		return -1;
	}

	axcl_bus_proc =
	    kmalloc(g_pcie_opt->remote_device_number *
		    sizeof(struct proc_dir_entry *), GFP_ATOMIC);
	if (!axcl_bus_proc) {
		printk("kmalloc proc file failed\n");
		return -1;
	}

	proc_file =
	    kmalloc(g_pcie_opt->remote_device_number *
		    sizeof(struct axcl_proc_file *), GFP_ATOMIC);
	if (!proc_file) {
		printk("kmalloc proc file failed\n");
		return -1;
	}

	for (i = 0; i < g_pcie_opt->remote_device_number; i++) {
		target = g_axera_dev_map[i]->slot_index;
		if (axcl_devices_heartbeat_status_get(target) ==
		    AXCL_HEARTBEAT_DEAD)
			continue;

		sprintf(busname, "%x", target);
		axcl_bus_proc[i] = proc_mkdir(busname, axcl_devs_proc);
		if (!axcl_bus_proc[i]) {
			axcl_trace(AXCL_ERR,
				   "Create device %x proc node failed\n",
				   target);
			continue;
		}

		ret = axcl_get_device_proc_filenum(&proc_info, target);
		if (ret < 0) {
			printk("axcl get device proc file number failed\n");
			return -1;
		}

		filenum[target] = proc_info.filenum;

		memset(&proc_info, 0, sizeof(proc_info));
		ret = axcl_get_device_proc_files(&proc_info, i, target);
		if (ret < 0) {
			printk("axcl get device proc file number failed\n");
			return -1;
		}

		axcl_proc_file_create(axcl_bus_proc[i], i, target);
	}
	return 0;
}

void find_subfile_remove_by_name(struct proc_dir_entry *parent, char *name, int num, unsigned int target)
{
	int i;
	char *parent_name;

	for (i = 0; i < filenum[target]; i++) {
		if (strcmp(proc_file[num][i].parent, "") == 0)
			continue;
		parent_name = strrchr(proc_file[num][i].parent, '/');
		parent_name += strlen("/");
		if (strcmp(name, parent_name) == 0) {
			if (proc_file[num][i].dir == DT_DIR) {
				find_subfile_remove_by_name(proc_file[num][i].entry, proc_file[num][i].name, num, target);
				remove_proc_entry(proc_file[num][i].name, parent);
			} else {
				remove_proc_entry(proc_file[num][i].name, parent);
			}
		}
	}
}

void remove_axcl_proc(struct proc_dir_entry *parent, int num, unsigned int target)
{
	int i;

	for (i = 0; i < filenum[target]; i++) {
		if (strcmp(proc_file[num][i].parent, "") == 0 && proc_file[num][i].dir == DT_DIR) {
			find_subfile_remove_by_name(proc_file[num][i].entry, proc_file[num][i].name, num, target);
			remove_proc_entry(proc_file[num][i].name, parent);
		} else {
			if (strcmp(proc_file[num][i].parent, "") == 0) {
				remove_proc_entry(proc_file[num][i].name, parent);
			}
		}
	}
}

void axcl_pcie_proc_remove(void)
{
	int i;
	char busname[5];
	unsigned int target;

	for (i = 0; i < g_pcie_opt->remote_device_number; i++) {
		sprintf(busname, "%x", g_axera_dev_map[i]->slot_index);
		target = g_axera_dev_map[i]->slot_index;
		remove_axcl_proc(axcl_bus_proc[i], i, target);
		remove_proc_entry(busname, axcl_devs_proc);
		kfree(proc_file[i]);
	}
	remove_proc_entry(AXCL_DEVS_PROC_NAME, axcl_proc);
	remove_proc_entry(AXCL_PROC_NAME, NULL);
	kfree(proc_file);
	kfree(axcl_bus_proc);
}
