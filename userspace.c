#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/proc_fs.h>

#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <net/sock.h>

#include "chip.h"
#include "platform.h"

#define NETLINK_USER	31
struct socket *nl_sk;

#define ILITEK_IOCTL_MAGIC	100 
#define ILITEK_IOCTL_MAXNR	4

#define ILITEK_IOCTL_I2C_WRITE_DATA			_IOWR(ILITEK_IOCTL_MAGIC, 0, uint8_t*)
#define ILITEK_IOCTL_I2C_SET_WRITE_LENGTH	_IOWR(ILITEK_IOCTL_MAGIC, 1, int)
#define ILITEK_IOCTL_I2C_READ_DATA			_IOWR(ILITEK_IOCTL_MAGIC, 2, uint8_t*)
#define ILITEK_IOCTL_I2C_SET_READ_LENGTH	_IOWR(ILITEK_IOCTL_MAGIC, 3, int)

#define UPGRADE_BY_IRAM 

extern CORE_CONFIG *core_config;
uint16_t length = 0;

static ssize_t ilitek_proc_glove_read(struct file *filp, char __user *buff, size_t size, loff_t *pPos)
{
	DBG_INFO();
	return size;
}

static ssize_t ilitek_proc_glove_write(struct file *filp, const char __user *buff, size_t size, loff_t *pPos)
{
	DBG_INFO();
	return size;
}

static ssize_t ilitek_proc_gesture_read(struct file *filp, char __user *buff, size_t size, loff_t *pPos)
{
	DBG_INFO();
	return size;
}

static ssize_t ilitek_proc_gesture_write(struct file *filp, const char __user *buff, size_t size, loff_t *pPos)
{
	DBG_INFO();
	return size;
}

static ssize_t ilitek_proc_mp_test_read(struct file *filp, char __user *buff, size_t size, loff_t *pPos)
{
	DBG_INFO();
	return size;
}

static ssize_t ilitek_proc_mp_test_write(struct file *filp, const char __user *buff, size_t size, loff_t *pPos)
{
	DBG_INFO();
	return size;
}

static ssize_t ilitek_proc_firmware_read(struct file *filp, char __user *buff, size_t size, loff_t *pPos)
{
	DBG_INFO();
	return size;
}

static ssize_t ilitek_proc_firmware_write(struct file *filp, const char __user *buff, size_t size, loff_t *pPos)
{
    char *pValid = NULL;
    char *pTmpFilePath = NULL;
    char szFilePath[100] = {0};
	ssize_t res = -EINVAL;

    if (buff != NULL)
    {
        pValid = strstr(buff, ".hex");

        if (pValid)
        {
            pTmpFilePath = strsep((char **)&buff, ".");

            strcat(szFilePath, pTmpFilePath);
            strcat(szFilePath, ".hex");

            DBG_INFO("File path: %s", szFilePath);

			ilitek_platform_disable_irq();

#ifndef UPGRADE_BY_IRAM
			res = core_firmware_upgrade(szFilePath);
#else
			res = core_firmware_iram_upgrade(szFilePath);
#endif

			ilitek_platform_enable_irq();

			if(res < 0)
			{
                DBG_ERR("Failed to upgrade firwmare, res = %d", res);
			}
            else
            {
                DBG_INFO("Succeed to upgrade firmware");
				return size;
            }
        }
        else
        {
            DBG_ERR("The file format is invalid");
        }
    }
    else
    {
        DBG_ERR("The file path is invalid");
    }

	return res;
}

static long ilitek_proc_i2c_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int res = 0;
	uint8_t	 szBuf[512] = {0};

	if(_IOC_TYPE(cmd) != ILITEK_IOCTL_MAGIC)
	{
		DBG_ERR("The Magic number doesn't match");
		return -ENOTTY;
	}

	if(_IOC_NR(cmd) > ILITEK_IOCTL_MAXNR)
	{
		DBG_ERR("The number of ioctl doesn't match");
		return -ENOTTY;
	}

	DBG_INFO("cmd = %d", _IOC_NR(cmd));

	switch(cmd)
	{
		case ILITEK_IOCTL_I2C_WRITE_DATA:
			res = copy_from_user(szBuf, (unsigned char*)arg, length);
			if(res < 0)
			{
				DBG_ERR("Failed to copy data from userspace");
				break;
			}
			//DBG_INFO("slave: %x , len : %d", core_config->slave_i2c_addr, length);
			res = core_i2c_write(core_config->slave_i2c_addr, &szBuf[0], length);
			if(res < 0)
			{
				DBG_ERR("Failed to write data via i2c");
				break;
			}
			break;

		case ILITEK_IOCTL_I2C_READ_DATA:
			res = core_i2c_read(core_config->slave_i2c_addr, szBuf, length);
			if(res < 0)
			{
				DBG_INFO("Failed to read data via i2c");
				break;
			}

			res = copy_to_user((unsigned char*)arg, szBuf, length);
			if(res < 0)
			{
				DBG_INFO("Failed to copy data to userspace");
				break;
			}
			break;

		case ILITEK_IOCTL_I2C_SET_WRITE_LENGTH:
		case ILITEK_IOCTL_I2C_SET_READ_LENGTH:
			length = arg;
			break;

		default:
			res = -ENOTTY;
			break;
	}

	return res;
}

struct proc_dir_entry *proc_dir_ilitek;
struct proc_dir_entry *proc_i2c;
struct proc_dir_entry *proc_firmware;
struct proc_dir_entry *proc_mp_test;
struct proc_dir_entry *proc_gesture;
struct proc_dir_entry *proc_glove;

struct file_operations proc_i2c_fops = {
	.unlocked_ioctl = ilitek_proc_i2c_ioctl,
};

struct file_operations proc_firmware_fops = {
	.read  = ilitek_proc_firmware_read,
	.write = ilitek_proc_firmware_write,
};

struct file_operations proc_mp_test_fops = {
	.read  = ilitek_proc_mp_test_read,
	.write = ilitek_proc_mp_test_write,
};

struct file_operations proc_gesture_fops = {
	.read  = ilitek_proc_gesture_read,
	.write = ilitek_proc_gesture_write,
};

struct file_operations proc_glove_fops = {
	.read  = ilitek_proc_glove_read,
	.write = ilitek_proc_glove_write,
};

/*
 * This struct lists all file nodes will be created under /proc filesystem.
 *
 * Before creating a node that you want, declaring its file_operations structure
 * is necessary. After that, puts the structure into proc_table, defines its
 * node's name in the same row, and the init function lterates the table and
 * creates all nodes under /proc.
 *
 */
typedef struct {

	// node's name.
	char *name;

	// point to a node created by proc_create.
	struct proc_dir_entry *node;

	// point to a fops declard the above already.
	struct file_operations *fops;

	// indicate if a node is created.
	bool isCreated;
	
} proc_node_t;

proc_node_t proc_table[] = {
	{"i2c",		 NULL, &proc_i2c_fops,     false},
	{"firmware", NULL, &proc_firmware_fops, false},
	{"mp_test",  NULL, &proc_mp_test_fops, false},
	{"gesture",  NULL, &proc_gesture_fops, false},
	{"glove",    NULL, &proc_glove_fops, false},
};

int ilitek_proc_init(void)
{
	int i = 0, res = 0;
	int node = sizeof(proc_table) / sizeof(proc_table[0]);

	proc_dir_ilitek = proc_mkdir("ilitek", NULL);

	for(; i < node; i++)
	{
		proc_table[i].node = proc_create(proc_table[i].name, 0666, proc_dir_ilitek, proc_table[i].fops);

		if(proc_table[i].node == NULL)
		{
			proc_table[i].isCreated = false;
			DBG_ERR("Failed to create %s under /proc", proc_table[i].name);
			res = -ENODEV;
		}
		else
		{
			proc_table[i].isCreated = true;
			DBG_INFO("Succeed to create %s under /proc", proc_table[i].name);
		}
	}

	return res;
}

void ilitek_proc_remove(void)
{
	int i = 0;
	int node = sizeof(proc_table) / sizeof(proc_table[0]);

	for(; i < node; i++)
	{
		if(proc_table[i].isCreated == true)
		{
			DBG_INFO("Removed %s under /proc", proc_table[i].name);
			remove_proc_entry(proc_table[i].name, proc_dir_ilitek);
		}
	}

	remove_proc_entry("ilitek", NULL);
}

void netlink_recv_msg(struct sk_buff *skb)
{
#if 0
	struct nlmsghdr *nlh;
	int pid;
	struct sk_buff *skb_out;
	int msg_size;
	char *msg = "Hello from kernel";
	int res;

	DBG_INFO();

	msg_size = strlen(msg);

	nlh = (struct nlmsghdr *)skb->data;
	printk(KERN_INFO "Netlink received msg payload:%s\n", (char *)nlmsg_data(nlh));
	pid = nlh->nlmsg_pid; /*pid of sending process */

	skb_out = nlmsg_new(msg_size, 0);
	if (!skb_out) {
		printk(KERN_ERR "Failed to allocate new skb\n");
		return;
	}

	nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
	NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
	strncpy(nlmsg_data(nlh), msg, msg_size);

	res = nlmsg_unicast(nl_sk, skb_out, pid);
	if (res < 0)
		printk(KERN_INFO "Error while sending bak to user\n");
#endif
}

int netlink_init(void)
{
	int res = 0;

	DBG_INFO();
#if 0
	struct netlink_kernel_cfg cfg = {
		.input = netlink_recv_msg,
	};

	nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);

	if(!nl_sk)
	{
		DBG_ERR("Failed to create nelink socket");
		return -EFAULT;
	}
#endif 

	return res;
}

void netlink_close(void)
{
	DBG_INFO();

	//netlink_kernel_release(nl_sk);

	ilitek_proc_remove();
}
