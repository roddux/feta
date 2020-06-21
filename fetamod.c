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






char *ata_cmd_names[] = {
	"ATA_NOP",
	"ATA_CFA_REQUEST_EXTENDED_ERROR_CODE",
	"ATA_DATA_SET_MANAGEMENT",
	"ATA_DEVICE_RESET",
	"ATA_RECALIBRATE",
	"ATA_READ_SECTORS",
	"ATA_READ_SECTORS_WITHOUT_RETRIES",
	"ATA_READ_LONG",
	"ATA_READ_LONG_WITHOUT_RETRIES",
	"ATA_READ_SECTORS_EXT",
	"ATA_READ_DMA_EXT",
	"ATA_READ_DMA_QUEUED_EXT",
	"ATA_READ_NATIVE_MAX_ADDRESS_EXT",
	"ATA_READ_MULTIPLE_EXT",
	"ATA_READ_STREAM_DMA_EXT",
	"ATA_READ_STREAM_EXT",
	"ATA_READ_LOG_EXT",
	"ATA_WRITE_SECTORS",
	"ATA_WRITE_SECTORS_WITHOUT_RETRIES",
	"ATA_WRITE_LONG",
	"ATA_WRITE_LONG_WITHOUT_RETRIES",
	"ATA_WRITE_SECTORS_EXT",
	"ATA_WRITE_DMA_EXT",
	"ATA_WRITE_DMA_QUEUED_EXT",
	"ATA_SET_MAX_ADDRESS_EXT",
	"ATA_CFA_WRITE_SECTORS_WITHOUT_ERASE",
	"ATA_WRITE_MULTIPLE_EXT",
	"ATA_WRITE_STREAM_DMA_EXT",
	"ATA_WRITE_STREAM_EXT",
	"ATA_WRITE_VERIFY",
	"ATA_WRITE_DMA_FUA_EXT",
	"ATA_WRITE_DMA_QUEUED_FUA_EXT",
	"ATA_WRITE_LOG_EXT",
	"ATA_READ_VERIFY_SECTORS",
	"ATA_READ_VERIFY_SECTORS_WITHOUT_RETRIES",
	"ATA_READ_VERIFY_SECTORS_EXT",
	"ATA_WRITE_UNCORRECTABLE_EXT",
	"ATA_READ_LOG_DMA_EXT",
	"ATA_FORMAT_TRACK",
	"ATA_CONFIGURE_STREAM",
	"ATA_WRITE_LOG_DMA_EXT",
	"ATA_TRUSTED_RECEIVE",
	"ATA_TRUSTED_RECEIVE_DMA",
	"ATA_TRUSTED_SEND",
	"ATA_TRUSTED_SEND_DMA",
	"ATA_READ_FPDMA_QUEUED",
	"ATA_WRITE_FPDMA_QUEUED",
	"ATA_SEEK",
	"ATA_CFA_TRANSLATE_SECTOR",
	"ATA_EXECUTE_DEVICE_DIAGNOSTIC",
	"ATA_INITIALIZE_DEVICE_PARAMETERS",
	"ATA_DOWNLOAD_MICROCODE",
	"ATA_STANDBY_IMMEDIATE__ALT",
	"ATA_IDLE_IMMEDIATE__ALT",
	"ATA_STANDBY__ALT",
	"ATA_IDLE__ALT",
	"ATA_CHECK_POWER_MODE__ALT",
	"ATA_SLEEP__ALT",
	"ATA_PACKET",
	"ATA_IDENTIFY_PACKET_DEVICE",
	"ATA_SERVICE",
	"ATA_SMART",
	"ATA_DEVICE_CONFIGURATION_OVERLAY",
	"ATA_NV_CACHE",
	"ATA_CFA_ERASE_SECTORS",
	"ATA_READ_MULTIPLE",
	"ATA_WRITE_MULTIPLE",
	"ATA_SET_MULTIPLE_MODE",
	"ATA_READ_DMA_QUEUED",
	"ATA_READ_DMA",
	"ATA_READ_DMA_WITHOUT_RETRIES",
	"ATA_WRITE_DMA",
	"ATA_WRITE_DMA_WITHOUT_RETRIES",
	"ATA_WRITE_DMA_QUEUED",
	"ATA_CFA_WRITE_MULTIPLE_WITHOUT_ERASE",
	"ATA_WRITE_MULTIPLE_FUA_EXT",
	"ATA_CHECK_MEDIA_CARD_TYPE",
	"ATA_GET_MEDIA_STATUS",
	"ATA_ACKNOWLEDGE_MEDIA_CHANGE",
	"ATA_BOOT_POST_BOOT",
	"ATA_BOOT_PRE_BOOT",
	"ATA_MEDIA_LOCK",
	"ATA_MEDIA_UNLOCK",
	"ATA_STANDBY_IMMEDIATE",
	"ATA_IDLE_IMMEDIATE",
	"ATA_STANDBY",
	"ATA_IDLE",
	"ATA_READ_BUFFER",
	"ATA_CHECK_POWER_MODE",
	"ATA_SLEEP",
	"ATA_FLUSH_CACHE",
	"ATA_WRITE_BUFFER",
	"ATA_WRITE_SAME",
	"ATA_FLUSH_CACHE_EXT",
	"ATA_IDENTIFY_DEVICE",
	"ATA_MEDIA_EJECT",
	"ATA_IDENTIFY_DMA",
	"ATA_SET_FEATURES",
	"ATA_SECURITY_SET_PASSWORD",
	"ATA_SECURITY_UNLOCK",
	"ATA_SECURITY_ERASE_PREPARE",
	"ATA_SECURITY_ERASE_UNIT",
	"ATA_SECURITY_FREEZE_LOCK",
	"ATA_SECURITY_DISABLE_PASSWORD",
	"ATA_READ_NATIVE_MAX_ADDRESS",
	"ATA_SET_MAX",
};

#define TOTCMDS 106
uint8_t ata_cmd_vals[TOTCMDS] = {
	0x00,
	0x03,
	0x06,
	0x08,
	0x10,
	0x20,
	0x21,
	0x22,
	0x23,
	0x24,
	0x25,
	0x26,
	0x27,
	0x29,
	0x2a,
	0x2b,
	0x2f,
	0x30,
	0x31,
	0x32,
	0x33,
	0x34,
	0x35,
	0x36,
	0x37,
	0x38,
	0x39,
	0x3a,
	0x3b,
	0x3c,
	0x3d,
	0x3e,
	0x3f,
	0x40,
	0x41,
	0x42,
	0x45,
	0x47,
	0x50,
	0x51,
	0x57,
	0x5c,
	0x5d,
	0x5e,
	0x5f,
	0x60,
	0x61,
	0x70,
	0x87,
	0x90,
	0x91,
	0x92,
	0x94,
	0x95,
	0x96,
	0x97,
	0x98,
	0x99,
	0xa0,
	0xa1,
	0xa2,
	0xb0,
	0xb1,
	0xb6,
	0xc0,
	0xc4,
	0xc5,
	0xc6,
	0xc7,
	0xc8,
	0xc9,
	0xca,
	0xcb,
	0xcc,
	0xcd,
	0xce,
	0xd1,
	0xda,
	0xdb,
	0xdc,
	0xdd,
	0xde,
	0xdf,
	0xe0,
	0xe1,
	0xe2,
	0xe3,
	0xe4,
	0xe5,
	0xe6,
	0xe7,
	0xe8,
	0xe9,
	0xea,
	0xec,
	0xed,
	0xee,
	0xef,
	0xf1,
	0xf2,
	0xf3,
	0xf4,
	0xf5,
	0xf6,
	0xf8,
	0xf9
};

static struct pci_driver fetadriver;
int ret = 0;

typedef struct {
	uint64_t abar_virt_addr;
	uint64_t abar_phys_addr;
	uint64_t clb;
	uintptr_t cmd_header_addr;
	uintptr_t cmd_tbl;
	struct pci_dev *dev;
} glob_state_hack;
glob_state_hack gState;

void start_pump(void); void stop_pump(void);
/*
	Stop an AHCI port (stolen from OSDev)
*/ // todo: un-hardcode from port0
void stop_pump(void) {
	// LOG("AYO HE FINNA ABOUT TO STOP THE COMMAND PUMP");
	uint32_t portbit = ioread32(gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,cmd));
	portbit &= ~HBA_PxCMD_ST;
	portbit &= ~HBA_PxCMD_FRE;
	iowrite32(portbit, gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,cmd));
	// LOG("looping until device indicates it has stopped"); 
	while(1) {
		portbit = ioread32(gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,cmd));
		if ( portbit & HBA_PxCMD_FR ) continue;
		if ( portbit & HBA_PxCMD_CR ) continue;
		break;
	}
	// LOG("device has stopped - done");
}

/*
	Start an AHCI port (stolen from OSDev)
*/ // todo: un-hardcode from port0
void start_pump(void) {
	// LOG("HE BOUTTA DO IT (starting command pump)");
	uint32_t portbit = ioread32(gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,cmd));
	// LOG("looping until device indicates it has started"); 
	while(portbit & HBA_PxCMD_CR) {
		portbit = ioread32(gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,cmd));
	}
	// LOG("device has started, setting bits...");
	portbit |= HBA_PxCMD_FRE;
	portbit |= HBA_PxCMD_ST;
	iowrite32(portbit, gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,cmd));
	// LOG("done");
}

void printBit(uint64_t pbit, uint64_t size) {
	char pStr[255]; char cur;
	uint64_t i, c;
	for( c=0, i=size-1; i>0; i--, c++ ) {
		cur = (pbit & (uint64_t)1<<i) ? '1' : '0';
		pStr[c] = cur;
	} pStr[c] = (pbit & (uint64_t)1<<i) ? '1' : '0';
	pStr[++c] = '\0';
	LOG("%s", pStr);
}

/*
	Fuzzing will be started/stopped with IOCTLs by olive.
*/ // new name: Fizzle (FIS+FUZZ)
#define ABAR 5
#define PORT 0
#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08
#define CMDSLOTS 1
void start_fuzz(void) {
	stop_pump();
	start_pump();
	// LOG("doing a fuzz");
	uint32_t q;
	get_random_bytes(&q, sizeof(q));

	uint16_t u;
	get_random_bytes(&u, sizeof(u));

	// send FIS
	// LOG("gonna send a FIS! first, clearing interrupts");
	iowrite32(-1, gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,is)); // clear int
	#define slot 0
	// fis size
	// LOG("setting FIS size, that we're reading, with 0 prdt entries");
	/* we can't use offsetof for bitfields! fuck.
	cfl:5;		// Command FIS length in DWORDS, 2 ~ 16
	a:1;		// ATAPI
	w:1;		// Write, 1: H2D, 0: D2H
	p:1;		// Prefetchable
	all make up one byte, though! so...    cfl  awp*/
		uint8_t fislen_atapi_write = 0b00000000;
		uint8_t proper_sz = sizeof(FIS_REG_H2D)/sizeof(uint32_t);
		fislen_atapi_write |= proper_sz;
		iowrite32(fislen_atapi_write, gState.cmd_header_addr);
		//iowrite32(0, cmd_header_addr + offsetof(HBA_CMD_HEADER,w)); // read
		iowrite16(u, gState.cmd_header_addr + offsetof(HBA_CMD_HEADER,prdtl)); // prdt entries

		/*memset_io(
			gState.cmd_tbl,
			0xEE,
			(sizeof(HBA_CMD_TBL)*CMDSLOTS) + (sizeof(HBA_PRDT_ENTRY)*1)
		);*/

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

		// TODO: Feedback-driven input data
		LOG("setting up FIS cmd (sz %u)", sizeof(FIS_REG_H2D));
		FIS_REG_H2D cmdfis;
		memset(&cmdfis, 0, sizeof(FIS_REG_H2D));
		get_random_bytes(&cmdfis, sizeof(FIS_REG_H2D));
		uint8_t index = 0;
		get_random_bytes(&index, sizeof(uint8_t));
		cmdfis.command = ata_cmd_vals[index % TOTCMDS];
		cmdfis.fis_type = FIS_TYPE_REG_H2D;
		cmdfis.c = 1;

		uintptr_t waddr = (gState.cmd_tbl + offsetof(HBA_CMD_TBL,cfis));
		LOG("writing FIS (%u bytes) to memory addr %#16llx", sizeof(FIS_REG_H2D), waddr);
		memcpy_toio(waddr, &cmdfis, sizeof(FIS_REG_H2D));

		LOG("waiting for port to non-busy");
		uint32_t portbit = ioread32(gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,tfd));
		uint32_t spin = 0;
		while( (portbit & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000 )  {
			portbit = ioread32(gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,cmd));
		}
		if (spin == 1000000) {
			LOG("port is hung!");
			return;
		}

		LOG("issuing FIS");
		iowrite32(1, gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,ci));
		
		// fuck with the FIS just after we issued it
		get_random_bytes(&cmdfis, sizeof(FIS_REG_H2D));
		memcpy_toio(waddr, &cmdfis, sizeof(FIS_REG_H2D));

		LOG("waiting for cmd completion"); spin=0;
		portbit = ioread32(gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,ci));
		while ( spin < 1000000 ) {
			if ((portbit & (1<<0)) == 0) 
				break;
			spin++;
		}
		// LOG("did we win?");
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
	LOG("%u slots implemented", commandSlotsImplemented); // is this right? maybe
	printBit(commandSlotsImplemented, 8);

	stop_pump();

	// bios already did it. can we just use this addr?
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

	// if we have space free in high mem, we should just be able to use it directly
	// did the system reserve it? who knows. let's just try lol.
	
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