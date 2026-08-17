#!/system/bin/sh
# floppaksu device diagnostics.
#
# Run as root on the device, then share the floppaksu-diag/ folder:
#   su -c 'sh /sdcard/floppaksu-diag.sh'
# or push this file first:
#   adb push floppaksu-diag.sh /data/local/tmp/
#   adb shell su -c 'sh /data/local/tmp/floppaksu-diag.sh'

DIR=/sdcard/floppaksu-diag
mkdir -p "$DIR"

PAT='version magic|disagrees about version of symbol|wlan|conninfra|wmt|btif|firmware|avc: denied|kernelsu|ksu|KernelSU|su_compat|sucompat|escape_with_root_profile|allowlist|ksud'

log() { echo "[floppaksu] $*"; }

log "saving /proc/version..."
cat /proc/version > "$DIR/version.txt" 2>&1

log "saving dmesg..."
dmesg > "$DIR/dmesg.txt" 2>&1

log "saving /proc/modules..."
cat /proc/modules > "$DIR/modules.txt" 2>&1

log "saving crash evidence (last_kmsg / pstore) if present..."
cat /proc/last_kmsg > "$DIR/last_kmsg.txt" 2>&1
cat /proc/last_kmsg_mtk > "$DIR/last_kmsg_mtk.txt" 2>&1
mkdir -p "$DIR/pstore"
for f in /sys/fs/pstore/* /dev/pstore/*; do
	[ -e "$f" ] || continue
	cat "$f" > "$DIR/pstore/$(basename "$f")" 2>&1
done

log "saving getprop..."
getprop > "$DIR/getprop.txt" 2>&1

if command -v logcat >/dev/null 2>&1; then
	log "saving logcat..."
	logcat -b all -d > "$DIR/logcat.txt" 2>&1
fi

log "matching dmesg lines:"
grep -E "$PAT" "$DIR/dmesg.txt" | head -200

log "loaded modules:"
cat "$DIR/modules.txt"

log "logs saved in $DIR"
ls -l "$DIR"
