#!/usr/bin/env bash
set -euxo pipefail

sudo cp arch/arm64/boot/dts/overlays/*.dtb* /run/media/michael/boot/overlays/

sudo cp arch/arm64/boot/dts/broadcom/*.dtb /run/media/michael/boot/

sudo cp arch/arm64/boot/Image /run/media/michael/boot/kernel8.img

sudo make INSTALL_MOD_PATH=/run/media/michael/rootfs modules_install

sudo sync

sudo umount /dev/mmcblk0p1 /dev/mmcblk0p2
