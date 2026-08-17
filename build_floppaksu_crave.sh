#!/usr/bin/env bash
set -euo pipefail

kernel_root="$(cd "$(dirname "$0")" && pwd)"
toolchain_root="$kernel_root/.crave-toolchains"
clang_dir="$toolchain_root/clang"
gcc_dir="$toolchain_root/aarch64"
out_dir="$kernel_root/out-crave"
dist_dir="$kernel_root/dist"
ak3_dir="$kernel_root/anykernel3"

mkdir -p "$clang_dir" "$gcc_dir" "$dist_dir"

if [[ ! -x "$clang_dir/bin/clang" ]]; then
	curl -fL --retry 3 \
		'https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/android11-release/clang-r383902.tar.gz' \
		| tar -xz -C "$clang_dir"
fi

if [[ ! -x "$gcc_dir/bin/aarch64-linux-android-ld" ]]; then
	curl -fL --retry 3 \
		'https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/+archive/refs/heads/android11-release.tar.gz' \
		| tar -xz -C "$gcc_dir"
fi

export PATH="$clang_dir/bin:$gcc_dir/bin:$PATH"
export ARCH=arm64
export ANDROID_MAJOR_VERSION=t
export KBUILD_BUILD_USER=floppaksu
export KBUILD_BUILD_HOST=floppaksu

common_args=(
	"O=$out_dir"
	ARCH=arm64
	CC=clang
	LD=ld.lld
	AR=llvm-ar
	NM=llvm-nm
	OBJCOPY=llvm-objcopy
	OBJDUMP=llvm-objdump
	STRIP=llvm-strip
	CLANG_TRIPLE=aarch64-linux-gnu-
	CROSS_COMPILE=aarch64-linux-android-
	KCFLAGS=-w
	CONFIG_SECTION_MISMATCH_WARN_ONLY=y
	LOCALVERSION=
)

make -C "$kernel_root" "${common_args[@]}" a32x_defconfig
make -C "$kernel_root" -j"$(nproc)" "${common_args[@]}" Image

release="$(cat "$out_dir/include/config/kernel.release")"
[[ "$release" == '4.14.186-27095505' ]] || {
	printf 'Unexpected kernel release: %s\n' "$release" >&2
	exit 1
}

zip_name="FLOPPAKSU-SM-A326B-$release-AnyKernel3.zip"

install -m 0644 "$out_dir/arch/arm64/boot/Image" "$ak3_dir/Image"

rm -f "$dist_dir/$zip_name"
(cd "$ak3_dir" && zip -q -r -9 "$dist_dir/$zip_name" . -x '.github/*' '.git/*')

sha256sum "$dist_dir/$zip_name" > "$dist_dir/$zip_name.sha256"

printf 'Built %s\n' "$dist_dir/$zip_name"
