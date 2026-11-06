# Embedded Linux - LAB2

## Contents
- driver/: Character device driver source, Makefile and kernel module.
- initramfs/: LAB2 initramfs image.
- rootfs_config/: BusyBox init configuration.

## Driver features
- Character device registration.
- Read and write operations.
- Mutex protection.
- Sysfs attributes.
- Procfs information at /proc/lab2_info.
- Module initialization and cleanup.

## Boot configuration
- /etc/inittab executes /etc/init.d/rcS.
- rcS loads lab2_driver.ko using insmod.
- Device node: /dev/lab2

## Archive validation
The initramfs archive was checked using gzip and cpio.
The archive contains lab2_driver.ko, BusyBox, init, inittab and rcS.
