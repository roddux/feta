#include <linux/version.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>

#define START_FUZZ _IO('q',  2)
#define SET_SEED _IOW('q', 3, unsigned long *)

#define FIRST_MINOR 0
#define MINOR_CNT 1

static dev_t dev;
static struct cdev c_dev;
static struct class *cl;
uint64_t seed = 0;

void start_fuzz(void);

int my_open(struct inode *i, struct file *f) {
	return 0;
}
int my_close(struct inode *i, struct file *f) {
	return 0;
}

// unsigned int = unsigned = uint32_t = 0xffffffff = %lu / 0x%08lx
// unsigned long long = unsigned long = uint64_t = 0xffffffffffffffff = %llu / 0x%16llx
long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
	switch (cmd) {
		case START_FUZZ:
			printk("feta-my_ioctl: ioctl handler received a START_FUZZ command\n");
			start_fuzz();
			break;
		case SET_SEED:
			printk("feta-my_ioctl: ioctl handler received a SET_SEED command\n");
			copy_from_user(&seed, (uint64_t *)arg, sizeof(uint64_t));
			printk("feta-my_ioctl: seed is now %llu 0x%16llx\n", seed, seed);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static struct file_operations query_fops = {
	.owner = THIS_MODULE,
	.open = my_open,
	.release = my_close,
	.unlocked_ioctl = my_ioctl
};

int query_ioctl_init(void) {
	int ret;
	struct device *dev_ret;

	if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, "query_ioctl")) < 0) {
		return ret;
	}

	cdev_init(&c_dev, &query_fops);

	if ((ret = cdev_add(&c_dev, dev, MINOR_CNT)) < 0) {
		return ret;
	}
	 
	if (IS_ERR(cl = class_create(THIS_MODULE, "char"))) {
		cdev_del(&c_dev);
		unregister_chrdev_region(dev, MINOR_CNT);
		return PTR_ERR(cl);
	}

	if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "feta"))) {
		class_destroy(cl);
		cdev_del(&c_dev);
		unregister_chrdev_region(dev, MINOR_CNT);
		return PTR_ERR(dev_ret);
	}

	return 0;
}

void query_ioctl_exit(void) {
	device_destroy(cl, dev);
	class_destroy(cl);
	cdev_del(&c_dev);
	unregister_chrdev_region(dev, MINOR_CNT);
}
