#include "defs.h"

/*
fetamod: PCI driver that issues ATA commands to a SATA/AHCI drive
         as directed by ioctl commands

Linux modules ahci / libahci sort out mmio addressing by storing the
mmio address in a private data struct. Accesses are then done by ioread
and iowrite using relative addressing. Let's do that!
*/

static struct pci_driver fetadriver;
int ret = 0;

glob_state_hack gState;

/*
	Fuzzing will be started/stopped with IOCTLs by olive.
*/ // new name: Fizzle (FIS+FUZZ)
void start_fuzz(void) {
	send_random_fis();
}

/*
	This is the function called when the module loads. Here we'll actually
	establish ourselves as a PCI driver (and setup some IOCTLs.)
*/
static int __init test_init(void) {
	LOG("initialising!");
	/*
		register this module as a PCI driver. as soon as we have a PCI device
		associated with us (which will happen when using the rebind.sh script)
		then the function fetadrv_init will be called.
	*/
	//LOG("calling pci_register_driver()");
	RETDIE(pci_register_driver(&fetadriver), "feta-init: error registering driver!\n");
	// LOG("fetadrv registered, ready for IOCTL calls.");
	return 0;
}

/*
	pci_enable_device sets up our IRQs and memory regions automagically
*/
int feta_enable_pci_device(struct pci_dev *pdev) {
	//LOG("calling pci_enable_device()");
	RETDIE(pci_enable_device(pdev), "error enabling device! error: %d\n", ret);
	//LOG("device enabled");

	//LOG("calling pci_request_regions()");
	RETDIE(pci_request_regions(pdev, "fetadrv"), "can't grab memory region! error: %d\n", ret);
	//LOG("got memory regsions");

	// assign module-wide pointer so we can access from other functions
	gState.dev = pdev;

	// BAR 5 is what we want, ABAR/AHCI base mem 
	gState.abar_virt_addr = (void *)pci_iomap(gState.dev, ABAR, 0); // 0 == whole size
	//LOG("got virtual address of ABAR: %#16llx", gState.abar_virt_addr);

	uint64_t start = (uint64_t) pci_resource_start(pdev, ABAR);
	uint64_t len   = (uint64_t) pci_resource_len(pdev, ABAR);
	//LOG("start of pci_resource_start(pdev, 5): %llu (%#16llx)", start, start);
	//LOG("length of pci_resource_start(pdev, 5): %llu (%#16llx)", len, len);
	gState.abar_phys_addr = start;
	//LOG("got physical address of ABAR: %#16llx", gState.abar_phys_addr);

	// let's use ioreadXX and offsets
	uint32_t pi = ioread32(gState.abar_virt_addr+offsetof(HBA_MEM,pi));
	//LOG("pointed HBA_MEM struct at ABAR");
	LOG("ports implemented == %u (%#8llx)", pi, pi);

	uint32_t cap = ioread32(gState.abar_virt_addr+offsetof(HBA_MEM,cap));
	// we want ports implemented. that is 4 bits from 8 to 12
	// so if we have a 32-bit variable; 0b01001100111011011101110101110111;
	// that's these bits                 |                    ^^^^        |
	uint8_t commandSlotsImplemented = (cap>>8) & 0b00001111; // bits 8->12 are cmd slots
	LOG("%u slots implemented", commandSlotsImplemented);    // is this right? maybe
	printBit(commandSlotsImplemented, 8);

	stop_pump();

	//LOG("clb == command list base addr");
	gState.clb = ioread32(gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,clb));
	                                         // + sizeof(HBA_PORT)*portnum  -- 0 in this case
	//LOG("(mem->port[0].clb)  == %u (%#8llx)", gState.clb, gState.clb);
	//uint32_t clbu = ioread32(gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,clbu));
	                                         //  + sizeof(HBA_PORT)*portnum -- 0 in this case
	//LOG("(mem->port[0].clbu) == %u (%#8llx)", clbu, clbu);

	uint32_t _cmdslots = commandSlotsImplemented+1; // 0-based
	LOG("assigning cmd_header to %#8llx", gState.clb);
	gState.cmd_header_addr = ioremap(gState.clb, sizeof(HBA_CMD_HEADER)*_cmdslots);

	// uint16_t prdtl_before = ioread32(gState.cmd_header_addr + offsetof(HBA_CMD_HEADER,prdtl)); 
	//LOG("prdtl before: %u %d %#8llx", prdtl_before, prdtl_before, prdtl_before);
	//LOG(" ctba before: %#8llx", ioread32(gState.cmd_header_addr + offsetof(HBA_CMD_HEADER,ctba)));
	uint8_t cmdslot = 0;
//	for(cmdslot = 0; cmdslot < _cmdslots; cmdslot++) {
		uint16_t new_prdtl = 8;
		iowrite16(new_prdtl, gState.cmd_header_addr + offsetof(HBA_CMD_HEADER,prdtl));
	//	uint32_t new_ctba = gState.abar_phys_addr + (40<<10) + (PORT<<13) + (cmdslot<<8);
	//	iowrite32(new_ctba, gState.cmd_header_addr + offsetof(HBA_CMD_HEADER,ctba));
//	}
	// uint16_t prdtl_new =ioread16(gState.cmd_header_addr + offsetof(HBA_CMD_HEADER,prdtl)); 
	//LOG("prdtl after: %u %d %#8llx", prdtl_new, prdtl_new, prdtl_new);
	//LOG(" ctba after: %#8llx", ioread32(gState.cmd_header_addr + offsetof(HBA_CMD_HEADER,ctba)));

	LOG("grabbing cmd_tbl with ioremap at addr %#8llx", ioread32(gState.cmd_header_addr + offsetof(HBA_CMD_HEADER,ctba)));
	
	// TODO: fix this bit. somehow. 
	gState.cmd_tbl = ioremap(
		ioread32(gState.cmd_header_addr + offsetof(HBA_CMD_HEADER,ctba)),
		sizeof(HBA_CMD_TBL)*32
	);

	start_pump();

	// sort out mem addresses
	// dev music: https://www.youtube.com/watch?v=HewCisojdkI&list=PLStQSHqGPa8hbvCdKSePLLxUWAAiXFn3D&index=44

	return 0;
}

static int fetadrv_init(struct pci_dev *pdev, const struct pci_device_id *ent) {
	/*LOG(
		"found PCI device. vendor: 0x%04x / device: 0x%04x / class: 0x%08x",
		pdev->vendor,
		pdev->device,
		pdev->class
	);*/
	RETDIE(feta_enable_pci_device(pdev), "feta-fetadrv_init: could not enable_pci_device\n");
	// LOG("adding control device /dev/feta");
	RETDIE(query_ioctl_init(), "feta-fetadrv_init: could not init ioctl driver\n");

	return 0;
}

static void fetadrv_remove(struct pci_dev *pdev) {
	// LOG("pci_release_regions()");
	pci_release_regions(gState.dev);
	// LOG("driver unregistered, good luck with development!");
}

static void __exit test_exit(void) {
	// LOG("pci_unregister_driver()");
	pci_unregister_driver(&fetadriver);
	// LOG("ioctl handler teardown");
	query_ioctl_exit();
}

static struct pci_driver fetadriver = {
	.name			= "fetadrv",
	.id_table		= mydevices,
	.probe			= fetadrv_init,
	.remove			= fetadrv_remove
};

module_init(test_init);
module_exit(test_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("roddux");
MODULE_DESCRIPTION("feta");