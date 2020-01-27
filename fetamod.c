#include <linux/module.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <uapi/asm/errno.h>

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

struct pci_dev *my_device;

typedef volatile struct tagHBA_PORT {
	uint32_t clb;		// 0x00, command list base address, 1K-byte aligned
	uint32_t clbu;		// 0x04, command list base address upper 32 bits
	uint32_t fb;		// 0x08, FIS base address, 256-byte aligned
	uint32_t fbu;		// 0x0C, FIS base address upper 32 bits
	uint32_t is;		// 0x10, interrupt status
	uint32_t ie;		// 0x14, interrupt enable
	uint32_t cmd;		// 0x18, command and status
	uint32_t rsv0;		// 0x1C, Reserved
	uint32_t tfd;		// 0x20, task file data
	uint32_t sig;		// 0x24, signature
	uint32_t ssts;		// 0x28, SATA status (SCR0:SStatus)
	uint32_t sctl;		// 0x2C, SATA control (SCR2:SControl)
	uint32_t serr;		// 0x30, SATA error (SCR1:SError)
	uint32_t sact;		// 0x34, SATA active (SCR3:SActive)
	uint32_t ci;		// 0x38, command issue
	uint32_t sntf;		// 0x3C, SATA notification (SCR4:SNotification)
	uint32_t fbs;		// 0x40, FIS-based switch control
	uint32_t rsv1[11];	// 0x44 ~ 0x6F, Reserved
	uint32_t vendor[4];	// 0x70 ~ 0x7F, vendor specific
} HBA_PORT;

typedef volatile struct tagHBA_MEM {
	// 0x00 - 0x2B, Generic Host Control
	uint32_t cap;		// 0x00, Host capability
	uint32_t ghc;		// 0x04, Global host control
	uint32_t is;		// 0x08, Interrupt status
	uint32_t pi;		// 0x0C, Port implemented
	uint32_t vs;		// 0x10, Version
	uint32_t ccc_ctl;	// 0x14, Command completion coalescing control
	uint32_t ccc_pts;	// 0x18, Command completion coalescing ports
	uint32_t em_loc;		// 0x1C, Enclosure management location
	uint32_t em_ctl;		// 0x20, Enclosure management control
	uint32_t cap2;		// 0x24, Host capabilities extended
	uint32_t bohc;		// 0x28, BIOS/OS handoff control and status

	// 0x2C - 0x9F, Reserved
	uint8_t  rsv[0xA0-0x2C];

	// 0xA0 - 0xFF, Vendor specific registers
	uint8_t  vendor[0x100-0xA0];

	// 0x100 - 0x10FF, Port control registers
	HBA_PORT	ports[1];	// 1 ~ 32
} HBA_MEM;




// long unsigned == uint32_t = 0xffffffff = %lu / %lx / 0x+PRIx32
// long long unsigned == uint64_t = 0xffffffffffffffff = %llu / %llx /  0x+PRIx64
void start_fuzz(void) {
	/*
		Fuzzing will be started/stopped with IOCTLs by olive.
	*/
	printk("feta: %s() called\n", __func__);
	printk("feta: current seed is %llu %llx 0x%16x\n", seed, seed, seed);

/*
	// BAR 5 points to ABAR, points to AHCI base memory
	printk("feta: issuing pci_resource_start()");
	uint32_t memstart = pci_resource_start(my_device, 5); // BAR 0 memory start
	uint32_t memend = pci_resource_end(my_device, 5); // BAR 0 memory end
	uint32_t memflags = pci_resource_flags(my_device, 5); // BAR 0 memory flags
	printk("feta: memstart: %lx\n", memstart);
	printk("feta: memend:   %lx\n", memend);
	printk("feta: memflags: %lx\n", memflags);
*/

	HBA_MEM *x;
	HBA_PORT *y;

	// BAR 5 is what we want, "ABAR"/AHCI base mem
	x = (void *)pci_iomap(my_device, 5, sizeof(HBA_MEM));
	printk("feta: assigned HBA_MEM struct to memory address %lx 0x%08x\n", x, x);
	printk("feta: ports implemented (HBA_MEM.pi) is %lx 0x%08x\n", x->pi, x->pi);

	y = &x->ports[0];
// bullshit magic detection stolen from OSDev wiki
	uint32_t ssts = y->ssts;
	uint8_t ipm = (ssts >> 8) & 0x0F;
	uint8_t det = ssts & 0x0F;
#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3
	printk("feta: port present val: 0x%08x\n", det);
	printk("feta: port active val: 0x%08x\n", ipm);

	printk("feta: assigned HBA_PORT struct to memory address %lx 0x%08x\n", &x->ports[0], &x->ports[0]);
	printk("feta: drive signature (HBA_PORT.sig) is %lx 0x%08x\n", y->sig, y->sig);

	// ITS A SATA DRIVE LADS https://www.youtube.com/watch?v=Y5tjtUFL0j4 WE FUCKING DID IT

	// now we gotta remap the AHCI memory address to somewhere we can read/write
	// lets print them first
	printk("feta: port->clb:  %lx 0x%08x\n", y->clb,  y->clb);
	printk("feta: port->clbu: %lx 0x%08x\n", y->clbu, y->clbu);

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

	/*
		register this module as a PCI driver. as soon as we have a PCI device
		associated with us (which will happen when using the rebind.sh script)
		then the function fetadrv_init will be called.
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
		pci_enable_device sets up our IRQs and memory regions automagically
	*/
	printk("feta: calling pci_enable_device()\n");
	ret = pci_enable_device(pdev);
	if (ret < 0) {
		printk("feta: error enabling device! error: %d\n", ret);
		return -1;
	} else {
		printk("feta: device enabled\n");
	}
	
	printk("feta: calling pci_request_regions()\n");
	ret = pci_request_regions(pdev, "fetadrv");
	if (ret < 0) {
		printk("feta: unable to request memory region! error: %d\n", ret);
		return;
	} else {
		printk("feta: got memory regsions\n");
	}

	// assign module-wide pointer my_device so we can access pdev from other functions
	my_device = pdev;
	/*
		we can now use stuff like (int) pci_read_config_byte/word/dword and
		(int) pci_write_config_byte/word/dword to issue ATA commands
	*/

	/*
		pci_enable_device sets up our memory regions. we can figure out those
		regions by issuing: (unsigned long) pci_resource_start/end for each
		PCI bar
	*/
	return 0;
}

static int fetadrv_init(struct pci_dev *pdev, const struct pci_device_id *ent) {
	printk("feta: %s() called\n", __func__);
	printk("feta: driver initialised -- PCI device found:\n");
	printk("feta: vendor: 0x%04x / device: 0x%04x / class: 0x%08x\n", pdev->vendor, pdev->device, pdev->class);

	if (feta_enable_pci_device(pdev)<0) return -1;

	printk("feta: adding control device /dev/feta\n");
	if (query_ioctl_init()<0) return -1;
	return 0;
}

static void fetadrv_remove(struct pci_dev *pdev) {
	printk("feta: %s() called\n", __func__);
	printk("feta: pci_release_regions()\n");
	pci_release_regions(my_device);
	printk("feta: driver unregistered, good luck with development!\n");
}

static void __exit test_exit(void) {
	printk("feta: %s() called\n", __func__);
	printk("feta: pci_unregister_driver()\n");
	pci_unregister_driver(&fetadriver);
	printk("feta: ioctl handler teardown\n");
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
