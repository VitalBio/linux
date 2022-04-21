#!/usr/bin/env bash
set -euxo pipefail

BOOT="${BOOT:-/run/media/michael/boot}"
ROOT="${ROOT:-/run/media/michael/rootfs}"

sudo cp arch/arm64/boot/dts/overlays/*.dtb* "$BOOT"/overlays/

sudo cp arch/arm64/boot/dts/broadcom/*.dtb "$BOOT"/

sudo cp arch/arm64/boot/Image "$BOOT"/kernel8.img

sudo make INSTALL_MOD_PATH="$ROOT" modules_install

sudo sync

sudo umount /dev/mmcblk0p1 /dev/mmcblk0p2
