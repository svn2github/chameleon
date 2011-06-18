#include "libsaio.h"
#include "boot.h"
#include "bootstruct.h"
#include "pci.h"
#include "modules.h"


extern void set_eth_builtin(pci_dt_t *eth_dev);
extern void notify_usb_dev(pci_dt_t *pci_dev);
extern void force_enable_hpet(pci_dt_t *lpc_dev);

extern pci_dt_t *dram_controller_dev;

void setup_pci_devs(pci_dt_t *pci_dt)
{
	bool do_eth_devprop, do_enable_hpet;
	pci_dt_t *current = pci_dt;
	
	do_eth_devprop = do_enable_hpet = false;
	
	getBoolForKey(kEthernetBuiltIn, &do_eth_devprop, &bootInfo->bootConfig);
	getBoolForKey(kForceHPET, &do_enable_hpet, &bootInfo->bootConfig);

	while (current)
	{
		switch (current->class_id)
		{
			case PCI_CLASS_BRIDGE_HOST:
					if (current->dev.addr == PCIADDR(0, 0, 0))
						dram_controller_dev = current;
				break;
			
			case PCI_CLASS_NETWORK_ETHERNET: 
				if (do_eth_devprop)
					set_eth_builtin(current);
				break;
			
			case PCI_CLASS_SERIAL_USB:
				notify_usb_dev(current);
				break;
			
			case PCI_CLASS_BRIDGE_ISA:
				if (do_enable_hpet)
					force_enable_hpet(current);
				break;
		}
		
		execute_hook("PCIDevice", current, NULL, NULL, NULL);
		
		setup_pci_devs(current->children);
		current = current->next;
	}
}