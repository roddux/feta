#include <linux/module.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/random.h>
#include <uapi/asm/errno.h>
#include <linux/dma-mapping.h>
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

/*
	Stop an AHCI port (stolen from OSDev)
*/
void stop_pump(HBA_PORT *y) {
	LOG("about to kill command pump");
	LOG("looping until device indicates it has stopped commands"); 
	LOG("device has now stopped"); 
}

/*
	Start an AHCI port (stolen from OSDev)
*/
void start_pump(HBA_PORT *port) {
	LOG("about to start command pump");
	LOG("looping until device indicates it has started"); 
	LOG("device has started");
}

/*
	Fuzzing will be started/stopped with IOCTLs by olive.
*/
void start_fuzz(void) { LOG("doing a fuzz (placeholder)"); }

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
	LOG("calling pci_register_driver()");

	RETDIE(pci_register_driver(&fetadriver), "feta-init: error registering driver!\n");

	LOG("fetadrv registered, ready for IOCTL calls.");
	return 0;
}


typedef struct {
	uint64_t abar_virt_addr;
	uint64_t abar_phys_addr;
	struct pci_dev *dev;
} ahcidev;
ahcidev fuck;

#define ABAR 5

/*
	pci_enable_device sets up our IRQs and memory regions automagically
*/
int feta_enable_pci_device(struct pci_dev *pdev) {
	LOG("calling pci_enable_device()");
	RETDIE(pci_enable_device(pdev), "error enabling device! error: %d\n", ret);
	LOG("device enabled");

	LOG("calling pci_request_regions()");
	RETDIE(pci_request_regions(pdev, "fetadrv"), "can't grab memory region! error: %d\n", ret);
	LOG("got memory regsions");

	// assign module-wide pointer so we can access from other functions
	fuck.dev = pdev;

	// BAR 5 is what we want, ABAR/AHCI base mem 
	fuck.abar_virt_addr = (void *)pci_iomap(fuck.dev, ABAR, 0); // 0 == whole size
	LOG("got virtual address of ABAR: %#16llx", fuck.abar_virt_addr);

	uint64_t start = (uint64_t) pci_resource_start(pdev, ABAR);
	uint64_t len   = (uint64_t) pci_resource_len(pdev, ABAR);
	LOG("start of pci_resource_start(pdev, 5): %llu (%#16llx)", start, start);
	LOG("length of pci_resource_start(pdev, 5): %llu (%#16llx)", len, len);
	fuck.abar_phys_addr = start;
	LOG("got physical address of ABAR: %#16llx", fuck.abar_phys_addr);

	// lets use ioreadXX and offsets
	uint32_t pi = ioread32(fuck.abar_virt_addr+offsetof(HBA_MEM,pi));
	LOG("pointed HBA_MEM struct at ABAR");
	LOG("(mem->pi) port implemented == %u (%#8llx)", pi, pi);

	// stop pump
	LOG("about to stop command pump");
	uint32_t portbit = ioread32(fuck.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,cmd));
	portbit &= ~HBA_PxCMD_ST;
	portbit &= ~HBA_PxCMD_FRE;
	iowrite32(portbit, fuck.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,cmd));
	LOG("looping until device indicates it has stopped"); 
	while(1) {
		portbit = ioread32(fuck.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,cmd));
		if ( portbit & HBA_PxCMD_FR ) continue;
		if ( portbit & HBA_PxCMD_CR ) continue;
		break;
	}
	LOG("device has stopped - done");

	// bios already did it. can we just use this addr?
	LOG("clb == command list base addr");
	uint32_t clb = ioread32(fuck.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,clb));
	                                         // + sizeof(HBA_PORT)*portnum  -- 0 in this case
	LOG("(mem->port[0].clb)  == %u (%#8llx)", clb, clb);
	uint32_t clbu = ioread32(fuck.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,clbu));
	                                         //  + sizeof(HBA_PORT)*portnum -- 0 in this case
	LOG("(mem->port[0].clbu) == %u (%#8llx)", clbu, clbu);

	#define CMDSLOTS 32
	LOG("assigning *cmdheader to %#8llx", clb);
	uintptr_t cmd_header_addr = ioremap(clb, sizeof(HBA_CMD_HEADER)*CMDSLOTS);

	// if we have space free in high mem, we should just be able to use it directly
	// did the system reserve it? who knows. let's just try lol.
	#define PORT 0
	uint32_t prdtl_before =ioread32(cmd_header_addr + offsetof(HBA_CMD_HEADER,prdtl)); 
	LOG("prdtl before: %u %d %#8llx", prdtl_before, prdtl_before, prdtl_before);
	LOG(" ctba before: %#8llx", ioread32(cmd_header_addr + offsetof(HBA_CMD_HEADER,ctba)));
	uint8_t cmdslot = 0;
//	for(cmdslot = 0; cmdslot < CMDSLOTS; cmdslot++) {
		uint32_t new_prdtl = 8;
		iowrite32(new_prdtl, cmd_header_addr + offsetof(HBA_CMD_HEADER,prdtl));
		uint32_t new_ctba = fuck.abar_phys_addr + (40<<10) + (PORT<<13) + (cmdslot<<8);
		iowrite32(new_ctba, cmd_header_addr + offsetof(HBA_CMD_HEADER,ctba));
		// how do we memset(); using iowrite() ? for loops? fucks sake.
//	}
	uint32_t prdtl_new =ioread32(cmd_header_addr + offsetof(HBA_CMD_HEADER,prdtl)); 
	LOG("prdtl after: %u %d %#8llx", prdtl_new, prdtl_new, prdtl_new);
	LOG(" ctba after: %#8llx", ioread32(cmd_header_addr + offsetof(HBA_CMD_HEADER,ctba)));

	// start pump
	LOG("about to start command pump");
	portbit = ioread32(fuck.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,cmd));
	LOG("looping until device indicates it has started"); 
	while(portbit & HBA_PxCMD_CR) {
		portbit = ioread32(fuck.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,cmd));
	}
	LOG("device has started, setting bits...");
	portbit |= HBA_PxCMD_FRE;
	portbit |= HBA_PxCMD_ST;
	iowrite32(portbit, fuck.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,cmd));
	LOG("done");

	uint32_t q;
	get_random_bytes(&q, sizeof(q));

	uint32_t u;
	get_random_bytes(&u, sizeof(u));

	int _i=0;
	for ( _i=0;_i<10;_i++ ) {
		// send FIS
		LOG("gonna send a FIS! first, clearing interrupts");
		iowrite32(-1, fuck.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,is)); // clear int
		#define slot 0
		// fis size
		LOG("setting FIS size, that we're reading, with 0 prdt entries");
	/* we can't use offsetof for bitfields! fuck.
	cfl:5;		// Command FIS length in DWORDS, 2 ~ 16
	a:1;		// ATAPI
	w:1;		// Write, 1: H2D, 0: D2H
	p:1;		// Prefetchable
	all make up one byte, though! so...    cfl  awp*/
		uint8_t fislen_atapi_write = 0b00000000;
		fislen_atapi_write = sizeof(FIS_REG_H2D)/sizeof(uint32_t) << 4;
		/*LOG("maybe? : %8llx %c%c%c%c%c%c%c%c", fislen_atapi_write,
			(fislen_atapi_write & 0b00000001) ? '1' : '0',
			(fislen_atapi_write & 0b00000010) ? '1' : '0',
			(fislen_atapi_write & 0b00000100) ? '1' : '0',
			(fislen_atapi_write & 0b00001000) ? '1' : '0',
			(fislen_atapi_write & 0b00010000) ? '1' : '0',
			(fislen_atapi_write & 0b00100000) ? '1' : '0',
			(fislen_atapi_write & 0b01000000) ? '1' : '0',
			(fislen_atapi_write & 0b10000000) ? '1' : '0'
		);*/
		//iowrite32(sizeof(FIS_REG_H2D)/sizeof(uint32_t), cmd_header_addr + offsetof(HBA_CMD_HEADER,cfl)); 
		//iowrite32(fislen_atapi_write, cmd_header_addr);
		iowrite32(q, cmd_header_addr);
		//iowrite32(0, cmd_header_addr + offsetof(HBA_CMD_HEADER,w)); // read
		iowrite32(u, cmd_header_addr + offsetof(HBA_CMD_HEADER,prdtl)); // prdt entries

		LOG("grabbing cmd_tbl with ioremap");
		#define CMDSLOTS 1
		uintptr_t cmd_tbl = ioremap(
			clb + offsetof(HBA_CMD_HEADER,ctba),
			sizeof(HBA_CMD_TBL)*CMDSLOTS
		);

		uint16_t i = 0;

		LOG("poor man's memset");
		memset_io(cmd_tbl + offsetof(HBA_CMD_TBL,cfis),
			0,
			(sizeof(HBA_CMD_TBL)*CMDSLOTS) + (sizeof(HBA_PRDT_ENTRY)*1)
		);

	#if 0
		// 8K bytes (16 sectors) per PRDT
		for (int i=0; i<cmdheader->prdtl-1; i++)
		{
			cmdtbl->prdt_entry[i].dba = (uint32_t) buf;
			cmdtbl->prdt_entry[i].dbc = 8*1024-1;	// 8K bytes (this value should always be set to 1 less than the actual value)
			cmdtbl->prdt_entry[i].i = 1;
			buf += 4*1024;	// 4K words
			count -= 16;	// 16 sectors
		}
		// Last entry
		cmdtbl->prdt_entry[i].dba = (uint32_t) buf;
		cmdtbl->prdt_entry[i].dbc = (count<<9)-1;	// 512 bytes per sector
		cmdtbl->prdt_entry[i].i = 1;
	#endif

		

		LOG("setting up FIS cmd");
		// Setup command
		FIS_REG_H2D cmdfis;
/*		cmdfis.fis_type = FIS_TYPE_REG_H2D;
		cmdfis.c = 1;	// Command
		cmdfis.command = e; // fuck it
		cmdfis.lba0 = (uint8_t)0;
		cmdfis.lba1 = (uint8_t)0;
		cmdfis.lba2 = (uint8_t)0;
		cmdfis.device = 1<<6;	// LBA mode
		cmdfis.lba3 = (uint8_t)0;
		cmdfis.lba4 = (uint8_t)0;
		cmdfis.lba5 = (uint8_t)0;
		cmdfis.countl = 0& 0xFF;
		cmdfis.counth = (0>> 8) & 0xFF;*/

		get_random_bytes(&cmdfis, sizeof(cmdfis));

		LOG("writing FIS to memory");
		// Write FIS
		memcpy_toio(cmd_tbl + offsetof(HBA_CMD_TBL,cfis), &cmdfis, sizeof(FIS_REG_H2D));


#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08

		LOG("waiting for port to non-busy");
		portbit = ioread32(fuck.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,tfd));
		uint32_t spin = 0;
		while( (portbit & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000 )  {
			portbit = ioread32(fuck.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,cmd));
		}
		if (spin == 1000000) {
			LOG("Port is hung\n");
			return -EFAULT;
		}
	
		LOG("issuing cmd lads");
		// issue cmd
		iowrite32(1, fuck.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,ci));
		
		LOG("waiting for cmd completion"); spin=0;
		portbit = ioread32(fuck.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,ci));
		while ( spin < 1000000 ) {
			if ((portbit & (1<<0)) == 0) 
				break;
			spin++;
		}
		LOG("did we win?");


	}

	return 0;
}

static int fetadrv_init(struct pci_dev *pdev, const struct pci_device_id *ent) {
	LOG(
		"found PCI device. vendor: 0x%04x / device: 0x%04x / class: 0x%08x",
		pdev->vendor,
		pdev->device,
		pdev->class
	);
	RETDIE(feta_enable_pci_device(pdev), "feta-fetadrv_init: could not enable_pci_device\n");
	// LOG("adding control device /dev/feta");
	// RETDIE(query_ioctl_init(), "feta-fetadrv_init: could not init ioctl driver\n");

	// ahci_init_one(pdev, ent);
	return 0;
}

static void fetadrv_remove(struct pci_dev *pdev) {
	LOG("pci_release_regions()");
	pci_release_regions(fuck.dev);
	LOG("driver unregistered, good luck with development!");
}

static void __exit test_exit(void) {
	LOG("pci_unregister_driver()");
	pci_unregister_driver(&fetadriver);
	LOG("ioctl handler teardown");
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