
/*
 * Copyright 2007 David F. Elliott.	 All rights reserved.
 */
//#include "saio_types.h"
//#include "libsaio.h"
#include "boot.h"
#include "bootstruct.h"
#include "efi.h"
#include "acpi.h"
#include "fake_efi.h"
#include "efi_tables.h"
#include "platform.h"
#include "acpi_patcher.h"
#include "smbios.h"
#include "device_inject.h"
#include "convert.h"
#include "pci.h"
#include "sl.h"
#include "failedboot.h"
#include "appleClut8.h"
//#include <mach-o/fat.h>
//#include <mach-o/loader.h>

#if DEBUG
#define DBG(x...)	printf(x)
#else
#define DBG(x...)	msglog(x)
#endif

extern void setup_pci_devs(pci_dt_t *pci_dt);

/*
 * Modern Darwin kernels require some amount of EFI because Apple machines all
 * have EFI. Modifying the kernel source to not require EFI is of course
 * possible but would have to be maintained as a separate patch because it is
 * unlikely that Apple wishes to add legacy support to their kernel.
 *
 * As you can see from the Apple-supplied code in bootstruct.c, it seems that
 * the intention was clearly to modify this booter to provide EFI-like structures
 * to the kernel rather than modifying the kernel to handle non-EFI stuff. This
 * makes a lot of sense from an engineering point of view as it means the kernel
 * for the as yet unreleased EFI-only Macs could still be booted by the non-EFI
 * DTK systems so long as the kernel checked to ensure the boot tables were
 * filled in appropriately.	Modern xnu requires a system table and a runtime
 * services table and performs no checks whatsoever to ensure the pointers to
 * these tables are non-NULL. Therefore, any modern xnu kernel will page fault
 * early on in the boot process if the system table pointer is zero.
 *
 * Even before that happens, the tsc_init function in modern xnu requires the FSB
 * Frequency to be a property in the /efi/platform node of the device tree or else
 * it panics the bootstrap process very early on.
 *
 * As of this writing, the current implementation found here is good enough
 * to make the currently available xnu kernel boot without modification on a
 * system with an appropriate processor. With a minor source modification to
 * the tsc_init function to remove the explicit check for Core or Core 2
 * processors the kernel can be made to boot on other processors so long as
 * the code can be executed by the processor and the machine contains the
 * necessary hardware.
 */

/*==========================================================================
 * Utility function to make a device tree string from an EFI_GUID
 */
static inline char * mallocStringForGuid(EFI_GUID const *pGuid)
{
	char *string = malloc(37);
	efi_guid_unparse_upper(pGuid, string);
	return string;
}

/*==========================================================================
 * Function to map 32 bit physical address to 64 bit virtual address
 */
static uint64_t ptov64(uint32_t addr)
{
	return ((uint64_t)addr | 0xFFFFFF8000000000ULL);
}

// ==========================================================================
// ErmaC
static inline uint64_t getCPUTick(void)
{
	uint32_t lowest;
	uint32_t highest;
	__asm__ volatile ("rdtsc" : "=a" (lowest), "=d" (highest));
	return (uint64_t) highest << 32 | lowest;
}

/*==========================================================================
 * Fake EFI implementation
 */

/* Identify ourselves as the EFI firmware vendor */
static EFI_CHAR16 const FIRMWARE_VENDOR[] = {'C','h','a','m','e','l','e','o','n','_','2','.','3', 0};
// Bungo
//static EFI_UINT32 const FIRMWARE_REVISION = 132; /* FIXME: Find a constant for this. */
static EFI_UINT32 const FIRMWARE_REVISION = 0x0001000a; // got from real MBP6,1, most Macs use it too
// Bungo
/* Default platform system_id (fix by IntVar)
 static EFI_CHAR8 const SYSTEM_ID[] = "0123456789ABCDEF"; //random value gen by uuidgen
 */

/* Just a ret instruction */
static uint8_t const VOIDRET_INSTRUCTIONS[] = {0xc3};

/* movl $0x80000003,%eax; ret */
static uint8_t const UNSUPPORTEDRET_INSTRUCTIONS_32[] = {0xb8, 0x03, 0x00, 0x00, 0x80, 0xc3};
static uint8_t const UNSUPPORTEDRET_INSTRUCTIONS_64[] = {0x48, 0xb8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc3};

/* EFI Guides */
EFI_GUID const gEfiSmbiosTableGuid = EFI_SMBIOS_TABLE_GUID;
EFI_GUID const gEfiAcpiTableGuid = EFI_ACPI_TABLE_GUID;
EFI_GUID const gEfiAcpi20TableGuid = EFI_ACPI_20_TABLE_GUID;

EFI_SYSTEM_TABLE_32 *gST32 = NULL;
EFI_SYSTEM_TABLE_64 *gST64 = NULL;
Node *gEfiConfigurationTableNode = NULL;

EFI_STATUS addConfigurationTable(EFI_GUID const *pGuid, void *table, char const *alias)
{
	EFI_UINTN i = 0;

	//Azi: as is, cpu's with em64t will use EFI64 on pre 10.6 systems,
	// wich seems to cause no problem. In case it does, force i386 arch.
	if (archCpuType == CPU_TYPE_I386)
	{
		i = gST32->NumberOfTableEntries;
	}
	else
	{
		i = gST64->NumberOfTableEntries;
	}

	// We only do adds, not modifications and deletes like InstallConfigurationTable
	if (i >= MAX_CONFIGURATION_TABLE_ENTRIES)
	{
		stop("Fake EFI [ERROR]: Ran out of space for configuration tables [%d]. Increase the reserved size in the code.\n", i);
	}

	if (pGuid == NULL)
	{
		return EFI_INVALID_PARAMETER;
	}

	if (table != NULL)
	{
		// FIXME
		//((EFI_CONFIGURATION_TABLE_64 *)gST->ConfigurationTable)[i].VendorGuid = *pGuid;
		//((EFI_CONFIGURATION_TABLE_64 *)gST->ConfigurationTable)[i].VendorTable = (EFI_PTR64)table;

		//++gST->NumberOfTableEntries;

		Node *tableNode = DT__AddChild(gEfiConfigurationTableNode, mallocStringForGuid(pGuid));

		// Use the pointer to the GUID we just stuffed into the system table
		DT__AddProperty(tableNode, "guid", sizeof(EFI_GUID), (void *)pGuid);

		// The "table" property is the 32-bit (in our implementation) physical address of the table
		DT__AddProperty(tableNode, "table", sizeof(void*) * 2, table);

		// Assume the alias pointer is a global or static piece of data
		if (alias != NULL)
		{
			DT__AddProperty(tableNode, "alias", strlen(alias)+1, (char *)alias);
		}
        
        i++;
        
        if (archCpuType == CPU_TYPE_I386)
        {
            gST32->NumberOfTableEntries = i;
        }
        else
        {
            gST64->NumberOfTableEntries = i;
        }

		return EFI_SUCCESS;
	}
	return EFI_UNSUPPORTED;
}

// ==========================================================================

//Azi: crc32 done in place, on the cases were it wasn't.
/*static inline void fixupEfiSystemTableCRC32(EFI_SYSTEM_TABLE_64 *efiSystemTable)
{
	efiSystemTable->Hdr.CRC32 = 0;
	efiSystemTable->Hdr.CRC32 = crc32(0L, efiSystemTable, efiSystemTable->Hdr.HeaderSize);
}*/

/*
 * What we do here is simply allocate a fake EFI system table and a fake EFI
 * runtime services table.
 *
 * Because we build against modern headers with kBootArgsRevision 4 we
 * also take care to set efiMode = 32.
 */
void setupEfiTables32(void)
{
	// We use the fake_efi_pages struct so that we only need to do one kernel
	// memory allocation for all needed EFI data.  Otherwise, small allocations
	// like the FIRMWARE_VENDOR string would take up an entire page.
	// NOTE WELL: Do NOT assume this struct has any particular layout within itself.
	// It is absolutely not intended to be publicly exposed anywhere
	// We say pages (plural) although right now we are well within the 1 page size
	// and probably will stay that way.
	struct fake_efi_pages
	{
		EFI_SYSTEM_TABLE_32 efiSystemTable;
		EFI_RUNTIME_SERVICES_32 efiRuntimeServices;
		EFI_CONFIGURATION_TABLE_32 efiConfigurationTable[MAX_CONFIGURATION_TABLE_ENTRIES];
		EFI_CHAR16 firmwareVendor[sizeof(FIRMWARE_VENDOR)/sizeof(EFI_CHAR16)];
		uint8_t voidret_instructions[sizeof(VOIDRET_INSTRUCTIONS)/sizeof(uint8_t)];
		uint8_t unsupportedret_instructions[sizeof(UNSUPPORTEDRET_INSTRUCTIONS_32)/sizeof(uint8_t)];
	};
	
	struct fake_efi_pages *fakeEfiPages = (struct fake_efi_pages*)AllocateKernelMemory(sizeof(struct fake_efi_pages));
	
	// Zero out all the tables in case fields are added later
	//bzero(fakeEfiPages, sizeof(struct fake_efi_pages));
	
	// --------------------------------------------------------------------
	// Initialize some machine code that will return EFI_UNSUPPORTED for
	// functions returning int and simply return for void functions.
	memcpy(fakeEfiPages->voidret_instructions, VOIDRET_INSTRUCTIONS, sizeof(VOIDRET_INSTRUCTIONS));
	memcpy(fakeEfiPages->unsupportedret_instructions, UNSUPPORTEDRET_INSTRUCTIONS_32, sizeof(UNSUPPORTEDRET_INSTRUCTIONS_32));
	
	// --------------------------------------------------------------------
	// System table
	EFI_SYSTEM_TABLE_32 *efiSystemTable = gST32 = &fakeEfiPages->efiSystemTable;
	efiSystemTable->Hdr.Signature = EFI_SYSTEM_TABLE_SIGNATURE;
	efiSystemTable->Hdr.Revision = EFI_SYSTEM_TABLE_REVISION;
	efiSystemTable->Hdr.HeaderSize = sizeof(EFI_SYSTEM_TABLE_32);
	efiSystemTable->Hdr.CRC32 = 0; // Initialize to zero and then do CRC32
	efiSystemTable->Hdr.Reserved = 0;
	
	efiSystemTable->FirmwareVendor = (EFI_PTR32)&fakeEfiPages->firmwareVendor;
	memcpy(fakeEfiPages->firmwareVendor, FIRMWARE_VENDOR, sizeof(FIRMWARE_VENDOR));
	efiSystemTable->FirmwareRevision = FIRMWARE_REVISION;
	
	// XXX: We may need to have basic implementations of ConIn/ConOut/StdErr
	// The EFI spec states that all handles are invalid after boot services have been
	// exited so we can probably get by with leaving the handles as zero.
	efiSystemTable->ConsoleInHandle = 0;
	efiSystemTable->ConIn = 0;

	efiSystemTable->ConsoleOutHandle = 0;
	efiSystemTable->ConOut = 0;

	efiSystemTable->StandardErrorHandle = 0;
	efiSystemTable->StdErr = 0;

	efiSystemTable->RuntimeServices = (EFI_PTR32)&fakeEfiPages->efiRuntimeServices;

	// According to the EFI spec, BootServices aren't valid after the
	// boot process is exited so we can probably do without it.
	// Apple didn't provide a definition for it in pexpert/i386/efi.h
	// so I'm guessing they don't use it.
	efiSystemTable->BootServices = 0;

	efiSystemTable->NumberOfTableEntries = 0;
	efiSystemTable->ConfigurationTable = (EFI_PTR32)fakeEfiPages->efiConfigurationTable;

	// We're done. Now CRC32 the thing so the kernel will accept it.
	// Must be initialized to zero before CRC32, done above.
	gST32->Hdr.CRC32 = crc32(0L, gST32, gST32->Hdr.HeaderSize);

	// --------------------------------------------------------------------
	// Runtime services
	EFI_RUNTIME_SERVICES_32 *efiRuntimeServices = &fakeEfiPages->efiRuntimeServices;
	efiRuntimeServices->Hdr.Signature = EFI_RUNTIME_SERVICES_SIGNATURE;
	efiRuntimeServices->Hdr.Revision = EFI_RUNTIME_SERVICES_REVISION;
	efiRuntimeServices->Hdr.HeaderSize = sizeof(EFI_RUNTIME_SERVICES_32);
	efiRuntimeServices->Hdr.CRC32 = 0;
	efiRuntimeServices->Hdr.Reserved = 0;

	// There are a number of function pointers in the efiRuntimeServices table.
	// These are the Foundation (e.g. core) services and are expected to be present on
	// all EFI-compliant machines.	Some kernel extensions (notably AppleEFIRuntime)
	// will call these without checking to see if they are null.
	//
	// We don't really feel like doing an EFI implementation in the bootloader
	// but it is nice if we can at least prevent a complete crash by
	// at least providing some sort of implementation until one can be provided
	// nicely in a kext.
	void (*voidret_fp)() = (void*)fakeEfiPages->voidret_instructions;
	void (*unsupportedret_fp)() = (void*)fakeEfiPages->unsupportedret_instructions;
	efiRuntimeServices->GetTime = (EFI_PTR32)unsupportedret_fp;
	efiRuntimeServices->SetTime = (EFI_PTR32)unsupportedret_fp;
	efiRuntimeServices->GetWakeupTime = (EFI_PTR32)unsupportedret_fp;
	efiRuntimeServices->SetWakeupTime = (EFI_PTR32)unsupportedret_fp;
	efiRuntimeServices->SetVirtualAddressMap = (EFI_PTR32)unsupportedret_fp;
	efiRuntimeServices->ConvertPointer = (EFI_PTR32)unsupportedret_fp;
	efiRuntimeServices->GetVariable = (EFI_PTR32)unsupportedret_fp;
	efiRuntimeServices->GetNextVariableName = (EFI_PTR32)unsupportedret_fp;
	efiRuntimeServices->SetVariable = (EFI_PTR32)unsupportedret_fp;
	efiRuntimeServices->GetNextHighMonotonicCount = (EFI_PTR32)unsupportedret_fp;
	efiRuntimeServices->ResetSystem = (EFI_PTR32)voidret_fp;

	// We're done.	Now CRC32 the thing so the kernel will accept it
	efiRuntimeServices->Hdr.CRC32 = crc32(0L, efiRuntimeServices, efiRuntimeServices->Hdr.HeaderSize);

	// --------------------------------------------------------------------
	// Finish filling in the rest of the boot args that we need.
	bootArgs->efiSystemTable = (uint32_t)efiSystemTable;
	bootArgs->efiMode = kBootArgsEfiMode32;

	// The bootArgs structure as a whole is bzero'd so we don't need to fill in
	// things like efiRuntimeServices* and what not.
	//
	// In fact, the only code that seems to use that is the hibernate code so it
	// knows not to save the pages.	 It even checks to make sure its nonzero.
}

void setupEfiTables64(void)
{
	struct fake_efi_pages
	{
		EFI_SYSTEM_TABLE_64 efiSystemTable;
		EFI_RUNTIME_SERVICES_64 efiRuntimeServices;
		EFI_CONFIGURATION_TABLE_64 efiConfigurationTable[MAX_CONFIGURATION_TABLE_ENTRIES];
		EFI_CHAR16 firmwareVendor[sizeof(FIRMWARE_VENDOR)/sizeof(EFI_CHAR16)];
		uint8_t voidret_instructions[sizeof(VOIDRET_INSTRUCTIONS)/sizeof(uint8_t)];
		uint8_t unsupportedret_instructions[sizeof(UNSUPPORTEDRET_INSTRUCTIONS_64)/sizeof(uint8_t)];
	};

	struct fake_efi_pages *fakeEfiPages = (struct fake_efi_pages*)AllocateKernelMemory(sizeof(struct fake_efi_pages));

	// Zero out all the tables in case fields are added later
	//bzero(fakeEfiPages, sizeof(struct fake_efi_pages));

	// --------------------------------------------------------------------
	// Initialize some machine code that will return EFI_UNSUPPORTED for
	// functions returning int and simply return for void functions.
	memcpy(fakeEfiPages->voidret_instructions, VOIDRET_INSTRUCTIONS, sizeof(VOIDRET_INSTRUCTIONS));
	memcpy(fakeEfiPages->unsupportedret_instructions, UNSUPPORTEDRET_INSTRUCTIONS_64, sizeof(UNSUPPORTEDRET_INSTRUCTIONS_64));

	// --------------------------------------------------------------------
	// System table
	EFI_SYSTEM_TABLE_64 *efiSystemTable = gST64 = &fakeEfiPages->efiSystemTable;
	efiSystemTable->Hdr.Signature = EFI_SYSTEM_TABLE_SIGNATURE;
	efiSystemTable->Hdr.Revision = EFI_SYSTEM_TABLE_REVISION;
	efiSystemTable->Hdr.HeaderSize = sizeof(EFI_SYSTEM_TABLE_64);
	efiSystemTable->Hdr.CRC32 = 0; // Initialize to zero and then do CRC32
	efiSystemTable->Hdr.Reserved = 0;

	efiSystemTable->FirmwareVendor = ptov64((EFI_PTR32)&fakeEfiPages->firmwareVendor);
	memcpy(fakeEfiPages->firmwareVendor, FIRMWARE_VENDOR, sizeof(FIRMWARE_VENDOR));
	efiSystemTable->FirmwareRevision = FIRMWARE_REVISION;

	// XXX: We may need to have basic implementations of ConIn/ConOut/StdErr
	// The EFI spec states that all handles are invalid after boot services have been
	// exited so we can probably get by with leaving the handles as zero.
	efiSystemTable->ConsoleInHandle = 0;
	efiSystemTable->ConIn = 0;

	efiSystemTable->ConsoleOutHandle = 0;
	efiSystemTable->ConOut = 0;

	efiSystemTable->StandardErrorHandle = 0;
	efiSystemTable->StdErr = 0;

	efiSystemTable->RuntimeServices = ptov64((EFI_PTR32)&fakeEfiPages->efiRuntimeServices);
	// According to the EFI spec, BootServices aren't valid after the
	// boot process is exited so we can probably do without it.
	// Apple didn't provide a definition for it in pexpert/i386/efi.h
	// so I'm guessing they don't use it.
	efiSystemTable->BootServices = 0;

	efiSystemTable->NumberOfTableEntries = 0;
	efiSystemTable->ConfigurationTable = ptov64((EFI_PTR32)fakeEfiPages->efiConfigurationTable);

	// We're done.	Now CRC32 the thing so the kernel will accept it
	gST64->Hdr.CRC32 = crc32(0L, gST64, gST64->Hdr.HeaderSize);

	// --------------------------------------------------------------------
	// Runtime services
	EFI_RUNTIME_SERVICES_64 *efiRuntimeServices = &fakeEfiPages->efiRuntimeServices;
	efiRuntimeServices->Hdr.Signature = EFI_RUNTIME_SERVICES_SIGNATURE;
	efiRuntimeServices->Hdr.Revision = EFI_RUNTIME_SERVICES_REVISION;
	efiRuntimeServices->Hdr.HeaderSize = sizeof(EFI_RUNTIME_SERVICES_64);
	efiRuntimeServices->Hdr.CRC32 = 0;
	efiRuntimeServices->Hdr.Reserved = 0;

	// There are a number of function pointers in the efiRuntimeServices table.
	// These are the Foundation (e.g. core) services and are expected to be present on
	// all EFI-compliant machines.	Some kernel extensions (notably AppleEFIRuntime)
	// will call these without checking to see if they are null.
	//
	// We don't really feel like doing an EFI implementation in the bootloader
	// but it is nice if we can at least prevent a complete crash by
	// at least providing some sort of implementation until one can be provided
	// nicely in a kext.

	void (*voidret_fp)() = (void*)fakeEfiPages->voidret_instructions;
	void (*unsupportedret_fp)() = (void*)fakeEfiPages->unsupportedret_instructions;
	efiRuntimeServices->GetTime = ptov64((EFI_PTR32)unsupportedret_fp);
	efiRuntimeServices->SetTime = ptov64((EFI_PTR32)unsupportedret_fp);
	efiRuntimeServices->GetWakeupTime = ptov64((EFI_PTR32)unsupportedret_fp);
	efiRuntimeServices->SetWakeupTime = ptov64((EFI_PTR32)unsupportedret_fp);
	efiRuntimeServices->SetVirtualAddressMap = ptov64((EFI_PTR32)unsupportedret_fp);
	efiRuntimeServices->ConvertPointer = ptov64((EFI_PTR32)unsupportedret_fp);
	efiRuntimeServices->GetVariable = ptov64((EFI_PTR32)unsupportedret_fp);
	efiRuntimeServices->GetNextVariableName = ptov64((EFI_PTR32)unsupportedret_fp);
	efiRuntimeServices->SetVariable = ptov64((EFI_PTR32)unsupportedret_fp);
	efiRuntimeServices->GetNextHighMonotonicCount = ptov64((EFI_PTR32)unsupportedret_fp);
	efiRuntimeServices->ResetSystem = ptov64((EFI_PTR32)voidret_fp);

	// We're done.	Now CRC32 the thing so the kernel will accept it
	efiRuntimeServices->Hdr.CRC32 = crc32(0L, efiRuntimeServices, efiRuntimeServices->Hdr.HeaderSize);

	// --------------------------------------------------------------------
	// Finish filling in the rest of the boot args that we need.
	bootArgs->efiSystemTable = (uint32_t)efiSystemTable;
	bootArgs->efiMode = kBootArgsEfiMode64;

	// The bootArgs structure as a whole is bzero'd so we don't need to fill in
	// things like efiRuntimeServices* and what not.
	//
	// In fact, the only code that seems to use that is the hibernate code so it
	// knows not to save the pages.	 It even checks to make sure its nonzero.
}

/*
 * In addition to the EFI tables there is also the EFI device tree node.
 * In particular, we need /efi/platform to have an FSBFrequency key. Without it,
 * the tsc_init function will panic very early on in kernel startup, before
 * the console is available.
 */

/*==========================================================================
 * FSB Frequency detection
 */

/* These should be const but DT__AddProperty takes char* */
static const char TSC_Frequency_prop[] = "TSCFrequency";
static const char FSB_Frequency_prop[] = "FSBFrequency";
static const char CPU_Frequency_prop[] = "CPUFrequency";

/*==========================================================================
 * Fake EFI implementation
 */

/* These should be const but DT__AddProperty takes char* */
static const char FIRMWARE_REVISION_PROP[] = "firmware-revision";
static const char FIRMWARE_ABI_PROP[] = "firmware-abi";
static const char FIRMWARE_VENDOR_PROP[] = "firmware-vendor";
//static const char FIRMWARE_ABI_32_PROP_VALUE[] = "EFI32";
static const char FIRMWARE_ABI_64_PROP_VALUE[] = "EFI64";
static const char EFI_MODE_PROP[] = "efi-mode";  //Bungo
static const char SYSTEM_ID_PROP[] = "system-id";
static const char SYSTEM_SERIAL_PROP[] = "SystemSerialNumber";
static const char SYSTEM_TYPE_PROP[] = "system-type";
static const char MODEL_PROP[] = "Model";
static const char BOARDID_PROP[] = "board-id";
static const char DEV_PATHS_SUP_PROP[] = "DevicePathsSupported";
static const char MACHINE_SIG_PROP[] = "machine-signature";
static EFI_UINT32 const ZERO_U32 = 0;
static EFI_UINT32 const ONE_U32 = 1;
static const char RANDOM_SEED_PROP[] = "random-seed";
/*
static EFI_UINT8 const RANDOM_SEED_PROP_VALUE[] =
{
    0x40, 0x00, 0x50, 0x00, 0x5c, 0x00, 0x53, 0x00, 0x79, 0x00, 0x73, 0x00, 0x74, 0x00, 0x65, 0x00,
    0x6d, 0x00, 0x5c, 0x00, 0x4c, 0x00, 0x69, 0x00, 0x62, 0x00, 0x72, 0x00, 0x61, 0x00, 0x72, 0x00,
    0x79, 0x00, 0x5c, 0x00, 0x43, 0x00, 0x6f, 0x00, 0x72, 0x00, 0x65, 0x00, 0x53, 0x00, 0x65, 0x00,
    0x72, 0x00, 0x76, 0x00, 0x69, 0x00, 0x63, 0x00, 0x65, 0x00, 0x73, 0x00, 0x5c, 0x00, 0x62, 0x00
}; */

/*
 * Get an smbios option string option to convert to EFI_CHAR16 string
 */
static EFI_CHAR16* getSmbiosChar16(const char * key, size_t* len)
{
	const char	*src = getStringForKey(key, &bootInfo->smbiosConfig);
	EFI_CHAR16*	 dst = 0;
	size_t		 i = 0;

	if (!key || !(*key) || !len || !src)
	{
		return 0;
	}
	
	*len = strlen(src);
	dst = (EFI_CHAR16*) malloc( ((*len)+1) * 2 );
	for (; i < (*len); i++)
	{
		dst[i] = src[i];
	}
	dst[(*len)] = '\0';
	*len = ((*len)+1)*2; // return the CHAR16 bufsize including zero terminated CHAR16
	return dst;
}

// Bungo
/*
 * Get the SystemID from the bios dmi info

static	EFI_CHAR8* getSmbiosUUID()
{
	static EFI_CHAR8		 uuid[UUID_LEN];
	int						 i, isZero, isOnes;
	SMBByte					*p;

	p = (SMBByte*)Platform.UUID;

	for (i=0, isZero=1, isOnes=1; i<UUID_LEN; i++)
	{
		if (p[i] != 0x00)
		{
			isZero = 0;
		}

		if (p[i] != 0xff)
		{
			isOnes = 0;
		}
	}

	if (isZero || isOnes) // empty or setable means: no uuid present
	{
		verbose("No UUID present in SMBIOS System Information Table\n");
		return 0;
	}

	memcpy(uuid, p, UUID_LEN);
	return uuid;
}


// return a binary UUID value from the overriden SystemID and SMUUID if found, 
// or from the bios if not, or from a fixed value if no bios value is found 

static EFI_CHAR8* getSystemID()
{
	// unable to determine UUID for host. Error: 35 fix
	// Rek: new SMsystemid option conforming to smbios notation standards, this option should
	// belong to smbios config only ...
	const char *sysId = getStringForKey(kSystemID, &bootInfo->chameleonConfig);
	EFI_CHAR8*	ret = getUUIDFromString(sysId);

	if (!sysId || !ret) // try bios dmi info UUID extraction
	{
		ret = getSmbiosUUID();
		sysId = 0;
	}

	if (!ret)
	{
		// no bios dmi UUID available, set a fixed value for system-id
		ret=getUUIDFromString((sysId = (const char*) SYSTEM_ID));
	}
	verbose("Customizing SystemID with : %s\n", getStringFromUUID(ret)); // apply a nice formatting to the displayed output
	return ret;
}
 */

/*
 * Must be called AFTER setupAcpi because we need to take care of correct
 * FACP content to reflect in ioregs
 */
void setupSystemType()
{
	Node *node = DT__FindNode("/", false);
	if (node == 0)
	{
		stop("Couldn't get root '/' node");
	}
	// we need to write this property after facp parsing
	// Export system-type only if it has been overrriden by the SystemType option
	DT__AddProperty(node, SYSTEM_TYPE_PROP, sizeof(Platform.Type), &Platform.Type);
}

bool getKernelCompat(EFI_UINT8 *compat)
{
    int kernelFileRef;
    EFI_UINT8 readBytes;
    char kernelFilePath[512], kernelHeaderBuf[sizeof(struct fat_header) + 4*sizeof(struct fat_arch)];
    
    snprintf(kernelFilePath, sizeof(kernelFilePath), "%s", bootInfo->bootFile);
    //strlcpy(kernelFilePath, bootInfo->bootFile, sizeof(kernelFilePath)); // user defined path
    DBG("EfiKernelCompat: trying to read kernel header from file '%s'...\n", kernelFilePath);
    if ((kernelFileRef = open(kernelFilePath, 0)) >= 0) {
    } else {
        snprintf(kernelFilePath, sizeof(kernelFilePath), "/%s", bootInfo->bootFile); // append a leading '/'
        DBG("EfiKernelCompat: trying to read kernel header from file '%s'...\n", kernelFilePath);
        if ((kernelFileRef = open(kernelFilePath, 0)) >= 0) {
        } else {
            snprintf(kernelFilePath, sizeof(kernelFilePath), "/System/Library/Kernels/%s", bootInfo->bootFile); // Yosemite path
            DBG("EfiKernelCompat: trying to read kernel header from file '%s'...\n", kernelFilePath);
            if ((kernelFileRef = open(kernelFilePath, 0)) >= 0) {
            } else {
                DBG("EfiKernelCompat: can't find any kernel file!\n");
                return false;
            }
        }
    }
    
    if ((readBytes = read(kernelFileRef, kernelHeaderBuf, sizeof(struct fat_header) + 4*sizeof(struct fat_arch))) > 0) {
        DBG("EfiKernelCompat: OK, read %d bytes from file '%s'.\n", readBytes, kernelFilePath);
    } else {
        DBG("EfiKernelCompat: can't read kernel file '%s'!\n", kernelFilePath);
        return false;
    }
    
    struct fat_header  *fatHeaderPtr = (struct fat_header *)kernelHeaderBuf;
	struct fat_arch    *fatArchPtr = (struct fat_arch *)(kernelHeaderBuf + sizeof(struct fat_header));
    struct mach_header *thinHeaderPtr = (struct mach_header *)kernelHeaderBuf;
    bool swapit = false;
    
    switch (fatHeaderPtr->magic) {
        case FAT_CIGAM:
            swapit = true;
            fatHeaderPtr->nfat_arch = OSSwapInt32(fatHeaderPtr->nfat_arch);
        case FAT_MAGIC:
            *compat = 0;
            DBG("EfiKernelCompat: kernel file is a fat binary: %d archs compatibility [", fatHeaderPtr->nfat_arch);
            if (fatHeaderPtr->nfat_arch > 4) fatHeaderPtr->nfat_arch = 4;
            for (; fatHeaderPtr->nfat_arch > 0; fatHeaderPtr->nfat_arch--, fatArchPtr++) {
                if (swapit) fatArchPtr->cputype = OSSwapInt32(fatArchPtr->cputype);
                switch(fatArchPtr->cputype) {
                    case CPU_TYPE_I386:
                        DBG(" i386");
                        break;
                    case CPU_TYPE_X86_64:
                        DBG(" x86_64");
                        break;
                    case CPU_TYPE_POWERPC:
                        DBG(" PPC");
                        break;
                    case CPU_TYPE_POWERPC64:
                        DBG(" PPC64");
                        break;
                    default:
                        DBG(" 0x%x", fatArchPtr->cputype);
                }
                if (fatHeaderPtr->nfat_arch - 1 > 0) DBG(" | ");
                *compat |= (!(fatArchPtr->cputype ^ CPU_TYPE_I386) << 0) | (!(fatArchPtr->cputype ^ CPU_TYPE_X86_64) << 1) | (!(fatArchPtr->cputype ^ CPU_TYPE_POWERPC) << 2) | (!(fatArchPtr->cputype ^ CPU_TYPE_POWERPC64) << 3);
            }
            DBG(" ]\n");
            break;
        case MH_CIGAM:
        case MH_CIGAM_64:
            thinHeaderPtr->cputype = OSSwapInt32(thinHeaderPtr->cputype);
        case MH_MAGIC:
        case MH_MAGIC_64:
            *compat = (!(thinHeaderPtr->cputype ^ CPU_TYPE_I386) << 0) | (!(thinHeaderPtr->cputype ^ CPU_TYPE_X86_64) << 1) | (!(thinHeaderPtr->cputype ^ CPU_TYPE_POWERPC) << 2) | (!(thinHeaderPtr->cputype ^ CPU_TYPE_POWERPC64) << 3);
            DBG("EfiKernelCompat: kernel file is a thin binary: 1 arch compatibility found [ %s ].\n", !((thinHeaderPtr->cputype & 0x000000FF) ^ 0x00000007) ? (!((thinHeaderPtr->cputype & 0xFF000000) ^ 0x01000000) ? "x86_64" : "i386") : (!((thinHeaderPtr->cputype & 0xFF000000) ^ 0x01000000) ? "PPC64" : "PPC"));
            break;
        default:
            DBG("EfiKernelCompat: unknown kernel file '%s'. Can't determine arch compatibility!\n", kernelFilePath);
            return false;
    }
    
    return true;
}

/*
 * Installs all the needed configuration table entries
 */
static void setupEfiConfigurationTable()
{
	// smbios_p = (EFI_PTR32)getSmbios(SMBIOS_PATCHED);
	if (smbios_p) {
        if (EFI_SUCCESS == addConfigurationTable(&gEfiSmbiosTableGuid, &smbios_p, NULL)) {
            DBG("Fake EFI: sucesfuly added %sbit configuration table for SMBIOS: guid {EB9D2D31-2D88-11D3-9A16-0090273FC14D}.\n", (archCpuType == CPU_TYPE_I386) ? "32":"64");
        }
    }
    
	// Setup ACPI with DSDT overrides (mackerintel's patch)
	//setupAcpi();
    
    if (acpi10_p) {
        if (EFI_SUCCESS == addConfigurationTable(&gEfiAcpiTableGuid, &acpi10_p, "ACPI")) {
            DBG("Fake EFI: sucesfuly added %sbit configuration table for ACPI: guid {EB9D2D30-2D88-11D3-9A16-0090273FC14D}.\n", (archCpuType == CPU_TYPE_I386) ? "32":"64");
        }
    }
    if (acpi20_p) {
        if (EFI_SUCCESS == addConfigurationTable(&gEfiAcpi20TableGuid, &acpi20_p, "ACPI_20")) {
            DBG("Fake EFI: sucesfuly added %sbit configuration table for ACPI_20: guid {8868E871-E4F1-11D3-BC22-0080C73C8881}.\n", (archCpuType == CPU_TYPE_I386) ? "32":"64");
        }
    }
    
	// We've obviously changed the count.. so fix up the CRC32
	if (archCpuType == CPU_TYPE_I386)
	{
		gST32->Hdr.CRC32 = 0;
		gST32->Hdr.CRC32 = crc32(0L, gST32, gST32->Hdr.HeaderSize);
	}
	else
	{
		gST64->Hdr.CRC32 = 0;
		gST64->Hdr.CRC32 = crc32(0L, gST64, gST64->Hdr.HeaderSize);
	}
}

void setupEfiNode(void)
{
	EFI_CHAR16*	 ret16 = 0;
	size_t		 len = 0;
    
	Node *efiNode = DT__FindNode("/efi", true);
    if (efiNode == 0)
	{
		stop("Couldn't get '/efi' node");
	}

/* Bungo: we have 64 bit ability fake efi but mode may be changed 32/64, like Macs
	if (archCpuType == CPU_TYPE_I386) {
		DT__AddProperty(efiNode, FIRMWARE_ABI_PROP, sizeof(FIRMWARE_ABI_32_PROP_VALUE), (EFI_CHAR8 *)FIRMWARE_ABI_32_PROP_VALUE);
	} else { */
		DT__AddProperty(efiNode, FIRMWARE_ABI_PROP, sizeof(FIRMWARE_ABI_64_PROP_VALUE), (EFI_CHAR8 *)FIRMWARE_ABI_64_PROP_VALUE);
//	}
// So, added 'efi-mode' property to tell us what mode we use 32 or 64 bit
	DT__AddProperty(efiNode, EFI_MODE_PROP, sizeof(EFI_UINT8), (EFI_UINT8 *)&bootArgs->efiMode);

	DT__AddProperty(efiNode, FIRMWARE_REVISION_PROP, sizeof(FIRMWARE_REVISION), (EFI_UINT32 *)&FIRMWARE_REVISION);
	DT__AddProperty(efiNode, FIRMWARE_VENDOR_PROP, sizeof(FIRMWARE_VENDOR), (EFI_CHAR16 *)&FIRMWARE_VENDOR);

	// TODO: Fill in other efi properties if necessary

	// Set up the /efi/runtime-services table node similar to the way a child node of configuration-table
	// is set up.  That is, name and table properties
	Node *runtimeServicesNode = DT__AddChild(efiNode, "runtime-services");

	if (archCpuType == CPU_TYPE_I386) {
		// The value of the table property is the 32-bit physical address for the RuntimeServices table.
		// Since the EFI system table already has a pointer to it, we simply use the address of that pointer
		// for the pointer to the property data.  Warning.. DT finalization calls free on that but we're not
		// the only thing to use a non-malloc'd pointer for something in the DT
		DT__AddProperty(runtimeServicesNode, "table", sizeof(EFI_UINT64), &gST32->RuntimeServices);
	} else {
		DT__AddProperty(runtimeServicesNode, "table", sizeof(EFI_UINT64), &gST64->RuntimeServices);
	}

	// Set up the /efi/configuration-table node which will eventually have several child nodes for
	// all of the configuration tables needed by various kernel extensions.
	gEfiConfigurationTableNode = DT__AddChild(efiNode, "configuration-table");

	// Set up the /efi/kernel-compatibility node only if 10.7 or better
    if (MacOSVerCurrent >= MacOSVer2Int("10.7")) {
        Node *EfiKernelCompatNode = DT__AddChild(efiNode, "kernel-compatibility");
        EFI_UINT8 compat = (archCpuType == CPU_TYPE_I386) ? 0b00000001 : 0b00000010;
        getKernelCompat(&compat);
        if (compat & 0b00000001) DT__AddProperty(EfiKernelCompatNode, "i386", sizeof(EFI_UINT32), (EFI_UINT32 *)&ONE_U32);
        if (compat & 0b00000010) DT__AddProperty(EfiKernelCompatNode, "x86_64", sizeof(EFI_UINT32), (EFI_UINT32 *)&ONE_U32);
        if (compat & 0b00000100) DT__AddProperty(EfiKernelCompatNode, "PPC", sizeof(EFI_UINT32), (EFI_UINT32 *)&ONE_U32);
        if (compat & 0b00001000) DT__AddProperty(EfiKernelCompatNode, "PPC64", sizeof(EFI_UINT32), (EFI_UINT32 *)&ONE_U32);
    }

	// Now fill in the /efi/platform Node
	Node *efiPlatformNode = DT__AddChild(efiNode, "platform");

	// NOTE WELL: If you do add FSB Frequency detection, make sure to store
	// the value in the fsbFrequency global and not an malloc'd pointer
	// because the DT_AddProperty function does not copy its args.

	if (Platform.CPU.FSBFrequency != 0) {
		DT__AddProperty(efiPlatformNode, FSB_Frequency_prop, sizeof(EFI_UINT64), (EFI_UINT64 *)&Platform.CPU.FSBFrequency);
	}

	// Export TSC and CPU frequencies for use by the kernel or KEXTs
	if (Platform.CPU.TSCFrequency != 0) {
		DT__AddProperty(efiPlatformNode, TSC_Frequency_prop, sizeof(EFI_UINT64), (EFI_UINT64 *)&Platform.CPU.TSCFrequency);
	}

	if (Platform.CPU.CPUFrequency != 0) {
		DT__AddProperty(efiPlatformNode, CPU_Frequency_prop, sizeof(EFI_UINT64), (EFI_UINT64 *)&Platform.CPU.CPUFrequency);
	}

	DT__AddProperty(efiPlatformNode, DEV_PATHS_SUP_PROP, sizeof(EFI_UINT32), (EFI_UINT32 *)&ONE_U32);

	// Bungo
	/* Export system-id. Can be disabled with SystemId=No in com.apple.Boot.plist
	if ((ret=getSystemID())) {
		DT__AddProperty(efiPlatformNode, SYSTEM_ID_PROP, UUID_LEN, (EFI_UINT32*) ret);
	}
	*/
	DT__AddProperty(efiPlatformNode, SYSTEM_ID_PROP, UUID_LEN, Platform.UUID);

	// Export SystemSerialNumber if present
	if ((ret16=getSmbiosChar16("SMserial", &len))) {
		DT__AddProperty(efiPlatformNode, SYSTEM_SERIAL_PROP, len, ret16);
	}

	// Export Model if present
	if ((ret16=getSmbiosChar16("SMproductname", &len))) {
		DT__AddProperty(efiPlatformNode, MODEL_PROP, len, ret16);
	}

	// Fill /efi/device-properties.
	setupDeviceProperties(efiNode);
    
    // Add configuration table entries to both the services table and the device tree
	setupEfiConfigurationTable();
}

/*
 * Must be called AFTER getSmbios
 */
void setupBoardId()
{
	Node *node;
	node = DT__FindNode("/", false);
	if (node == 0)
	{
		stop("Couldn't get root '/' node");
	}
	const char *boardid = getStringForKey("SMboardproduct", &bootInfo->smbiosConfig);
	if (boardid)
	{
		DT__AddProperty(node, BOARDID_PROP, strlen(boardid) + 1, (EFI_CHAR16 *)boardid);
	}
}

/*
 * Fill up the root node
 *
setupRootNode()
{
    
}
*/
/*
 * Fill up the chosen node
 */
void setupChosenNode()
{
	Node *chosenNode = DT__FindNode("/chosen", false);
	if (chosenNode == 0) {
		stop("setupChosenNode: Couldn't get '/chosen' node");
	}

    // Add boot-uuid property
	int length = strlen(gBootUUIDString);
	if (length) {
		DT__AddProperty(chosenNode, "boot-uuid", length + 1, gBootUUIDString);
	}

    // Add boot-args property
	length = strlen(bootArgs->CommandLine);
	DT__AddProperty(chosenNode, "boot-args", length + 1, bootArgs->CommandLine);

    // Add boot-file property
	length = strlen(bootInfo->bootFile);
	DT__AddProperty(chosenNode, "boot-file", length + 1, bootInfo->bootFile);
    
    //  TODO:
    //	DT__AddProperty(chosenNode, "boot-device-path", bootDPsize, gBootDP);
    //	DT__AddProperty(chosenNode, "boot-file-path", bootFPsize, gBootFP);
    //	DT__AddProperty(chosenNode, "boot-kernelchache-adler32", sizeof(adler32), adler32);

    // Add machine-signature property
	DT__AddProperty(chosenNode, MACHINE_SIG_PROP, sizeof(EFI_UINT32), (EFI_UINT32 *)&Platform.HWSignature);
    
	//if(YOSEMITE)
    if (MacOSVerCurrent >= MacOSVer2Int("10.10")) // Add random-seed property if Yosemite or better only
	{
		//
		// Pike R. Alpha - 12 October 2014
		//
		UInt8 index = 0;
		EFI_UINT16 PMTimerValue = 0;
		EFI_UINT32 randomValue, tempValue, cpuTick;
		EFI_UINT32 ecx, esi, edi = 0;
		EFI_UINT32 rcx, rdx, rsi, rdi;

		randomValue = tempValue = ecx = esi = edi = 0;					// xor		%ecx,	%ecx
		rcx = rdx = rsi = rdi = cpuTick = 0;

		// LEAF_1 - Feature Information (Function 01h).
		if (Platform.CPU.CPUID[CPUID_1][2] & 0x40000000)				// Checking ecx:bit-30
		{
			//
			// i5/i7 Ivy Bridge and Haswell processors with RDRAND support.
			//
			EFI_UINT32 seedBuffer[16] = {0};
			//
			// Main loop to get 16 qwords (four bytes each).
			//
			for (index = 0; index < 16; index++)					// 0x17e12:
			{
				randomValue = computeRand();					// callq	0x18e20
				cpuTick = getCPUTick();						// callq	0x121a7
				randomValue = (randomValue ^ cpuTick);				// xor		%rdi,	%rax
				seedBuffer[index] = randomValue;				// mov		%rax,(%r15,%rsi,8)
			}									// jb		0x17e12

			DT__AddProperty(chosenNode, RANDOM_SEED_PROP, sizeof(seedBuffer), (EFI_UINT32*) &seedBuffer);
		}
		else
		{
			//
			// All other processors without RDRAND support.
			//
			EFI_UINT8 seedBuffer[64] = {0};
			//
			// Main loop to get the 64 bytes.
			//
			do									// 0x17e55:
			{
				PMTimerValue = inw(0x408);					// in		(%dx),	%ax
				esi = PMTimerValue;						// movzwl	%ax,	%esi

				if (esi < ecx)							// cmp		%ecx,	%esi
				{
					continue;						// jb		0x17e55		(retry)
				}

				cpuTick = getCPUTick();						// callq	0x121a7
				rcx = (cpuTick >> 8);						// mov		%rax,	%rcx
				// shr		$0x8,	%rcx
				rdx = (cpuTick >> 10);						// mov		%rax,	%rdx
				// shr		$0x10,	%rdx
				rdi = rsi;							// mov		%rsi,	%rdi
				rdi = (rdi ^ cpuTick);						// xor		%rax,	%rdi
				rdi = (rdi ^ rcx);						// xor		%rcx,	%rdi
				rdi = (rdi ^ rdx);						// xor		%rdx,	%rdi

				seedBuffer[index] = (rdi & 0xff);				// mov		%dil,	(%r15,%r12,1)

				edi = (edi & 0x2f);						// and		$0x2f,	%edi
				edi = (edi + esi);						// add		%esi,	%edi
				index++;							// inc		r12
				ecx = (edi & 0xffff);						// movzwl	%di,	%ecx

			} while (index < 64);							// cmp		%r14d,	%r12d
			// jne		0x17e55		(next)

			DT__AddProperty(chosenNode, RANDOM_SEED_PROP, sizeof(seedBuffer), (EFI_UINT8*) &seedBuffer);

		}
	}

    // setup '/chosen/memory-map' node
    Node * mapNode = DT__FindNode("/chosen/memory-map", false);
	if (mapNode == 0) {
		DBG("setupChosenNode:Couldn't get '/chosen/memory-map' node\n");
	} else { /*
        static EFI_UINT64 BootClutPropValue = 0;
        static EFI_UINT64 FailedBootPictPropValue = 0;
        BootClutPropValue = ((EFI_UINT64)sizeof(appleClut8) << 32) | (EFI_UINT64)(EFI_UINT8 *)&appleClut8;
        FailedBootPictPropValue = ((EFI_UINT64)(sizeof(gFailedBootPict) + 32) << 32) | (EFI_UINT64)(EFI_UINT8 *)&gFailedBootPict;
        if (MacOSVerCurrent < MacOSVer2Int("10.7.3")) {
            DT__AddProperty(node, "BootCLUT", sizeof(EFI_UINT64), (EFI_UINT64 *)&BootClutPropValue);
            DT__AddProperty(node, "Pict-FailedBoot", sizeof(EFI_UINT64), (EFI_UINT64 *)&FailedBootPictPropValue);
        } else {
            DT__AddProperty(node, "FailedCLUT", sizeof(EFI_UINT64), (EFI_UINT64 *)&BootClutPropValue);
            DT__AddProperty(node, "FailedImage", sizeof(EFI_UINT64), (EFI_UINT64 *)&FailedBootPictPropValue);
        } */
    }
}

/*
 * Load the smbios.plist override config file if any
 */
static void setupSmbiosConfigFile(const char *filename)
{
	char		dirSpecSMBIOS[128];
	const char	*override_pathname = NULL;
	int		len = 0, err = 0;
	extern void scan_mem();

	// Take in account user overriding
	if (getValueForKey(kSMBIOSKey, &override_pathname, &len, &bootInfo->chameleonConfig) && len > 0)
	{
		// Specify a path to a file, e.g. SMBIOS=/Extra/macProXY.plist
		sprintf(dirSpecSMBIOS, override_pathname);
		err = loadConfigFile(dirSpecSMBIOS, &bootInfo->smbiosConfig);
	}
	else
	{
		// Check selected volume's Extra.
		sprintf(dirSpecSMBIOS, "/Extra/%s", filename);
		if ( (err = loadConfigFile(dirSpecSMBIOS, &bootInfo->smbiosConfig)) )
		{
			// Check booter volume/rdbt Extra.
			sprintf(dirSpecSMBIOS, "bt(0,0)/Extra/%s", filename);
			err = loadConfigFile(dirSpecSMBIOS, &bootInfo->smbiosConfig);
		}
	}

	if (err)
	{
		DBG("setupSmbiosConfigFile: No SMBIOS replacement found.\n");
	}

	// get a chance to scan mem dynamically if user asks for it while having the config options
	// loaded as well, as opposed to when it was in scan_platform(); also load the orig. smbios
	// so that we can access dmi info, without patching the smbios yet.
	scan_mem(); 
}

void saveOriginalSMBIOS(void)
{
	Node *node;
	SMBEntryPoint *origeps;
	void *tableAddress;

	node = DT__FindNode("/efi/platform", false);
	if (!node)
	{
		DBG("saveOriginalSMBIOS: '/efi/platform' node not found\n");
		return;
	}

	origeps = getSmbios(SMBIOS_ORIGINAL);
	if (!origeps)
	{
        DBG("saveOriginalSMBIOS: original SMBIOS not found\n");
		return;
	}

	tableAddress = (void *)AllocateKernelMemory(origeps->dmi.tableLength);
	if (!tableAddress)
	{
        DBG("saveOriginalSMBIOS: can not allocate memory for original SMBIOS\n");
		return;
	}

	memcpy(tableAddress, (void *)origeps->dmi.tableAddress, origeps->dmi.tableLength);
	DT__AddProperty(node, "SMBIOS", origeps->dmi.tableLength, tableAddress);
}

char saveOriginalACPI()
{
    Node *node = DT__FindNode("/chosen/acpi", true);
    if (!node) {
        DBG("saveOriginalACPI: node '/chosen/acpi' not found. Can't save OEM ACPI tables.\n");
        return -1;
    }
    
    struct acpi_2_rsdp *RSDP = getRSDPaddress();
    
    if (!RSDP) {
        DBG("saveOriginalACPI: RSDP not found or incorrect, can't save OEM tables.\n");
        return -1;
    }
    
    DBG("saveOriginalACPI: saving OEM tables into IODT/chosen/acpi...\n");
    uint32_t length = RSDP->Revision ? RSDP->Length : 20;
    uint8_t nameLen = strlen("RSDP@") + 8 + 1;
    char *nameBuf = malloc(nameLen);
    sprintf(nameBuf, "RSDP@%08X", RSDP);
    DBG("saveOriginalACPI: OEM table %s found, size=%d: saving.\n", nameBuf, length);
    DT__AddProperty(node, nameBuf, length, RSDP);
    uint8_t total_number = 1;
    uint32_t total_size = length;
    uint8_t r, ssdt_number = 0;
    struct acpi_2_header *RSDT = (struct acpi_2_header *)(RSDP->RsdtAddress), *XSDT = NULL;
    void *origTable = (void *)(RSDT + 1), *origTable2, *origTable3;
    if (RSDT && tableSign(RSDT->Signature, "RSDT")) {
        length = RSDT->Length;
        nameLen = strlen("RSDT@") + 8 + 1;
        nameBuf = malloc(nameLen);
        sprintf(nameBuf, "RSDT@%08X", RSDT);
        DBG("saveOriginalACPI: OEM table %s found, size=%d: saving.\n", nameBuf, length);
        DT__AddProperty(node, nameBuf, length, RSDT);
        total_size += length;
        total_number++;
        for (; origTable < ((void *)RSDT + RSDT->Length); origTable += 4) {
            origTable2 = (void *)(*(uint32_t *)origTable);
            length = ((struct acpi_2_header *)origTable2)->Length;
            total_size += length;
            total_number++;
            if (tableSign(((struct acpi_2_header *)origTable2)->Signature, "SSDT")) {
                ssdt_number++;
                if (!strcmp(((struct acpi_2_header *)origTable2)->OEMTableId, "CpuPm")) {
                    nameLen = strlen("SSDT@") + 8 + 1;
                    nameBuf = malloc(nameLen);
                    sprintf(nameBuf, "SSDT@%08X", origTable2);
                    DBG("saveOriginalACPI: OEM table %s found, size=%d: saving.\n", nameBuf, length);
                    DT__AddProperty(node, nameBuf, length, origTable2);
                    origTable2 += sizeof(struct acpi_2_header) + 15;
                    r = *((uint8_t *)origTable2 - 2) / 3;
                    for (; r > 0; r--, origTable2 += sizeof(struct ssdt_pmref)) {
                        origTable3 = (void *)(((struct ssdt_pmref *)origTable2)->addr);
                        if (!origTable3) continue;
                        length = ((struct acpi_2_header *)origTable3)->Length;
                        nameLen = strlen("SSDT_@") + strlen(((struct acpi_2_header *)origTable3)->OEMTableId) + 8 + 1;
                        nameBuf = malloc(nameLen);
                        sprintf(nameBuf, "SSDT_%s@%08X", ((struct acpi_2_header *)origTable3)->OEMTableId, origTable3);
                        DBG("saveOriginalACPI: OEM table %s found, size=%d: saving.\n", nameBuf, length);
                        DT__AddProperty(node, nameBuf, length, origTable3);
                        total_size += length;
                        total_number++;
                    }
                } else {
                    nameLen = strlen("SSDT-@") + ((ssdt_number < 10) ? 1:2) + 8 + 1;
                    nameBuf = malloc(nameLen);
                    sprintf(nameBuf, "SSDT-%d@%08X", ssdt_number, origTable2);
                    DBG("saveOriginalACPI: OEM table %s found, size=%d: saving.\n", nameBuf, length);
                    DT__AddProperty(node, nameBuf, length, origTable2);
                }
            } else {
                nameLen = strlen("XXXX@") + 8 + 1;
                nameBuf = malloc(nameLen);
                sprintf(nameBuf, "%c%c%c%c@%08X", ((char *)origTable2)[0], ((char *)origTable2)[1], ((char *)origTable2)[2], ((char *)origTable2)[3], origTable2);
                DBG("saveOriginalACPI: OEM table %s found, size=%d: saving.\n", nameBuf, length);
                DT__AddProperty(node, nameBuf, length, origTable2);
                if (tableSign(((struct acpi_2_header *)origTable2)->Signature, "FACP")) {
                    origTable3 = (void *)(((struct acpi_2_fadt *)origTable2)->FACS);
                    length = ((struct acpi_2_header *)origTable3)->Length;
                    nameLen = strlen("FACS@") + 8 + 1;
                    nameBuf = malloc(nameLen);
                    sprintf(nameBuf, "FACS@%08X", origTable3);
                    DBG("saveOriginalACPI: OEM table %s found, size=%d: saving.\n", nameBuf, length);
                    DT__AddProperty(node, nameBuf, length, origTable3);
                    total_size += length;
                    total_number++;
                    origTable3 = (void *)(((struct acpi_2_fadt *)origTable2)->DSDT);
                    length = ((struct acpi_2_header *)origTable3)->Length;
                    nameLen = strlen("DSDT@") + 8 + 1;
                    nameBuf = malloc(nameLen);
                    sprintf(nameBuf, "DSDT@%08X", origTable3);
                    DBG("saveOriginalACPI: OEM table %s found, size=%d: saving.\n", nameBuf, length);
                    DT__AddProperty(node, nameBuf, length, origTable3);
                    total_size += length;
                    total_number++;
                }
            }
        }
    } else {
        DBG("saveOriginalACPI: can't find OEM table: RSDT@%08x.\n", RSDT);
    }
    
    if (RSDP->Revision > 0) {
        XSDT = (struct acpi_2_header *)(RSDP->XsdtAddress);
        if (XSDT && tableSign(XSDT->Signature, "XSDT")) {
            length = XSDT->Length;
            nameLen = strlen("XSDT@") + 8 + 1;
            nameBuf = malloc(nameLen);
            sprintf(nameBuf, "XSDT@%08X", XSDT);
            DBG("saveOriginalACPI: OEM table %s found, size=%d: saving.\n", nameBuf, length);
            DT__AddProperty(node, nameBuf, length, XSDT);
            total_number++;
            total_size += length;
            origTable = (void *)(XSDT + 1);
            for (r = 0; origTable < ((void *)XSDT + XSDT->Length); origTable += 8, r += 4) {
                if ((uint64_t)*(uint32_t *)((void *)(RSDT + 1) + r) != *(uint64_t *)origTable) {
                    origTable2 = (void *)(*(uint64_t *)origTable);
                    length = ((struct acpi_2_header *)origTable2)->Length;
                    nameLen = strlen("X_XXXX@") + 8 + 1;
                    nameBuf = malloc(nameLen);
                    sprintf(nameBuf, "X_%c%c%c%c@%08X", ((char *)origTable2)[0], ((char *)origTable2)[1], ((char *)origTable2)[2], ((char *)origTable2)[3], origTable2);
                    DBG("saveOriginalACPI: OEM table %s found, size=%d: saving.\n", nameBuf, length);
                    DT__AddProperty(node, nameBuf, length, origTable2);
                    total_size += length;
                    total_number++;
                    if (tableSign(((struct acpi_2_header *)origTable2)->Signature, "FACP")) {
                        if (((struct acpi_2_fadt *)origTable2)->FACS != (uint32_t)((struct acpi_2_fadt *)origTable2)->X_FACS) {
                            origTable3 = (void *)(((struct acpi_2_fadt *)origTable2)->X_FACS);
                            length = ((struct acpi_2_rsdt *)origTable3)->Length;
                            nameLen = strlen("X_FACS@") + 8 + 1;
                            nameBuf = malloc(nameLen);
                            sprintf(nameBuf, "X_FACS@%08X", origTable3);
                            DBG("saveOriginalACPI: OEM table %s found, size=%d: saving.\n", nameBuf, length);
                            DT__AddProperty(node, nameBuf, length, origTable3);
                            total_size += length;
                            total_number++;
                        }
                        if (((struct acpi_2_fadt *)origTable2)->DSDT != (uint32_t)((struct acpi_2_fadt *)origTable2)->X_DSDT) {
                            origTable3 = (void *)(((struct acpi_2_fadt *)origTable2)->X_DSDT);
                            length = ((struct acpi_2_rsdt *)origTable3)->Length;
                            nameLen = strlen("X_DSDT@") + 8 + 1;
                            nameBuf = malloc(nameLen);
                            sprintf(nameBuf, "X_DSDT@%08X", origTable3);
                            DBG("saveOriginalACPI: OEM table %s found, size=%d: saving.\n", nameBuf, length);
                            DT__AddProperty(node, nameBuf, length, origTable3);
                            total_size += length;
                            total_number++;
                        }
                    }
                }
            }
        } else {
            DBG("saveOriginalACPI: can't find OEM table: XSDT@%x.\n", origTable);
        }
    }
    
    DBG("saveOriginalACPI: %d original table%s found and saved, total size=%d.\n", total_number, (total_number != 1) ? "s" : "", total_size);
    if (!RSDT && !XSDT) return -1;
    else return total_number;
}

/*
 * Entrypoint from boot.c
 */
void setupFakeEfi(void)
{
	// Generate efi device strings
	setup_pci_devs(root_pci_dev);

	readSMBIOSInfo(getSmbios(SMBIOS_ORIGINAL));

	// load smbios.plist file if any
	setupSmbiosConfigFile("smbios.plist");
    
    // Setup ACPI with DSDT overrides (mackerintel's patch)
	setupAcpi();
    
    setupSMBIOS();
    
    // Setup board-id: need to be called after getSmbios!
    setupBoardId();
    
    // Setup system-type: We now have to write the systemm-type in ioregs: we cannot do it before in setupDeviceTree()
	// because we need to take care of FACP original content, if it is correct.
	setupSystemType();
    
    // Setup the '/' node
	//setupRootNode();
    
	// Setup the '/chosen' node
	setupChosenNode();

	// Initialize the base table
	if (archCpuType == CPU_TYPE_I386)
	{
		setupEfiTables32();
	}
	else
	{
		setupEfiTables64();
	}

    // Setup efi node
	setupEfiNode();

    saveOriginalSMBIOS();
    
    saveOriginalACPI();
}
