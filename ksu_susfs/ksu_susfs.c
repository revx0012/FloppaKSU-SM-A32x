#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <errno.h>
#include <stdint.h>

#define KSU_INSTALL_MAGIC1 0xDEADBEEF
#define KSU_INSTALL_MAGIC2 0xCAFEBABE

#define KSU_IOCTL_SUSFS_ADD_SUS_PATH      _IOC(_IOC_WRITE, 'K', 50, 0)
#define KSU_IOCTL_SUSFS_ADD_SUS_MOUNT     _IOC(_IOC_WRITE, 'K', 51, 0)
#define KSU_IOCTL_SUSFS_ADD_SUS_KSTAT     _IOC(_IOC_WRITE, 'K', 52, 0)
#define KSU_IOCTL_SUSFS_UPDATE_SUS_KSTAT  _IOC(_IOC_WRITE, 'K', 53, 0)
#define KSU_IOCTL_SUSFS_ADD_TRY_UMOUNT    _IOC(_IOC_WRITE, 'K', 54, 0)
#define KSU_IOCTL_SUSFS_SET_UNAME         _IOC(_IOC_WRITE, 'K', 55, 0)
#define KSU_IOCTL_SUSFS_ENABLE_LOG        _IOC(_IOC_WRITE, 'K', 56, 0)
#define KSU_IOCTL_SUSFS_SET_CMDLINE       _IOC(_IOC_WRITE, 'K', 57, 0)
#define KSU_IOCTL_SUSFS_ADD_OPEN_REDIRECT _IOC(_IOC_WRITE, 'K', 58, 0)
#define KSU_IOCTL_SUSFS_SHOW_VERSION      _IOC(_IOC_READ,  'K', 60, 0)
#define KSU_IOCTL_SUSFS_SHOW_FEATURES     _IOC(_IOC_READ,  'K', 61, 0)
#define KSU_IOCTL_SUSFS_SHOW_VARIANT      _IOC(_IOC_READ,  'K', 62, 0)
#define KSU_IOCTL_SUSFS_ADD_SUS_KSTAT_STATICALLY _IOC(_IOC_WRITE, 'K', 63, 0)
#define KSU_IOCTL_SUSFS_RUN_UMOUNT        _IOC(_IOC_WRITE, 'K', 64, 0)

#define SUSFS_MAX_LEN_PATHNAME 256
#define TRY_UMOUNT_DEFAULT 0
#define TRY_UMOUNT_DETACH 1

struct st_susfs_sus_path {
	unsigned long target_ino;
	char target_pathname[SUSFS_MAX_LEN_PATHNAME];
};

struct st_susfs_sus_mount {
	char target_pathname[SUSFS_MAX_LEN_PATHNAME];
	unsigned long target_dev;
};

struct st_susfs_sus_kstat {
	int is_statically;
	unsigned long target_ino;
	char target_pathname[SUSFS_MAX_LEN_PATHNAME];
	unsigned long spoofed_ino;
	unsigned long spoofed_dev;
	unsigned int spoofed_nlink;
	long long spoofed_size;
	long spoofed_atime_tv_sec;
	long spoofed_mtime_tv_sec;
	long spoofed_ctime_tv_sec;
	long spoofed_atime_tv_nsec;
	long spoofed_mtime_tv_nsec;
	long spoofed_ctime_tv_nsec;
	unsigned long spoofed_blksize;
	unsigned long long spoofed_blocks;
};

struct st_susfs_try_umount {
	char target_pathname[SUSFS_MAX_LEN_PATHNAME];
	int mnt_mode;
};

struct st_susfs_uname {
	char release[65];
	char version[65];
};

struct st_susfs_open_redirect {
	unsigned long target_ino;
	char target_pathname[SUSFS_MAX_LEN_PATHNAME];
	char redirected_pathname[SUSFS_MAX_LEN_PATHNAME];
};

static int ksu_fd = -1;

static int get_ksu_fd(void)
{
	int fd_slot = -1;
	/*
	 * The real sys_reboot returns -EINVAL for our magic numbers,
	 * but the kprobe pre_handler fires first and installs the fd
	 * via copy_to_user before the real syscall body runs.
	 * We must ignore the syscall return value and just use fd_slot.
	 */
	syscall(__NR_reboot, KSU_INSTALL_MAGIC1, KSU_INSTALL_MAGIC2, 0, &fd_slot);
	return fd_slot;
}

static int susfs_ioctl(unsigned int cmd, void *data)
{
	return ioctl(ksu_fd, cmd, data);
}

static void print_usage(const char *prog)
{
	printf("KernelSU-Next SUSFS Tool (ioctl ABI)\n\n");
	printf("Usage: %s <command> [args]\n\n", prog);
	printf("Show commands:\n");
	printf("  show version                  Show SUSFS version\n");
	printf("  show enabled_features         Show enabled feature bitmask\n");
	printf("  show variant                  Show GKI/NON-GKI variant\n\n");
	printf("SUS path commands:\n");
	printf("  add_sus_path <path>           Hide path from getdents/namei\n\n");
	printf("SUS mount commands:\n");
	printf("  add_sus_mount <path>          Hide mount from /proc/mounts\n\n");
	printf("SUS kstat commands:\n");
	printf("  add_sus_kstat <path> [ino dev nlink size atime mtime ctime atime_nsec mtime_nsec ctime_nsec blksize blocks]\n");
	printf("  update_sus_kstat <path> [same fields as above]\n\n");
	printf("Try umount commands:\n");
	printf("  add_try_umount <path> <mode>  mode: 0=default, 1=detach\n");
	printf("  run_try_umount                Execute pending umounts\n\n");
	printf("Spoof commands:\n");
	printf("  set_uname <release> <version> Spoof kernel uname\n");
	printf("  set_cmdline_or_bootconfig <file>  Spoof /proc/cmdline\n\n");
	printf("Open redirect:\n");
	printf("  add_open_redirect <target_path> <redirected_path>\n\n");
	printf("Logging:\n");
	printf("  enable_log <0|1>              Toggle SUSFS logging\n\n");
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		print_usage(argv[0]);
		return 1;
	}

	ksu_fd = get_ksu_fd();
	if (ksu_fd < 0) {
		fprintf(stderr, "[-] Failed to get KernelSU fd (fd_slot=%d). Is KernelSU-Next running?\n", ksu_fd);
		fprintf(stderr, "[-] If KernelSU-Next is running, the reboot kprobe may not be intercepting this process.\n");
		return 1;
	}

	const char *cmd = argv[1];

	if (strcmp(cmd, "show") == 0 && argc >= 3) {
		const char *sub = argv[2];

		if (strcmp(sub, "version") == 0) {
			char version[65] = {0};
			if (susfs_ioctl(KSU_IOCTL_SUSFS_SHOW_VERSION, version) == 0) {
				printf("[+] Kernel: susfs %s (ABI: ioctl)\n", version);
			} else {
				fprintf(stderr, "[-] Failed to get version: %s\n", strerror(errno));
				return 1;
			}
		} else if (strcmp(sub, "enabled_features") == 0) {
			uint64_t features = 0;
			if (susfs_ioctl(KSU_IOCTL_SUSFS_SHOW_FEATURES, &features) == 0) {
				printf("[+] Enabled features: 0x%lx\n", (unsigned long)features);
				if (features & (1 << 0)) printf("  [+] SUS_PATH\n");
				if (features & (1 << 1)) printf("  [+] SUS_MOUNT\n");
				if (features & (1 << 2)) printf("  [+] SUS_KSTAT\n");
				if (features & (1 << 3)) printf("  [+] TRY_UMOUNT\n");
				if (features & (1 << 4)) printf("  [+] SPOOF_UNAME\n");
				if (features & (1 << 5)) printf("  [+] SPOOF_CMDLINE\n");
				if (features & (1 << 6)) printf("  [+] OPEN_REDIRECT\n");
				if (features & (1 << 7)) printf("  [+] HIDE_KSU_SUSFS_SYMBOLS\n");
			} else {
				fprintf(stderr, "[-] Failed to get features: %s\n", strerror(errno));
				return 1;
			}
		} else if (strcmp(sub, "variant") == 0) {
			char variant[65] = {0};
			if (susfs_ioctl(KSU_IOCTL_SUSFS_SHOW_VARIANT, variant) == 0) {
				printf("[+] Variant: %s\n", variant);
			} else {
				fprintf(stderr, "[-] Failed to get variant: %s\n", strerror(errno));
				return 1;
			}
		} else {
			fprintf(stderr, "[-] Unknown show command: %s\n", sub);
			return 1;
		}
	} else if (strcmp(cmd, "add_sus_path") == 0) {
		if (argc < 3) {
			fprintf(stderr, "[-] Usage: %s add_sus_path <path>\n", argv[0]);
			return 1;
		}
		struct st_susfs_sus_path sp = {0};
		strncpy(sp.target_pathname, argv[2], SUSFS_MAX_LEN_PATHNAME - 1);
		if (susfs_ioctl(KSU_IOCTL_SUSFS_ADD_SUS_PATH, &sp) == 0) {
			printf("[+] Added sus_path: %s\n", argv[2]);
		} else {
			fprintf(stderr, "[-] Failed to add sus_path: %s\n", strerror(errno));
			return 1;
		}
	} else if (strcmp(cmd, "add_sus_mount") == 0) {
		if (argc < 3) {
			fprintf(stderr, "[-] Usage: %s add_sus_mount <path>\n", argv[0]);
			return 1;
		}
		struct st_susfs_sus_mount sm = {0};
		strncpy(sm.target_pathname, argv[2], SUSFS_MAX_LEN_PATHNAME - 1);
		if (susfs_ioctl(KSU_IOCTL_SUSFS_ADD_SUS_MOUNT, &sm) == 0) {
			printf("[+] Added sus_mount: %s\n", argv[2]);
		} else {
			fprintf(stderr, "[-] Failed to add sus_mount: %s\n", strerror(errno));
			return 1;
		}
	} else if (strcmp(cmd, "add_sus_kstat") == 0 || strcmp(cmd, "update_sus_kstat") == 0 || strcmp(cmd, "add_sus_kstat_statically") == 0) {
		if (argc < 3) {
			fprintf(stderr, "[-] Usage: %s %s <path> [ino dev nlink size atime mtime ctime atime_nsec mtime_nsec ctime_nsec blksize blocks]\n", argv[0], cmd);
			return 1;
		}
		struct st_susfs_sus_kstat sk = {0};
		strncpy(sk.target_pathname, argv[2], SUSFS_MAX_LEN_PATHNAME - 1);
		sk.is_statically = (strcmp(cmd, "add_sus_kstat_statically") == 0) ? 1 : 0;
		if (argc > 3) sk.target_ino = strtoul(argv[3], NULL, 0);
		if (argc > 4) sk.spoofed_ino = strtoul(argv[4], NULL, 0);
		if (argc > 5) sk.spoofed_dev = strtoul(argv[5], NULL, 0);
		if (argc > 6) sk.spoofed_nlink = strtoul(argv[6], NULL, 0);
		if (argc > 7) sk.spoofed_size = strtoll(argv[7], NULL, 0);
		if (argc > 8) sk.spoofed_atime_tv_sec = strtol(argv[8], NULL, 0);
		if (argc > 9) sk.spoofed_mtime_tv_sec = strtol(argv[9], NULL, 0);
		if (argc > 10) sk.spoofed_ctime_tv_sec = strtol(argv[10], NULL, 0);
		if (argc > 11) sk.spoofed_atime_tv_nsec = strtol(argv[11], NULL, 0);
		if (argc > 12) sk.spoofed_mtime_tv_nsec = strtol(argv[12], NULL, 0);
		if (argc > 13) sk.spoofed_ctime_tv_nsec = strtol(argv[13], NULL, 0);
		if (argc > 14) sk.spoofed_blksize = strtoul(argv[14], NULL, 0);
		if (argc > 15) sk.spoofed_blocks = strtoull(argv[15], NULL, 0);

		unsigned int iocmd = (strcmp(cmd, "update_sus_kstat") == 0) ?
			KSU_IOCTL_SUSFS_UPDATE_SUS_KSTAT :
			(sk.is_statically ? KSU_IOCTL_SUSFS_ADD_SUS_KSTAT_STATICALLY : KSU_IOCTL_SUSFS_ADD_SUS_KSTAT);

		if (susfs_ioctl(iocmd, &sk) == 0) {
			printf("[+] %s: %s\n", cmd, argv[2]);
		} else {
			fprintf(stderr, "[-] Failed: %s: %s\n", cmd, strerror(errno));
			return 1;
		}
	} else if (strcmp(cmd, "add_try_umount") == 0) {
		if (argc < 4) {
			fprintf(stderr, "[-] Usage: %s add_try_umount <path> <mode(0|1)>\n", argv[0]);
			return 1;
		}
		struct st_susfs_try_umount tu = {0};
		strncpy(tu.target_pathname, argv[2], SUSFS_MAX_LEN_PATHNAME - 1);
		tu.mnt_mode = atoi(argv[3]);
		if (susfs_ioctl(KSU_IOCTL_SUSFS_ADD_TRY_UMOUNT, &tu) == 0) {
			printf("[+] Added try_umount: %s (mode=%d)\n", argv[2], tu.mnt_mode);
		} else {
			fprintf(stderr, "[-] Failed to add try_umount: %s\n", strerror(errno));
			return 1;
		}
	} else if (strcmp(cmd, "run_try_umount") == 0) {
		if (susfs_ioctl(KSU_IOCTL_SUSFS_RUN_UMOUNT, NULL) == 0) {
			printf("[+] Ran try_umount for current mnt namespace\n");
		} else {
			fprintf(stderr, "[-] Failed to run try_umount: %s\n", strerror(errno));
			return 1;
		}
	} else if (strcmp(cmd, "set_uname") == 0) {
		if (argc < 4) {
			fprintf(stderr, "[-] Usage: %s set_uname <release> <version>\n", argv[0]);
			return 1;
		}
		struct st_susfs_uname un = {0};
		strncpy(un.release, argv[2], 64);
		strncpy(un.version, argv[3], 64);
		if (susfs_ioctl(KSU_IOCTL_SUSFS_SET_UNAME, &un) == 0) {
			printf("[+] Set uname: release='%s' version='%s'\n", un.release, un.version);
		} else {
			fprintf(stderr, "[-] Failed to set uname: %s\n", strerror(errno));
			return 1;
		}
	} else if (strcmp(cmd, "set_cmdline_or_bootconfig") == 0) {
		if (argc < 3) {
			fprintf(stderr, "[-] Usage: %s set_cmdline_or_bootconfig <file_or_string>\n", argv[0]);
			return 1;
		}
		char cmdline[4096] = {0};
		FILE *f = fopen(argv[2], "r");
		if (f) {
			size_t n = fread(cmdline, 1, sizeof(cmdline) - 1, f);
			fclose(f);
			if (n > 0 && cmdline[n-1] == '\n') cmdline[n-1] = '\0';
		} else {
			strncpy(cmdline, argv[2], sizeof(cmdline) - 1);
		}
		if (susfs_ioctl(KSU_IOCTL_SUSFS_SET_CMDLINE, cmdline) == 0) {
			printf("[+] Set cmdline/bootconfig\n");
		} else {
			fprintf(stderr, "[-] Failed to set cmdline: %s\n", strerror(errno));
			return 1;
		}
	} else if (strcmp(cmd, "add_open_redirect") == 0) {
		if (argc < 4) {
			fprintf(stderr, "[-] Usage: %s add_open_redirect <target_path> <redirected_path>\n", argv[0]);
			return 1;
		}
		struct st_susfs_open_redirect orr = {0};
		strncpy(orr.target_pathname, argv[2], SUSFS_MAX_LEN_PATHNAME - 1);
		strncpy(orr.redirected_pathname, argv[3], SUSFS_MAX_LEN_PATHNAME - 1);
		if (susfs_ioctl(KSU_IOCTL_SUSFS_ADD_OPEN_REDIRECT, &orr) == 0) {
			printf("[+] Added open_redirect: %s -> %s\n", argv[2], argv[3]);
		} else {
			fprintf(stderr, "[-] Failed to add open_redirect: %s\n", strerror(errno));
			return 1;
		}
	} else if (strcmp(cmd, "enable_log") == 0) {
		if (argc < 3) {
			fprintf(stderr, "[-] Usage: %s enable_log <0|1>\n", argv[0]);
			return 1;
		}
		uint8_t enabled = atoi(argv[2]) ? 1 : 0;
		if (susfs_ioctl(KSU_IOCTL_SUSFS_ENABLE_LOG, &enabled) == 0) {
			printf("[+] Logging %s\n", enabled ? "enabled" : "disabled");
		} else {
			fprintf(stderr, "[-] Failed to set log: %s\n", strerror(errno));
			return 1;
		}
	} else {
		fprintf(stderr, "[-] Unknown command: %s\n", cmd);
		print_usage(argv[0]);
		close(ksu_fd);
		return 1;
	}

	close(ksu_fd);
	return 0;
}
