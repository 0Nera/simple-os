#ifndef ARCH_I386_ATA_H
#define ARCH_I386_ATA_H

#include<stdint.h>
#include "port_io.h"

// 28 bit ATA PIO disk driver
// From http://learnitonweb.com/2020/05/22/12-developing-an-operating-system-tutorial-episode-6-ata-pio-driver-osdev/


/*
 BSY: a 1 means that the controller is busy executing a command. No register should be accessed (except the digital output register) while this bit is set.
RDY: a 1 means that the controller is ready to accept a command, and the drive is spinning at correct speed..
WFT: a 1 means that the controller detected a write fault.
SKC: a 1 means that the read/write head is in position (seek completed).
DRQ: a 1 means that the controller is expecting data (for a write) or is sending data (for a read). Don't access the data register while this bit is 0.
COR: a 1 indicates that the controller had to correct data, by using the ECC bytes (error correction code: extra bytes at the end of the sector that allows to verify its integrity and, sometimes, to correct errors).
IDX: a 1 indicates the the controller retected the index mark (which is not a hole on hard-drives).
ERR: a 1 indicates that an error occured. An error code has been placed in the error register.
*/

#define STATUS_BSY 0x80
#define STATUS_RDY 0x40
#define STATUS_DRQ 0x08
#define STATUS_DF 0x20
#define STATUS_ERR 0x01

//This is really specific to out OS now, assuming ATA bus 0 master 
//Source - OsDev wiki
static void ATA_wait_BSY();
static void ATA_wait_DRQ();
static void ATA_delay_400ns();

// 28bit LBA shall be between 0 to 0x0FFFFFFF
void read_sectors_ATA_28bit_PIO(uint16_t* target, uint32_t LBA, uint8_t sector_count)
{
	ATA_wait_BSY();
    // Send 0xE0 for the "master" or 0xF0 for the "slave", ORed with the highest 4 bits of the LBA to port 0x1F6: outb(0x1F6, 0xE0 | (slavebit << 4) | ((LBA >> 24) & 0x0F))
	outb(0x1F6,0xE0 | ((LBA >>24) & 0xF));
    // Send the sectorcount to port 0x1F2: outb(0x1F2, (unsigned char) count)
	outb(0x1F2,sector_count);
    // Send the low 8 bits of the LBA to port 0x1F3: outb(0x1F3, (unsigned char) LBA))
	outb(0x1F3, (uint8_t) LBA);
    // Send the next 8 bits of the LBA to port 0x1F4: outb(0x1F4, (unsigned char)(LBA >> 8))
	outb(0x1F4, (uint8_t)(LBA >> 8));
    // Send the next 8 bits of the LBA to port 0x1F5: outb(0x1F5, (unsigned char)(LBA >> 16))
	outb(0x1F5, (uint8_t)(LBA >> 16));
    // Send the "READ SECTORS" command (0x20) to port 0x1F7: outb(0x1F7, 0x20)
	outb(0x1F7,0x20); //Send the read command

	for (int j =0;j<sector_count;j++)
	{
        // Poll for status
        ATA_delay_400ns();
		ATA_wait_BSY();
		ATA_wait_DRQ();
         // Transfer 256 16-bit values, a uint16_t at a time, into your buffer from I/O port 0x1F0. (In assembler, REP INSW works well for this.)
		for(int i=0;i<256;i++)
			target[i] = inw(0x1F0);
		target+=256;
	}
}

// 28bit LBA shall be between 0 to 0x0FFFFFFF
void write_sectors_ATA_28bit_PIO(uint32_t LBA, uint8_t sector_count, uint16_t* words)
{
	ATA_wait_BSY();
	outb(0x1F6,0xE0 | ((LBA >>24) & 0xF));
	outb(0x1F2,sector_count);
	outb(0x1F3, (uint8_t) LBA);
	outb(0x1F4, (uint8_t)(LBA >> 8));
	outb(0x1F5, (uint8_t)(LBA >> 16));
    // To write sectors in 28 bit PIO mode, send command "WRITE SECTORS" (0x30) to the Command port
	outb(0x1F7,0x30); //Send the write command

	for (int j =0;j<sector_count;j++)
	{
		ATA_wait_BSY();
		ATA_wait_DRQ();
		for(int i=0;i<256;i++)
		{
            // Do not use REP OUTSW to transfer data. There must be a tiny delay between each OUTSW output uint16_t. A jmp $+2 size of delay
			outw(0x1F0, words[i]);
            io_wait();
		}
	}

    // Make sure to do a Cache Flush (ATA command 0xE7) after each write command completes.
    outb(0x1F7,0xE7);
}

static void ATA_delay_400ns() 
{
    inb(0x1F7);
    inb(0x1F7);
    inb(0x1F7);
    inb(0x1F7);
}

// How to poll (waiting for the drive to be ready to transfer data): 
// Read the Regular Status port until bit 7 (BSY, value = 0x80) clears, 
// and bit 3 (DRQ, value = 8) sets 
// -- or until bit 0 (ERR, value = 1) or bit 5 (DF, value = 0x20) sets. 
// If neither error bit is set, the device is ready right then.
static void ATA_wait_BSY()   //Wait for BSY to be 0
{
	while(inb(0x1F7)&STATUS_BSY);
}
static void ATA_wait_DRQ()  //Wait fot DRQ to be 1
{
	while(!(inb(0x1F7)&STATUS_RDY));
}

#endif