/*
 * Copyright 2009 netkas
 */

#include "libsaio.h"
#include "boot.h"
#include "bootstruct.h"

#ifndef DEBUG_PCIROOT
#define DEBUG_PCIROOT 0
#endif

#if DEBUG_PCIROOT
#define DBG(x...)  printf(x)
#else
#define DBG(x...)  msglog(x)
#endif

static int rootuid = 10; //value means function wasn't ran yet

static unsigned int findrootuid(unsigned char * dsdt, int len)
{
	int i;
	for (i=0; i<64 && i<len-5; i++) //not far than 64 symbols from pci root 
	{
		if(dsdt[i] == '_' && dsdt[i+1] == 'U' && dsdt[i+2] == 'I' && dsdt[i+3] == 'D' && dsdt[i+5] == 0x08)
		{
			return dsdt[i+4];
		}
	}
	return 11;
}

static unsigned int findpciroot(unsigned char * dsdt,int len)
{
	int i;

	for (i=0; i<len-4; i++) {
		if(dsdt[i] == 'P' && dsdt[i+1] == 'C' && dsdt[i+2] == 'I' && (dsdt[i+3] == 0x08 || dsdt [i+4] == 0x08)) {
			return findrootuid(dsdt+i, len-i);
		}
	}
	return 10;
}

int getPciRootUID(void)
{
	char dsdt_dirSpec[128];
	void *new_dsdt = NULL;
	const char *val = "";
	int len = 0,fsize = 0;
	const char * dsdt_filename = "";
	extern int search_and_get_acpi_fd(const char *, const char **);

	if (rootuid < 10) return rootuid;
	rootuid = 0;	/* default _UID = 0 */

	if (getValueForKey(kPCIRootUID, &val, &len, &bootInfo->chameleonConfig) && len) {
		if (isdigit(val[0])) rootuid = val[0] - '0';
		goto out;
	}
	/* Chameleon compatibility */
	else if (getValueForKey("PciRoot", &val, &len, &bootInfo->chameleonConfig) && len) {
		if (isdigit(val[0])) rootuid = val[0] - '0';
		goto out;
	}
	/* PCEFI compatibility */
	else if (getValueForKey("-pci0", &val, &len, &bootInfo->chameleonConfig) && len) {
		rootuid = 0;
		goto out;
	}
	else if (getValueForKey("-pci1", &val, &len, &bootInfo->chameleonConfig) && len) {
		rootuid = 1;
		goto out;
	}

	// Try using the file specified with the DSDT option
	if (getValueForKey(kDSDT, &dsdt_filename, &len, &bootInfo->chameleonConfig) && len)
	{
		snprintf(dsdt_dirSpec, sizeof(dsdt_dirSpec), dsdt_filename);
	}
	else
	{
		sprintf(dsdt_dirSpec, "DSDT.aml");
	}
	
    DBG("PCIrootUID: trying DSDT.aml... ");
	int fd = search_and_get_acpi_fd(dsdt_dirSpec, &dsdt_filename);

	// Check booting partition
	if (fd<0)
	{	  
	  DBG("PCIrootUID: file '%s' not found, using default value.\n", dsdt_filename);
	  rootuid = 0;
	  goto out;
	}
	
	fsize = file_size(fd);

	if (!(new_dsdt = malloc(fsize))) {
		DBG("PCIrootUID: ERROR: allocating DSDT memory failed.\n");
		close (fd);
		goto out;
	}
    
	if (read (fd, new_dsdt, fsize) != fsize) {
		DBG("PCIrootUID: ERROR: failed reading DSDT from '%s'.\n", dsdt_filename);
		free(new_dsdt);
		close (fd);
		goto out;
	}
    
	close (fd);

	rootuid = findpciroot(new_dsdt, fsize);
	free(new_dsdt);

	// make sure it really works: 
	if (rootuid == 11) rootuid=0; //usually when _UID isnt present, it means uid is zero
	else if (rootuid < 0 || rootuid > 9) 
	{
		DBG("PCIrootUID: proper value wasn't found. Using default value (0). Use -PciRootUID flag to force.\n");
		rootuid = 0;
        return rootuid;
	}
    
out:
    DBG("PCIrootUID=0x%02X: using.\n", rootuid);
	return rootuid;
}
