#include "defs.h"

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

void send_random_fis(void) {
	stop_pump(); start_pump(); // this is wrong and makes it slow -> why do we need this? 
        //                            doesn't seem to function without. need to suss the proto better.
	// LOG("doing a fuzz");
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
	all make up one uint8_t byte, though! so...*/
		uint8_t fislen_atapi_write = 0b00000000;
                uint8_t random_data = 0b00000000;
		uint8_t proper_sz = sizeof(FIS_REG_H2D)/sizeof(uint32_t);
                printBit(proper_sz, 8);
                get_random_bytes(&random_data, sizeof(uint8_t)); // does this break it?
                printBit(random_data, 8);
                fislen_atapi_write |= random_data & 0b11100000; // randomise ATAPI, write and prefetchable fields
		fislen_atapi_write |=   proper_sz & 0b00011111; // set the size correctly
                printBit(fislen_atapi_write, 8);
		iowrite8(fislen_atapi_write, gState.cmd_header_addr);
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
		//get_random_bytes(&cmdfis, sizeof(FIS_REG_H2D));
		//memcpy_toio(waddr, &cmdfis, sizeof(FIS_REG_H2D));

		LOG("waiting for cmd completion"); spin=0;
		portbit = ioread32(gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,ci));
		while ( spin < 1000 ) {
			if ((portbit & (1<<0)) == 0) {
				LOG("command completed properly");
				break;
			}
			spin++;
			portbit = ioread32(gState.abar_virt_addr + offsetof(HBA_MEM,ports) + offsetof(HBA_PORT,ci));
		}
		// LOG("did we win?");
}
