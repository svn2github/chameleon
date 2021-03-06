/*
 *  platform.h
 *  AsereBLN: reworked and extended
 *	valv: further additions
 */

#ifndef __LIBSAIO_PLATFORM_H
#define __LIBSAIO_PLATFORM_H

#include "libsaio.h"

extern bool platformCPUFeature(uint32_t);
extern void scan_platform(void);
extern void dumpPhysAddr(const char * title, void * a, int len);

#define bit(n)			(1UL << (n))
#define bitmask(h,l)		((bit(h)|(bit(h)-1)) & ~(bit(l)-1))
#define bitfield(x,h,l)		(((x) & bitmask(h,l)) >> l)


/* CPUID index into cpuid_raw */
#define CPUID_0				0
#define CPUID_1				1
#define CPUID_2				2
#define CPUID_3				3
#define CPUID_4				4
#define CPUID_80			5
#define CPUID_81			6
#define CPUID_MAX			7

/* CPU Features */
// NOTE: These are currently mapped to the actual bit in the cpuid value
#define CPU_FEATURE_MMX			bit(23)		// MMX Instruction Set
#define CPU_FEATURE_SSE			bit(25)		// SSE Instruction Set
#define CPU_FEATURE_SSE2		bit(26)		// SSE2 Instruction Set
#define CPU_FEATURE_SSE3		bit(0)		// SSE3 Instruction Set
#define CPU_FEATURE_SSE41		bit(19)		// SSE41 Instruction Set
#define CPU_FEATURE_SSE42		bit(20)		// SSE42 Instruction Set
#define CPU_FEATURE_EM64T		bit(29)		// 64Bit Support
#define CPU_FEATURE_HTT			bit(28)		// HyperThreading
#define CPU_FEATURE_MSR			bit(5)		// MSR Support
#define CPU_FEATURE_APIC		bit(9)		// On-chip APIC Hardware
#define CPU_FEATURE_EST			bit(7)		// Enhanced Intel SpeedStep
#define CPU_FEATURE_TM2			bit(8)		// Thermal Monitor 2
#define CPU_FEATURE_TM1			bit(29)		// Thermal Monitor 1
#define CPU_FEATURE_SSSE3		bit(9)		// Supplemental SSE3 Instruction Set
#define CPU_FEATURE_xAPIC		bit(21)		// Extended APIC Mode
#define CPU_FEATURE_ACPI		bit(22)		// Thermal Monitor and Software Controlled Clock
#define CPU_FEATURE_LAHF		bit(20)		// LAHF/SAHF Instructions
#define CPU_FEATURE_XD			bit(20)		// Execute Disable

// NOTE: Determine correct bit for below (28 is already in use)
#define CPU_FEATURE_MOBILE		bit(1)		// Mobile CPU


/* SMBIOS Memory Types */ 
#define SMB_MEM_TYPE_UNDEFINED	0
#define SMB_MEM_TYPE_OTHER		1
#define SMB_MEM_TYPE_UNKNOWN	2
#define SMB_MEM_TYPE_DRAM		3
#define SMB_MEM_TYPE_EDRAM		4
#define SMB_MEM_TYPE_VRAM		5
#define SMB_MEM_TYPE_SRAM		6
#define SMB_MEM_TYPE_RAM		7
#define SMB_MEM_TYPE_ROM		8
#define SMB_MEM_TYPE_FLASH		9
#define SMB_MEM_TYPE_EEPROM		10
#define SMB_MEM_TYPE_FEPROM		11
#define SMB_MEM_TYPE_EPROM		12
#define SMB_MEM_TYPE_CDRAM		13
#define SMB_MEM_TYPE_3DRAM		14
#define SMB_MEM_TYPE_SDRAM		15
#define SMB_MEM_TYPE_SGRAM		16
#define SMB_MEM_TYPE_RDRAM		17
#define SMB_MEM_TYPE_DDR		18
#define SMB_MEM_TYPE_DDR2		19
#define SMB_MEM_TYPE_FBDIMM		20
#define SMB_MEM_TYPE_DDR3		24			// Supported in 10.5.6+ AppleSMBIOS

/* Memory Configuration Types */ 
#define SMB_MEM_CHANNEL_UNKNOWN		0
#define SMB_MEM_CHANNEL_SINGLE		1
#define SMB_MEM_CHANNEL_DUAL		2
#define SMB_MEM_CHANNEL_TRIPLE		3

/* Maximum number of ram slots */
#define MAX_RAM_SLOTS			8
#define RAM_SLOT_ENUMERATOR		{0, 2, 4, 1, 3, 5, 6, 8, 10, 7, 9, 11}

/* Maximum number of SPD bytes */
#define MAX_SPD_SIZE			256

/* Size of SMBIOS UUID in bytes */
#define UUID_LEN			16

typedef struct _RamSlotInfo_t {
    uint32_t		ModuleSize;						// Size of Module in MB
    uint32_t		Frequency; // in Mhz
    const char*		Vendor;
    const char*		PartNo;
    const char*		SerialNo;
    char*			spd;							// SPD Dump
    bool			InUse;
    uint8_t			Type;
    uint8_t			BankConnections; // table type 6, see (3.3.7)
    uint8_t			BankConnCnt;

} RamSlotInfo_t;

typedef struct _PlatformInfo_t {
	struct CPU {
		uint32_t		Features;				// CPU Features like MMX, SSE2, VT, MobileCPU
		uint32_t		Vendor;					// Vendor
		uint32_t		Signature;				// Signature
		uint32_t		Stepping;				// Stepping
		uint32_t		Model;					// Model
		uint32_t		Type;					// Processor Type
		uint32_t		ExtModel;				// Extended Model
		uint32_t		Family;					// Family
		uint32_t		ExtFamily;				// Extended Family
		uint32_t		NoCores;				// No Cores per Package
		uint32_t		NoThreads;				// Threads per Package
		uint8_t			MaxDiv;					// Max Halving ID
		uint8_t			CurrDiv;				// Current Halving ID
		uint64_t		TSCFrequency;			// TSC Frequency Hz
		uint64_t		FSBFrequency;			// FSB Frequency Hz
		uint64_t		FSBIFrequency;			// FSB Frequency Hz (initial)
		uint64_t		CPUFrequency;			// CPU Frequency Hz
		uint32_t		MaxRatio;				// Max Bus Ratio
		uint32_t		MinRatio;				// Min Bus Ratio
		uint8_t			Tone;					// Turbo Ratio limit (1 core)
		uint8_t			Ttwo;					// Turbo Ratio limit (2 cores)
		uint8_t			Tthr;					// Turbo Ratio limit (3 cores)
		uint8_t			Tfor;					// Turbo Ratio limit (4 cores)
		bool			ISerie;					// Intel's Core-i model
		bool			Turbo;					// Intel's Turbo Boost support
		uint8_t			SLFM;					// Dynamic FSB
		uint8_t			EST;					// Enhanced SpeedStep
		char			BrandString[48];		// 48 Byte Branding String
		uint32_t		CPUID[CPUID_MAX][4];	// CPUID 0..4, 80..81 Raw Values
	} CPU;

	struct RAM {
		uint64_t		Frequency;				// Ram Frequency
		uint32_t		Divider;				// Memory divider
		uint8_t			CAS;					// CAS 1/2/2.5/3/4/5/6/7
		uint8_t			TRC;					
		uint8_t			TRP;
		uint8_t			RAS;
		uint8_t			Channels;				// Channel Configuration Single,Dual or Triple
		uint8_t			NoSlots;				// Maximum no of slots available
		uint8_t			Type;					// Standard SMBIOS v2.5 Memory Type
		RamSlotInfo_t	DIMM[MAX_RAM_SLOTS];	// Information about each slot
	} RAM;

	struct DMI {
		int			MaxMemorySlots;		// number of memory slots polulated by SMBIOS
		int			CntMemorySlots;		// number of memory slots counted
		int			MemoryModules;		// number of memory modules installed
		int			DIMM[MAX_RAM_SLOTS];	// Information and SPD mapping for each slot
	} DMI;

	uint8_t				Type;			// System Type: 1=Desktop, 2=Portable... according ACPI2.0 (FACP: PM_Profile)

} PlatformInfo_t;

extern PlatformInfo_t Platform;

#endif /* !__LIBSAIO_PLATFORM_H */
