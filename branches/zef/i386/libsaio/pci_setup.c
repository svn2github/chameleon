#include "libsaio.h"
#include "boot.h"
#include "bootstruct.h"
#include "pci.h"
#include "nvidia.h"
#include "ati.h"

extern void set_eth_builtin(pci_dt_t *eth_dev);
extern void notify_usb_dev(pci_dt_t *pci_dev);
extern void force_enable_hpet(pci_dt_t *lpc_dev);

void setup_pci_devs(pci_dt_t *pci_dt)
{
	char *devicepath;
	bool do_eth_devprop, do_gfx_devprop, do_enable_hpet;
	pci_dt_t *current = pci_dt;

	do_eth_devprop = do_gfx_devprop = do_enable_hpet = false;

	getBoolForKey(kEthernetBuiltIn, &do_eth_devprop, &bootInfo->bootConfig);
	getBoolForKey(kGraphicsEnabler, &do_gfx_devprop, &bootInfo->bootConfig);
	getBoolForKey(kForceHPET, &do_enable_hpet, &bootInfo->bootConfig);

	while (current)
	{
		devicepath = get_pci_dev_path(current);

		switch (current->class_id)
		{
			case PCI_CLASS_NETWORK_ETHERNET: 
				if (do_eth_devprop)
					set_eth_builtin(current);
				break;
				
			case PCI_CLASS_DISPLAY_VGA:
				if (do_gfx_devprop)
					switch (current->vendor_id)
					{
						case PCI_VENDOR_ID_ATI:
							verbose("ATI VGA Controller [%04x:%04x] :: %s \n", 
							current->vendor_id, current->device_id, devicepath);
							setup_ati_devprop(current); 
							break;
					
						case PCI_VENDOR_ID_INTEL: 
							/* message to be removed once support for these cards is added */
							verbose("Intel VGA Controller [%04x:%04x] :: %s (currently NOT SUPPORTED)\n", 
								current->vendor_id, current->device_id, devicepath);
							break;
					
						case PCI_VENDOR_ID_NVIDIA: 
							setup_nvidia_devprop(current);
							break;
					}
				break;

			case PCI_CLASS_SERIAL_USB:
				notify_usb_dev(current);
				break;

			case PCI_CLASS_BRIDGE_ISA:
				if (do_enable_hpet)
					force_enable_hpet(current);
				break;
		}
		
		setup_pci_devs(current->children);
		current = current->next;
	}
}
