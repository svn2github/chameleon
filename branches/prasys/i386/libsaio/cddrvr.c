/*
 * Copyright (c) 2009 netkas. All rights reserved.
 * Redistribution and use in binary form for direct or indirect commercial purposes, with or without
 * modification, is stricktly forbidden.
 * Redistributions in binary form for non-commercial purposes must reproduce the above license notice,
 * this list of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 * Neither the names of EFI V1-V10 copyright owner nor the names of its contributors may be used
 * to endorse or promote products derived direct or indirect from this software.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "libsaio.h"
#include "mindrvr.h"

#define CDB_SIZE_T 12 
uint8_t cdb[CDB_SIZE_T];

uint32_t cd_drives[32][4];
uint32_t cd_count = 0;

uint8_t cd_bad[10] = {0,0,0,0,0,0,0,0,0,0};

uint32_t pci_count = 0;
uint32_t pci_devs[ 768 ];
uint32_t curdrive = 0;
#define CACHE_BLKN 4
unsigned char * cache_2k;
unsigned int cache_blk;

static void DriveReset(uint32_t dev)
{
	if (dev > cd_count)
	{
		printf("device %d doesnt exist, max is %d\n", dev, cd_count);
		return;
	}
	reg_reset(cd_drives[dev][2]);
}

static int SelectDrive(uint32_t dev)
{
	if(cd_bad[dev] == 1) return 1;
	if(curdrive == dev) return 0;
	if (dev > cd_count)
	{
		printf("device %d doesnt exist, max is %d\n", dev, cd_count);
		return 1;
	}
	pio_set_iobase_addr(cd_drives[dev][0], cd_drives[dev][1], cd_drives[dev][3]);
//	reg_config();
	reg_config_info[0]=0;
	reg_config_info[1]=0;
	reg_config_info[cd_drives[dev][2]]=REG_CONFIG_TYPE_ATAPI;
	curdrive = dev;
//	DriveReset(dev);
	return 0;
}

static int cdcheck (int dev, char * buffer)
{
uint32_t result, try, wait_time;

if(SelectDrive(dev))
{
	printf("Select drive returned 1, thats bad\n");
	return -1;
};


for (try=0; try<10; ++try)
  {
	memset( cdb, 0, sizeof( cdb ) );
    cdb[0] = 0xa8;
    cdb[1] = 0x00;
    cdb[2] = 0x00;
    cdb[3] = 0x00;
    cdb[4] = 0x00;
    cdb[5] = 0x00;
    cdb[6] = 0x00;
    cdb[7] = 0x00;
    cdb[8] = 0x00;
    cdb[9] = 0x01;
    cdb[10] = 0x00;
	cdb[11] = 0x00;
	result = reg_packet(cd_drives[dev][2], sizeof(cdb), (unsigned char*) cdb, 0, 2048, cache_2k);
	if(!result)
	{
		return 0;
	}
	wait_time = time18() + 20;
	while(time18() < wait_time);
  }
return -1;
}


static void AddDrive(uint32_t base1, uint32_t base2, uint32_t bmbase)
{
	uint32_t res,i;
	pio_set_iobase_addr(base1, base2, bmbase);
	res=reg_config();
	if(res>0)
	{
		printf( "AddDrive: Found %d devices, base1 is %x, base2 is %x, dev 0 is %d,  dev 1 is %d.\n",
	         res, base1, base2, 
			reg_config_info[0] ,
	      reg_config_info[1] );
		if(reg_config_info[0] == REG_CONFIG_TYPE_ATAPI) 
		{
			i=++cd_count;
			cd_drives[i][0] = base1;
			cd_drives[i][1] = base2;
			cd_drives[i][2] = 0; //master
			cd_drives[i][3] = bmbase;
			printf("added master device %d\n", i);
			reg_reset(0);
		}
		if(reg_config_info[1] == REG_CONFIG_TYPE_ATAPI) 
		{
			i=++cd_count;
			cd_drives[i][0] = base1;
			cd_drives[i][1] = base2;
			cd_drives[i][2] = 1; //slave
			cd_drives[i][3] = bmbase;	
			printf("added slave device %d\n", i);
			reg_reset(1);
		}
	}
	
}

int setup_cdread(void)
{
	int res;
	int trylegacy = 0;
	uint16_t i;
	unsigned int pcidev=0;
    unsigned int index=0;
    unsigned int vendid=0;
    unsigned int devid=0;
	unsigned int cmdBase=0;
	unsigned int ctrlBase=0;
	unsigned int classid=0;
	unsigned int bmideBase=0;
	if(cd_count>0) return cd_count;

	for(i=0; i<0x05ff;i++)
	{
		vendid = GetPciDword( i, 0x00)  & 0xffff;
		devid = (GetPciDword( i, 0x00) >>16)  & 0xffff; 
		if((vendid != 0xffff) && (vendid != 0x0000) && (devid != 0x2825) && (devid != 0x2921) && (devid != 0x2926) && (devid != 0x3a06) && (devid != 0x3a26))//  && (vendid != 0x1283) && (vendid != 0x197b))  // dont need jmicron and ite
		{	
//			devid = (GetPciDword( i, 0x00) >>16)  & 0xffff; 
			classid = (GetPciDword( i, 0x08) >>16)  & 0xffff;
			if(classid == 0x0101)
			{
				printf("device %x, vendid %x, devid %x, classid %x\n", i, vendid, devid, classid);
				cmdBase = GetPciDword(i,  0x10) & 0xfffe;
				ctrlBase = GetPciDword(i,  0x14) & 0xfffe;
				bmideBase = GetPciDword(i, 0x20) & 0xffff;
				if(cmdBase && ctrlBase)AddDrive(cmdBase, ctrlBase, bmideBase); //no need in bmidebase?
				if(cmdBase == 0 && ctrlBase == 0) trylegacy=1;
				printf("pri port cmdbase - %x, ctrlbase - %x, bmideBase - %x\n", cmdBase, ctrlBase, bmideBase);
				cmdBase = GetPciDword(i,  0x10+8) & 0xfffe;
				ctrlBase = GetPciDword(i,  0x14+8) & 0xfffe;
				if(cmdBase && ctrlBase)AddDrive(cmdBase, ctrlBase, bmideBase);
				printf("sec port cmdbase - %x, ctrlbase - %x, bmideBase - %x\n", cmdBase, ctrlBase, bmideBase);
			}
		}
	}
	if(trylegacy)
	{
			AddDrive(0x1f0, 0x3f4, 0x00);
			AddDrive(0x170, 0x374, 0x00);
	}
	if(cd_count == 0) return 0;
//	AddDrive(0xa000, 0x9c00, 0x00); //ich9r pri
//	AddDrive(0x9880, 0x9800, 0x00); //ich9r sec
//	AddDrive(0xd600, 0xd700, 0x00);
//	AddDrive(0xd800, 0xd900, 0x00);
	cache_2k = malloc(2048*CACHE_BLKN);
	cache_blk = 0xFFFF0000;
	for(i=1; i<=cd_count;i++)
		if(cdcheck(i, cache_2k))
		{
			printf("no inserted cd found in drive %d\n",i);
			cd_bad[i]=1;
		}
	printf("cd setup done\n");
	return cd_count;
}

int cdread (int dev, unsigned int secno, char * buffer)
{
uint32_t result, try, wait_time, olddrive;

olddrive=curdrive; //saving curdrive until its overwrited in SelectDrive

if(SelectDrive(dev))
{
	printf("Select drive returned 1, thats bad\n");
	return -1;
};

if(secno >= cache_blk && secno < (cache_blk + CACHE_BLKN) && olddrive == dev)
{
	bcopy(cache_2k + (secno - cache_blk) * 2048, buffer, 2048);
	return 0;
}
for (try=0; try<10; ++try)
  {
	memset( cdb, 0, sizeof( cdb ) );
	cdb[0] = 0xa8;
	cdb[1] = 0x00;
	cdb[2] = (OSSwapBigToHostInt32(secno) & 0xFF);  //4
	cdb[3] = (OSSwapBigToHostInt32(secno) & 0xFF00) >> 8; //3
	cdb[4] = (OSSwapBigToHostInt32(secno) & 0xFF0000) >> 16; //2
	cdb[5] = (OSSwapBigToHostInt32(secno) & 0xFF000000) >> 24; //1
	cdb[6] = 0x00;
	cdb[7] = 0x00;
	cdb[8] = 0x00;
	cdb[9] = CACHE_BLKN;
	cdb[10] = 0x00;
	cdb[11] = 0x00;

	result = reg_packet(cd_drives[dev][2], sizeof(cdb), (unsigned char*) cdb, 0, 2048*CACHE_BLKN, cache_2k);
	if(!result)
	{
		bcopy(cache_2k, buffer, 2048);
		cache_blk=secno;
		return 0;
	}
	//	DriveReset(dev);
	wait_time = time18() + 50;
	while(time18() < wait_time);
  }
return -1;
}

void ejectcd(uint32_t dev, uint32_t dir)
{
	SelectDrive(dev);
	memset( cdb, 0, sizeof( cdb ) );

	cdb[0] = 0x1b;
	cdb[1] = 0x00;
	cdb[2] = 0x00;
	cdb[3] = 0x00;
	cdb[4] = dir ? 2 :3;
	cdb[5] = 0x00;
	cdb[6] = 0x00;
	cdb[7] = 0x00;
	cdb[8] = 0x00;
	cdb[9] = 0x00;
	cdb[10] = 0x00;
	cdb[11] = 0x00;

	reg_packet(cd_drives[dev][2], sizeof(cdb), (unsigned char*) cdb, 0, 2048,	kLoadAddr);
//	DriveReset(dev);
}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  