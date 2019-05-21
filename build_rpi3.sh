case $1 in
	all)
	make ARCH=arm CROSS_COMPILE=../tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf- bcm2709_defconfig
	make ARCH=arm CROSS_COMPILE=../tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf- zImage modules dtbs -j8
	sudo scripts/mkknlimg arch/arm/boot/zImage arch/arm/boot/new_kernel.img
	;;
	dtbs)
	make ARCH=arm CROSS_COMPILE=../tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf- bcm2709_defconfig
	make ARCH=arm CROSS_COMPILE=../tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf- dtbs -j8
	;;
	modules_install)
	if [ -d "/mnt/rpi_root/" ];then
		make ARCH=arm CROSS_COMPILE=../tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf- INSTALL_MOD_PATH=/mnt/rpi_root modules_install
	else
		echo "You need mount the sdcard on /mnt/rpi_root/"
	fi
	;;
esac

