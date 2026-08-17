# SM-A326B connectivity compatibility

The first floppaksu test kernel booted and KernelSU worked, but Wi-Fi,
Bluetooth, and other vendor-backed functions failed.

Findings:

- The official CYB7 boot image contains Linux `4.14.186-27095505` built with
  Android Clang `r383902`.
- Its extracted kernel configuration enables `CONFIG_MODVERSIONS=y`.
- CYB7 and this source tree use the same MT6853 connectivity configuration:
  `CONFIG_MTK_COMBO_CHIP_CONSYS_6853`, `CONFIG_MTK_COMBO_BT`,
  `CONFIG_MTK_COMBO_WIFI`, and `CONFIG_MTK_BTIF`.
- The first custom build changed the kernel release to
  `4.14.186-floppaksu`; a later compatibility build used `4.14.186-g7725a014f`
  (a string taken from a custom ROM, not from CYB7). Both broke connectivity.

## Resolution

Connectivity was broken because the drivers were missing, not because of the
release string. Samsung's tree builds the real Wi-Fi/BT/GPS/FM drivers from
external `vendor/mediatek/kernel_modules` sources only when
`CONFIG_WLAN_DRV_BUILD_IN=y`; the stock A32x source ships only adapter stubs,
so the custom kernel had no Wi-Fi or WMT driver at all.

The fix commits the MTK driver sources in-tree under
`drivers/misc/mediatek/connectivity/` (`wlan_drv_gen4m`, `wmt_drv`,
`wmt_chrdev_wifi`, `bt`, `gps`, `fmradio_drv`) and links them into the kernel
as built-in drivers (`export CONFIG_WLAN_DRV_BUILD_IN=y` + `obj-y` in the
connectivity `Makefile`). This matches the approach proven on the A32x custom
kernels (a325g). The build now produces `wlan_6853_axi build-in boot.img`,
`wmt_drv build-in boot.img`, and `wmt_chrdev_wifi build-in boot.img`, and the
release `4.14.186-27095505` is preserved so the remaining stock vendor modules
still load.

## Known limitation: KernelSU app-profile grants

This kernel is built from the legacy `v3.2.0` KernelSU-Next source
(`KSU_APP_PROFILE_VER 4`). The v3.2.0-era manager sends profile version 3 and
is rejected by the kernel (`failed to update app profile` in the manager, and
`Unsupported profile version` in `kernel/policy/allowlist.c`). Install the
official KernelSU Next manager **v3.3.0 (33214) or newer** so per-app root
grants work.

If connectivity still fails, run the bundled helper (saves everything and
greps for the relevant lines):

```sh
su -c 'sh /sdcard/floppaksu-diag.sh'   # after pushing floppaksu-diag.sh
```

or collect manually:

```sh
su -c 'dmesg' > /sdcard/floppaksu-dmesg.txt
su -c 'cat /proc/modules' > /sdcard/floppaksu-modules.txt
logcat -b all -d > /sdcard/floppaksu-logcat.txt
getprop > /sdcard/floppaksu-getprop.txt
```

The most useful errors contain `version magic`, `disagrees about version of
symbol`, `wlan`, `conninfra`, `wmt`, `btif`, `firmware`, or `avc`.
