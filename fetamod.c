#include <linux/module.h>
#include <linux/pci.h>
#include <linux/types.h>

/*
0: boot vm, insert kernel module (feta), rebind (the sole) pci-ahci device to use fetadrv module
1: feta takes posession of the device (fetadrv_init)
2: feta initialises the device (feta_enable_pci_device)
3: userland coponent (olive) tells fuzz handler it's starting (with ID and a new random seed. see: iofuzz net handler)
4: olive issues START_FUZZ ioctl to feta and gives a seed (olive.py, using IOCTL values from olive_c)
5: feta runs fuzz round as instructed
6: feta signals to olive that the fuzz round has completed
7: olive calls out to net handler to say that seed has completed
8: goto 4
*/

/*
so, what are we aiming to fuzz here? the whole PCI subsystem, or just the ATA/AHCI parts?
if we use the linux pci_enable_device functions, then we won't be albe to fuzz the PCI parts from initialisation.

TODO: use them for now, but bear this in mind for future fuzzing targets.
*/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("roddux");
MODULE_DESCRIPTION("feta");

int query_ioctl_init(void);
void query_ioctl_exit(void);
extern uint64_t seed;

static struct pci_driver fetadriver;
int ret = 0;

void start_fuzz(void) {
	/*
		Fuzzing will be started/stopped with IOCTLs by olive.
	*/
	printk("feta: %s() called\n", __func__);
	printk("feta: current seed is %#12x\n", seed);
/*	operations = [read sector, write sector, detect disk, select disk, reset, ...]
	printk("Entering fuzz loop")
	for (;;) {
		printk("fuzz round")
		switch random.choice(operations) {
			case "read_sector":
				printk("reading sector [..]");
				read_sector()
			[..]
		}
	}
*/
}

static int __init test_init(void) {
	/*
		This is the function called when the module loads. Here we'll actually
		establish ourselves as a PCI driver (and setup some IOCTLs.)
	*/
	printk("feta: %s() called\n", __func__);
/*
	setup_irqs()
	get memory 
*/
	printk("feta: calling pci_register_driver()\n");
	ret = pci_register_driver(&fetadriver);
	if (ret < 0) {
		printk("feta: error registering driver!\n");
		return -1;
	} else {
		printk("feta: fetadrv registered, ready for IOCTL calls.\n");
	}
	return 0;
}

int feta_enable_pci_device(struct pci_dev *pdev) {
	/*
		pci_enable_device sets up our IRQs and memory regions
	*/
	printk("feta: calling pci_enable_device()\n");
	ret = pci_enable_device(pdev);
	if (ret < 0) {
		printk("feta: error enabling device!\n");
		return -1;
	} else {
		printk("feta: device enabled\n");
	}
	return 0;
}

static int fetadrv_init(struct pci_dev *pdev, const struct pci_device_id *ent) {
	printk("feta: %s() called\n", __func__);
	printk("feta: driver initialised -- PCI device found:\n");
	printk("feta: vendor: %#04x / device: %#04x / class: %#08x\n", pdev->vendor, pdev->device, pdev->class);

	if (feta_enable_pci_device(pdev)<0) return -1;

	printk("feta: adding control device /dev/feta\n");
	if (query_ioctl_init()<0) return -1;
	return 0;
}

static void fetadrv_remove(struct pci_dev *pdev) {
	printk("feta: %s() called\n", __func__);
	printk("feta: pci_unregister_driver\n");
	printk("feta: driver unregistered, good luck with development!\n");
}

static void __exit test_exit(void) {
	printk("feta: %s() called\n", __func__);
	pci_unregister_driver(&fetadriver);
	query_ioctl_exit();
}

static const struct pci_device_id mydevices[] = {
	/* Generic, PCI class code for AHCI */
	{ PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, PCI_CLASS_STORAGE_SATA_AHCI,
	  0xffffff, 0 },
	{}
};

static struct pci_driver fetadriver = {
	.name			= "fetadrv",
	.id_table		= mydevices,
	.probe			= fetadrv_init,
	.remove			= fetadrv_remove
};

module_init(test_init);
module_exit(test_exit);
