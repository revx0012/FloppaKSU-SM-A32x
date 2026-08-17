# KernelSU-Next Variant A — Samsung Galaxy A32 5G

KernelSU-Next root for **Samsung Galaxy A32 5G** (`SM-A326(variant) / a32x`). This is **ksu1 branch**, the earlier build variant.

| Property | Value |
|----------|-------|
| Device | Samsung Galaxy A32 5G |
| Model | SM-A326B |
| Chipset | MediaTek MT6853 (Dimensity 720) |
| Android | 13 (SDK 33) |
| Kernel | 4.14.186-27095505 |
| KernelSU-Next | v3.2.0-legacy |
| Build variant | A (earlier) |

## Build Variants

This repository contains three build variants of the same KernelSU-Next source
for the Samsung Galaxy A32 5G:

- **Variant 1** (this) — earlier build, selinux not modified.
- **Variant 2** (`ksu2`) — later build, selinux modified. (some adjusts to SELinux hide logic)
- **Variant SUSFS** (`ksu-susfs`) KernelSU-Next with susfs implemented.


The only code difference between the two builds is in
`KernelSU-Next/kernel/feature/selinux_hide.c`. All other kernel source and
configuration files are identical.

## Features

- **KernelSU-Next root** — kernel-level su without Magisk/Zygisk
- **Built-in drivers** — Wi-Fi, Bluetooth, GPS, FM radio drivers work just fine! (compiled from in-tree sources)
- **No SUSFS** — this build does not include SUSFS integration

## Building

Requires x86_64 Linux (Ubuntu/Debian). The build script auto-downloads the
Clang + GCC toolchains.

```bash
# Install dependencies
sudo apt update && sudo apt install -y \
  git curl ca-certificates make bc bison flex \
  libssl-dev libelf-dev python3 zip unzip xz-utils

# Clone
git clone https://github.com/revx0012/FloppaKSU-SM-A32x
cd FloppaKSU-SM-A32x

# Build
./build_floppaksu_crave.sh
```

Output is written to `dist/`.

## Flashing

1. Back up your original `boot.img`
2. Copy the ZIP to your device
3. Flash via KernelSU-Next Manager app, or via custom recovery (TWRP/OrangeFox)
4. Reboot

**IMPORTANT!!! WHEN FLASHING FOR FIRST TIME, IF IT REBOOTS AGAIN WHEN BOOTING.. DO NOT PANIC! LET IT BOOT! This is completely normal, let it boot again. If it does not boot after waiting for 1 or 2 reboots then assume it is a bootloop and restore from your original boot.img, this may also happen even after flashing successfully but it depends.**  

## Credits

- [KernelSU-Next](https://github.com/KernelSU-Next/KernelSU-Next) — kernel-level root for Android
- [AnyKernel3](https://github.com/osm0sis/AnyKernel3) — flashable ZIP template
- Samsung open-source kernel release for A32 5G

## License

This project contains the Linux kernel (GPL-2.0) and KernelSU-Next (GPL-2.0).
See `COPYING` for details.
