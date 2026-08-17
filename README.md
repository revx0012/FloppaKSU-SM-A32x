# KernelSU-Next Build A — Samsung Galaxy A32 5G

KernelSU-Next root for **Samsung Galaxy A32 5G** (`SM-A326B` / `a32x`) on
Android 13. This is **Build A** — the earlier build variant.

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

This repository contains two build variants of the same KernelSU-Next source
for the Samsung Galaxy A32 5G:

- **Build A** (this) — earlier build, `selinux_hide.c` at 13,994 bytes
- **Build B** (`copy2`) — later build, `selinux_hide.c` at 13,840 bytes
  (154-byte reduction, likely a fix or optimization to SELinux hide logic)

The only code difference between the two builds is in
`KernelSU-Next/kernel/feature/selinux_hide.c`. All other kernel source and
configuration files are identical.

## Features

- **KernelSU-Next root** — kernel-level su without Magisk/Zygisk
- **Built-in MTK connectivity** — Wi-Fi, Bluetooth, GPS, FM radio drivers
  compiled from in-tree sources
- **No SUSFS** — this build does not include SUSFS integration

## Building

Requires x86_64 Linux (Ubuntu/Debian). The build script auto-downloads the
Clang + GCC toolchains.

```bash
# Install dependencies
sudo apt update && sudo apt install -y \
  git curl ca-certificates make bc bison flex \
  libssl-dev libelf-dev python3 zip unzip xz-utils

# Clone (with KernelSU-Next submodule)
git clone --recurse-submodules https://github.com/itzlalpekhlua/MizoKSU-SM-A326B.git
cd MizoKSU-SM-A326B

# Build
./build_floppaksu_crave.sh
```

Output is written to `dist/`.

## Flashing

1. Back up your original `boot.img`
2. Copy the ZIP to your device
3. Flash via KernelSU-Next Manager app, or via custom recovery (TWRP/OrangeFox)
4. Reboot

## Credits

- [KernelSU-Next](https://github.com/KernelSU-Next/KernelSU-Next) — kernel-level root for Android
- [AnyKernel3](https://github.com/osm0sis/AnyKernel3) — flashable ZIP template
- Samsung open-source kernel release for SM-A326B

## License

This project contains the Linux kernel (GPL-2.0) and KernelSU-Next (GPL-2.0).
See `COPYING` for details.
