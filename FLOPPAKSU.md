# floppaksu for Samsung Galaxy A32 5G (SM-A326B)

KernelSU Next integration for the Samsung Galaxy A32 5G (`a32x`, SM-A326B),
targeting Android 13 firmware `A326BXXSECYB7`.

## Build information

- Kernel release: `4.14.186-27095505`
- Build identity: `floppaksu@floppaksu`
- Base source branch: `A326BXUU9CWL1`
- Base commit: `e35042c7322329792b9a41153c6540e79236d213`
- KernelSU Next: `v3.2.0-legacy`
- KernelSU commit: `a54e4fa46c6cc25bcaa055cf14d790194beffed8`
- Integration mode: legacy non-GKI manual hooks
- Connectivity drivers: MTK Wi-Fi/BT/GPS/FM built into the kernel
  (`CONFIG_WLAN_DRV_BUILD_IN=y`, drivers compiled in-tree under
  `drivers/misc/mediatek/connectivity/`)
- Compiler: Android Clang `r383902` with AArch64 GCC 4.9 cross tools

The public Samsung source used here is from older Android 13 firmware than
`A326BXXSECYB7`. The CYB7 stock kernel configuration was extracted and
compared with this tree; its connectivity options match. The stock tree only
ships adapter stubs: the real Wi-Fi (`wlan_drv_gen4m`), WMT (`wmt_drv`), Wi-Fi
character device (`wmt_chrdev_wifi`), Bluetooth, GPS, and FM radio drivers live
in the external `vendor/mediatek/kernel_modules` sources. These were committed
in-tree and linked into the kernel as built-in drivers, matching the approach
used by the A32x custom-kernel community (a325g). The compatibility build
preserves the stock CYB7 release `4.14.186-27095505` exactly because Samsung
enables `CONFIG_MODVERSIONS`; keeping the release string also keeps the stock
vendor modules loadable. Always back up the original boot image and treat the
result as experimental until tested on physical hardware.

## Clone

```sh
git clone --recurse-submodules <repository-url>
cd android_kernel_samsung_a32x
ln -s ../KernelSU-Next/kernel drivers/kernelsu
```

## Build

Install Android Clang r383902 and the AArch64 Android GCC 4.9 cross tools,
then place their `bin` directories first in `PATH`:

```sh
export ARCH=arm64
export ANDROID_MAJOR_VERSION=t
export KBUILD_BUILD_USER=floppaksu
export KBUILD_BUILD_HOST=floppaksu

make O=out ARCH=arm64 CC=clang \
  CLANG_TRIPLE=aarch64-linux-gnu- \
  CROSS_COMPILE=aarch64-linux-android- \
  a32x_defconfig

make O=out -j"$(nproc)" ARCH=arm64 \
  CC=clang LD=ld.lld AR=llvm-ar NM=llvm-nm \
  OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip \
  CLANG_TRIPLE=aarch64-linux-gnu- \
  CROSS_COMPILE=aarch64-linux-android- \
  KCFLAGS=-w CONFIG_SECTION_MISMATCH_WARN_ONLY=y \
  LOCALVERSION= Image
```

The resulting kernel is `out/arch/arm64/boot/Image`. For a reproducible remote
build on Crave, run `build_floppaksu_crave.sh`; it writes the flashable
AnyKernel3 ZIP and its SHA-256 checksum under `dist/`.

## Flashing

Use the release ZIP only on an unlocked SM-A326B running the stated Android
13 firmware. The AnyKernel3 installer preserves the installed boot ramdisk
and metadata, replaces the kernel, repacks the boot image, and flashes it.

Install the official [KernelSU Next manager](https://github.com/KernelSU-Next/KernelSU-Next/releases)
(`KernelSU_Next_...-release.apk`). Use **v3.3.0 (33214) or newer**: this kernel
is built from the legacy `v3.2.0` KernelSU-Next source, whose app-profile ABI
is `KSU_APP_PROFILE_VER 4`; the v3.2.0-era manager sends profile version 3 and
is rejected by the kernel (`failed to update app profile`), while the v3.3.0
manager sends version 4 with a byte-identical struct, so per-app root grants
work. The kernel verifies the manager's v2 signing certificate; the official
release APKs match the compiled-in certificate hash and size, so no custom
manager is needed.

## Using root / su

The kernel force-enables the legacy `su_compat` path
(`kernel/feature/sucompat.c` ignores any request to disable it), so any app
added to the manager's allowlist can run `su` even though the legacy `su_compat`
switch exists only for the stock manager behavior. After adding an app to the
allowlist, force-stop the app and reopen it — grants are applied at process
exec time, so a running process cannot gain root.

## Diagnostics

Run `floppaksu-diag.sh` (as root) to capture `dmesg`, `logcat`, `/proc/version`,
`/proc/modules`, and `getprop` into `/sdcard/floppaksu-diag/`:

```sh
su -c 'sh /sdcard/floppaksu-diag.sh'   # after pushing floppaksu-diag.sh
```

## Credits and licensing

- Samsung and MediaTek kernel contributors
- [KernelSU Next](https://github.com/KernelSU-Next/KernelSU-Next)
- [AnyKernel3](https://github.com/osm0sis/AnyKernel3)
- Original source mirror: [Potakilain/android_kernel_samsung_a32x](https://github.com/Potakilain/android_kernel_samsung_a32x)

Kernel source changes remain subject to GPL-2.0. See `COPYING`.
