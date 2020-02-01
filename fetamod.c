#include <linux/module.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <uapi/asm/errno.h>
#include "defs.h"

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

void start_fuzz(void) {
	/*
		Fuzzing will be started/stopped with IOCTLs by olive.
	*/
	printk("feta: %s() called\n", __func__);
	printk("feta: current seed is %llu %llx 0x%16x\n", seed, seed, seed);

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

	// stop commands (lifted straight from osdev)
	// Clear ST (bit0)
	printk("feta: about to kill command pump\n"); 
#define AHCI_BASE	0x400000
#define HBA_PxCMD_ST    0x0001
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR    0x4000
#define HBA_PxCMD_CR    0x8000
	y->cmd &= ~HBA_PxCMD_ST;

	printk("feta: looping until device indicates it has stopped commands\n"); 
	// Wait until FR (bit14), CR (bit15) are cleared
	while(1) {
		if (y->cmd & HBA_PxCMD_FR) continue;
		if (y->cmd & HBA_PxCMD_CR) continue;
		break;
	}
	printk("feta: signal received!\n"); 

	// Clear FRE (bit4)
	y->cmd &= ~HBA_PxCMD_FRE;

	printk("feta: allocating memory for command base\n");
	// Command list offset: 1K*0
	// Command list entry size = 32
	// Command list entry maxim count = 32
	// Command list maxim size = 32*32 = 1K per port
	y->clb = AHCI_BASE + (1<<10);
	y->clbu = 0;

	printk("feta: port->clb:  %lx 0x%08x\n", y->clb,  y->clb);
	printk("feta: port->clbu: %lx 0x%08x\n", y->clbu, y->clbu);
	
	// make clb into a virtual address, cuz it's currently physical - so we cannae touch it
	void *addr = ioremap(y->clb, sizeof(HBA_CMD_HEADER)); 

	memset((void*)(addr), 1, sizeof(HBA_CMD_HEADER));
	printk("feta: done\n");

	// FIS offset: 32K+256*0
	// FIS entry size = 256 bytes per port
	printk("feta: allocating memory at AHCI_BASE\n");
	y->fb = AHCI_BASE + (32<<10) + (1<<8);
	void *fbaddr = ioremap(y->fb, 256);
	y->fbu = 0;
	memset((void*)(fbaddr), 0, 256);
	printk("feta: done\n");
	
	printk("feta: setting up HBA_CMD_HEADER\n");
	// Command table offset: 40K + 8K*0
	// Command table size = 256*32 = 8K per port
	HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)(fbaddr);
	
	printk("feta: writing junk value to fbaddr\n");
	iowrite32(0xdeadbeef, fbaddr);
#if 0
	cmdheader[0].prdtl = 8;	// 8 prdt entries per command table
	// 256 bytes per command table, 64+16+48+16*8
	// Command table offset: 40K + 8K*0 + cmdheader_index*256
	cmdheader[0].ctba = AHCI_BASE + (40<<10) + (0<<13) + (0<<8);
	cmdheader[0].ctbau = 0;
	memset((void*)cmdheader[0].ctba, 0, 256);
#endif

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
		return -1;
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
