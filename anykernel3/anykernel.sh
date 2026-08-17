### AnyKernel3 Ramdisk Mod Script
## osm0sis @ xda-developers

### AnyKernel setup
# global properties
properties() { '
kernel.string=floppaksu for Samsung SM-A326B (a32x) | 4.14.186-27095505
do.devicecheck=0
do.modules=0
do.systemless=0
do.cleanup=1
do.cleanuponabort=0
device.name1=
device.name2=
device.name3=
device.name4=
device.name5=
supported.versions=
supported.patchlevels=
supported.vendorpatchlevels=
'; } # end properties


### AnyKernel install
## boot files attributes
boot_attributes() {
set_perm_recursive 0 0 755 644 $RAMDISK/*;
set_perm_recursive 0 0 750 750 $RAMDISK/init* $RAMDISK/sbin;
} # end attributes

# boot shell variables
BLOCK=boot;
IS_SLOT_DEVICE=0;
RAMDISK_COMPRESSION=auto;
PATCH_VBMETA_FLAG=auto;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh;

# --- floppaksu flash debug ---
ui_print " ";
ui_print "floppaksu installer";
ui_print "device: Samsung SM-A326B (a32x)";
KVER=$(strings "$AKHOME/Image" 2>/dev/null | grep -m1 "Linux version");
KVER=${KVER:-$(strings "$AKHOME/Image" 2>/dev/null | grep -m1 "4.14.186")};
ui_print "kernel: ${KVER:-Image}";
[ "$(strings "$AKHOME/Image" 2>/dev/null | grep -m1 floppaksu)" ] && ui_print "build id: floppaksu@floppaksu";
ui_print "Image size: $(wc -c < "$AKHOME/Image" 2>/dev/null) bytes";
ui_print "boot partition: $BLOCK$SLOT";
ui_print " ";

# boot install
ui_print "Step 1/3: unpacking existing boot image...";
dump_boot; # use split_boot to skip ramdisk unpack, e.g. for devices with init_boot ramdisk

ui_print "Step 2/3: replacing kernel, existing DTB/DTBO and ramdisk preserved...";
ui_print "Step 3/3: repacking and writing boot image...";
write_boot; # use flash_boot to skip ramdisk repack, e.g. for devices with init_boot ramdisk

ui_print " ";
ui_print "floppaksu install complete!";
ui_print "Reboot now to boot the new kernel.";

# best-effort install log (never aborts the flash)
LOG=/tmp/floppaksu_flash.log;
[ -d /data/local/tmp ] && LOG=/data/local/tmp/floppaksu_flash.log;
{
  echo "floppaksu flash log";
  echo "date: $(date)";
  echo "kernel: ${KVER:-Image}";
  echo "build id: floppaksu@floppaksu";
  echo "image size: $(wc -c < "$AKHOME/Image" 2>/dev/null) bytes";
  echo "boot block: $BLOCK$SLOT";
} > "$LOG" 2>/dev/null && ui_print "install log saved: $LOG";
## end boot install
