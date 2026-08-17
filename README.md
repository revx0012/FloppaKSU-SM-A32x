# FloppaKSU — KernelSU-Next + SUSFS for Samsung Galaxy A32 5G

Custom kernel for **Samsung Galaxy A32 5G** (`SM-A326(variant)` / `a32x`) with
[KernelSU-Next](https://github.com/KernelSU-Next/KernelSU-Next) root and
[SUSFS v1.5.5](https://gitlab.com/simonpunk/susfs4ksu) root-hiding
integration.

| Property | Value |
|----------|-------|
| Device | Samsung Galaxy A32 5G |
| Model | SM-A326B |
| Chipset | MediaTek MT6853 (Dimensity 720) |
| Android | 13 (in my case) |
| Kernel | 4.14.186-27095505 |
| KernelSU-Next | v3.2.0-legacy |
| SUSFS | v1.5.5 (NON-GKI) |
| Security patch | 2025-02-01 |

## Features

- **KernelSU-Next root** — kernel-level su without Magisk/Zygisk
- **SUSFS v1.5.5** — full root-hiding subsystem compiled into the kernel:
  - `SUS_PATH` — hide paths from `getdents`/`namei`/`dcache`
  - `SUS_MOUNT` — hide KSU mounts from `/proc/*/mounts`, `mountinfo`, `statfs`
  - `SUS_KSTAT` — spoof inode numbers, sizes, timestamps
  - `TRY_UMOUNT` — auto-umount bind mounts for non-root apps
  - `SPOOF_UNAME` — spoof kernel release/version
  - `SPOOF_CMDLINE` — spoof `/proc/cmdline`
  - `OPEN_REDIRECT` — redirect file opens
  - `HIDE_SYMBOLS` — hide `ksu_`/`susfs_` from `/proc/kallsyms`
- **Dual ABI support** — SUSFS commands work via both:
  - **prctl** (original `ksu_susfs` binary compatibility)
  - **ioctl** (native KernelSU-Next anonymous fd interface)
- **Built-in drivers** — Wi-Fi, Bluetooth, GPS, FM radio drivers work just fine! (compiled from in-tree sources)

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

Output is written to `dist/`:
- `FLOPPAKSU-SM-A326B-4.14.186-27095505-AnyKernel3.zip` — flashable ZIP
- `ksu_susfs` — SUSFS userspace tool (ioctl ABI)

## Flashing

1. Back up your original `boot.img`
2. Copy the ZIP to your device
3. Flash via KernelSU-Next Manager app, or via custom recovery (TWRP/OrangeFox)
4. Reboot


**IMPORTANT!!! WHEN FLASHING FOR FIRST TIME, IF IT REBOOTS AGAIN WHEN BOOTING.. DO NOT PANIC! LET IT BOOT! This is completely normal, let it boot again. If it does not boot after waiting for 1 or 2 reboots then assume it is a bootloop and restore from your original boot.img, this may also happen even after flashing successfully but it depends.**  


## SUSFS Usage

### The module (easier)
1. We need to download the .zip from [susfs4ksu](https://github.com/sidex15/susfs4ksu-module/releases).
2. Flash the .zip inside KSU-Next app in the modules tab by pressing a + button.
3. After flashing and rebooting, you should see a checkmark with version being 1.5.5, if not, download a simple file named `ksu_susfs` from releases of this repository and then do the following inside Termux app (must have root access, yes we need termux):

```bash
su
rm /data/adb/ksu/bin/ksu_susfs
mv /sdcard/Download/ksu_susfs /data/adb/ksu/bin/ksu_susfs
chmod 755 /data/adb/ksu/bin/ksu_susfs
```
After doing that, reboot and now it should be fixed. 

### CLI (Advanced)

One binary are provided from release, both work with this kernel:

**New binary (ioctl ABI)** a file from from release named `ksu_susfs`:
```bash
su
rm /data/adb/ksu/bin/ksu_susfs
mv /sdcard/Download/ksu_susfs /data/adb/ksu/bin/ksu_susfs
chmod 755 /data/adb/ksu/bin/ksu_susfs
```

**Commands:**
```bash
# Check version and features
su -c ksu_susfs show version
su -c ksu_susfs show enabled_features
su -c ksu_susfs show variant

# Hide paths
su -c ksu_susfs add_sus_path /data/adb/ksu
su -c ksu_susfs add_sus_path /data/adb/modules

# Hide mounts
su -c ksu_susfs add_sus_mount /data/adb/ksu

# Spoof uname
su -c ksu_susfs set_uname default default

# Configure umount
su -c ksu_susfs add_try_umount /data/adb/ksu 1

# Open redirect
su -c ksu_susfs add_open_redirect /data/adb/ksu /system/bin
```

The original prctl-based `ksu_susfs` binary (from the susfs4ksu module) also
works.

## Architecture

```
Userspace                         Kernel
─────────                         ──────
ksu_susfs (ioctl) ──ioctl()──→  [ksu_driver] fd → dispatch.c → susfs.c
ksu_susfs (prctl) ──prctl()──→  kprobe on sys_prctl → susfs.c
KernelSU Manager ──ioctl()──→   [ksu_driver] fd → dispatch.c
```

The kernel source modifications are minimal and self-contained:
- `fs/susfs/susfs.c` — standalone SUSFS implementation + prctl shim
- `fs/susfs/Kbuild` — builds as a kernel object (not a module)
- `KernelSU-Next/kernel/supercall/dispatch.c` — IOCTL handlers for SUSFS
- VFS hooks across `namei.c`, `namespace.c`, `stat.c`, `readdir.c`, etc.

## Credits

- [KernelSU-Next](https://github.com/KernelSU-Next/KernelSU-Next) — kernel-level root for Android
- [SUSFS4KSU](https://gitlab.com/simonpunk/susfs4ksu) — root-hiding subsystem by simonpunk
- [AnyKernel3](https://github.com/osm0sis/AnyKernel3) — flashable ZIP template
- Samsung open-source kernel release for A32 5G

## License

This project contains the Linux kernel (GPL-2.0) and KernelSU-Next (GPL-2.0).
See `COPYING` for details.
