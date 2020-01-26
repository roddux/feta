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

static void start_fuzz(void);

int my_open(struct inode *i, struct file *f) {
	return 0;
}
int my_close(struct inode *i, struct file *f) {
	return 0;
}

long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
	switch (cmd) {
		case START_FUZZ:
			start_fuzz();
			break;
		case SET_SEED:
			printk("feta: got a set_variable command\n");
			printk("feta: arg is %ul64 %#16x (%p)\n", arg);
			printk("feta: val is %ul64 %#16x", arg, arg);
			copy_from_user(&seed, (uint64_t *)arg, sizeof(uint64_t));
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