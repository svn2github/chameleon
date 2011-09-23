#!/bin/bash

echo "==============================================="
echo "Write Chameleon Stage 2 Loader:"
echo "*******************************"

# Writes Chameleon stage 2 loader.

# Receives espformat: 1 for HFS, 2 for MSDOS, 0 for unknown
# Receives stage2Loader: Name of file - boot
# Receives selectedDestination: for example, /Volumes/USB
# Receives targetDevice: for example, /dev/disk3s1
# Receives targetVolume: for example, /Volumes/USB
# Receives scriptDir: The location of the main script dir.


if [ "$#" -eq 6 ]; then
	espformat="$1"
	stage2Loader="$2"
	selectedDestination="$3"
	targetDevice="$4"
	targetVolume="$5"
	scriptDir="$6"
	echo "DEBUG: passed argument for espformat = $espformat"
	echo "DEBUG: passed argument for stage2Loader = $stage2Loader"
	echo "DEBUG: passed argument for selectedDestination = $selectedDestination"
	echo "DEBUG: passed argument for targetDevice = $targetDevice"
	echo "DEBUG: passed argument for targetVolume = $targetVolume"
	echo "DEBUG: passed argument for scriptDir = $scriptDir"
else
	echo "Error - wrong number of values passed"
	exit 9
fi

# check to see if install to EFI system partition was selected
if [ "${selectedDestination}" = "/Volumes/EFI" ]; then
	echo "DEBUG: EFI install chosen"

	if [ ! -d "${selectedDestination}" ]; then
		echo "Executing Command: mkdir -p ${selectedDestination}"
		mkdir -p "${targetVolume}"
	else
		echo "DEBUG: folder /Volumes/EFI already exists"
	fi

	#if the EFI system partition was selected then
	# mount '/Volumes/EFI' with the correct format type

	if [ ${espformat} = 1 ]; then

		echo "Executing command: mount_hfs ${targetDevice} ${targetVolume}"
		mount_hfs "${targetDevice}" "${targetVolume}"
	fi
	if [ ${espformat} = 2 ]; then
		[ -d "${selectedDestination}" ] || mkdir -p "${selectedDestination}"
		echo "Executing command: mount_msdos -u 0 -g 0 ${targetDevice} ${selectedDestination}"
		mount_msdos -u 0 -g 0 "${targetDevice}" "${selectedDestination}"
	fi

	echo "Executing command: cp "${targetVolume}"/usr/standalone/i386/${stage2Loader} ${selectedDestination}"
	cp "${targetVolume}"/usr/standalone/i386/"${stage2Loader}" "${selectedDestination}"
	"$scriptDir"InstallLog.sh "${targetVolume}" "Written boot to ${selectedDestination}."
else
	echo "Executing command: cp "${targetVolume}"/usr/standalone/i386/${stage2Loader} ${targetVolume}"
	cp "${targetVolume}"/usr/standalone/i386/"${stage2Loader}" "${targetVolume}"
	"$scriptDir"InstallLog.sh "${targetVolume}" "Written boot to ${targetVolume}."
fi

#�Check to see if the user wants to hide the boot file
#if [ -f "${selectedDestination}"/.Chameleon/nullhideboot ]; then
#	echo "Executing command: SetFile -a V ${targetVolume}/${stage2Loader}"
#	"${selectedDestination}"/.Chameleon//Tools/SetFile -a V "${targetVolume}"/"${stage2Loader}"
#fi

echo "-----------------------------------------------"
echo ""
echo ""

exit 0