#!/bin/bash

echo "==============================================="
echo "Set Active Partition ONLY if Windows is not installed"
echo "*****************************************************"

# Sets partition active if Windows is not installed.

# Receives efiformat: code is 1 for HFS, 2 for MSDOS, 0 for unknown
# Receives diskSigCheck: code is 1 for a Windows install, 0 for no Windows install
# Receives targetDiskRaw: for example, /dev/rdisk1
# Receives targetSlice: for example, 1
# Receives targetVolume: Volume to install to.
# Receives scriptDir: The location of the main script dir.

if [ "$#" -eq 6 ]; then
	efiformat="$1"
	diskSigCheck="$2"
	targetDiskRaw="$3"
	targetSlice="$4"
	targetVolume="$5"
	scriptDir="$6"

	echo "DEBUG: passed argument for efiformat = $efiformat"
	echo "DEBUG: passed argument for diskSigCheck = $diskSigCheck"
	echo "DEBUG: passed argument for targetDiskRaw = $targetDiskRaw"
	echo "DEBUG: passed argument for targetSlice = $targetSlice"
	echo "DEBUG: passed argument for targetVolume = $targetVolume"
	echo "DEBUG: passed argument for scriptDir = $scriptDir"
else
	echo "Error - wrong number of values passed"
	exit 9
fi

partitionactive=$( fdisk -d ${targetDiskRaw} | grep -n "*" | awk -F: '{print $1}')
"$scriptDir"InstallLog.sh "${targetVolume}" "Current active partition on ${targetDiskRaw#/dev/r}: ${partitionactive}"

if [ ${diskSigCheck} == "0" ]; then
	#echo "DEBUG: Windows is not installed so let's change the active partition"
	"$scriptDir"InstallLog.sh "${targetVolume}" "Windows is not installed so let's change the active partition"

	if [ "${partitionactive}" = "${targetSlice}" ]; then
		#echo "${targetVolume} is already flagged as active"
		"$scriptDir"InstallLog.sh "${targetVolume}" "${targetVolume} is already flagged as active"
	else
		#echo "${targetVolume} is not flagged as active, so let's do it."
		"$scriptDir"InstallLog.sh "${targetVolume}" "${targetVolume} is not flagged as active, so let's do it."
		# BadAxe requires EFI partition to be flagged active.
		# but it doesn't' hurt to do it for any non-windows partition.
		
		fdisk -e ${targetDiskRaw} <<-MAKEACTIVE
		print
		flag ${targetSlice}
		write
		y
		quit
		MAKEACTIVE
	fi
else
	#echo "Windows is installed so we let that remain the active partition"
	"$scriptDir"InstallLog.sh "${targetVolume}" "Windows is installed so that can remain the active partition"
fi

echo "-----------------------------------------------"
echo ""
echo ""

exit 0