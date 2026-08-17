#include <linux/fs.h>
#include <linux/jump_label.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
#include <asm/set_memory.h>
#else
#include <asm/cacheflush.h>
#endif
#include <linux/namei.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include "policy/feature.h"
#include "include/ksu.h"
#include  "uapi/feature.h"
#include "selinux/selinux.h"
#include "feature/selinux_hide.h"

#if defined(CONFIG_KSU_KPROBES_HOOK)
extern struct kprobe *init_kprobe(const char *name, int (*pre_handler)(struct kprobe *, struct pt_regs *));
extern void destroy_kprobe(struct kprobe **kp_ptr);
extern int slow_avc_audit_pre_handler(struct kprobe *p, struct pt_regs *regs);
extern struct kprobe *slow_avc_audit_kp;
#endif

static struct page *fake_status = NULL;
static DEFINE_MUTEX(fake_status_init_mutex);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
extern bool ksu_input_hook __read_mostly __attribute__((weak));
#else
extern bool ksu_input_hook __read_mostly;
#endif
extern struct selinux_state selinux_state;

// enabled by default
static bool ksu_selinux_hide_is_enabled __read_mostly = true;

static u32 ksu_sid __read_mostly = 0;
static u32 priv_app_sid __read_mostly = 0;

static int ksu_selinux_get_sids(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
	int err1 = security_context_to_sid("u:r:ksu:s0", strlen("u:r:ksu:s0"), &ksu_sid, GFP_KERNEL);
    int err2 = security_context_to_sid("u:r:priv_app:s0:c512,c768", 
                                       strlen("u:r:priv_app:s0:c512,c768"), &priv_app_sid, GFP_KERNEL);
#else
	int err1 = security_secctx_to_secid("u:r:ksu:s0", strlen("u:r:ksu:s0"), &ksu_sid);
	int err2 = security_secctx_to_secid("u:r:priv_app:s0:c512,c768",
					     strlen("u:r:priv_app:s0:c512,c768"), &priv_app_sid);
#endif
	if (!err1) pr_info("ksu_selinux_hide: ksu_sid=%u\n", ksu_sid);
	if (!err2) pr_info("ksu_selinux_hide: priv_app_sid=%u\n", priv_app_sid);
	return (!ksu_sid || !priv_app_sid) ? -1 : 0;
}

static void ksu_selinux_hide_enable(void)
{
	if (ksu_selinux_get_sids())
		pr_warn("ksu_selinux_hide: sid grab failed\n");
#if defined(CONFIG_KSU_KPROBES_HOOK)
	slow_avc_audit_kp = init_kprobe("slow_avc_audit", slow_avc_audit_pre_handler);
#endif
}

static void ksu_selinux_hide_disable(void)
{
#if defined(CONFIG_KSU_KPROBES_HOOK)
	destroy_kprobe(&slow_avc_audit_kp);
#endif
}

static void initialize_fake_status(void)
{
	if (READ_ONCE(fake_status))
		return;

	mutex_lock(&fake_status_init_mutex);
	if (fake_status) /* double-check after lock */
		goto out;

#ifdef KSU_COMPAT_USE_SELINUX_STATE
	struct page *real_page = selinux_kernel_status_page(&selinux_state);
#else
	struct page *real_page = selinux_kernel_status_page();
#endif
	if (!real_page) {
		pr_warn("ksu_selinux_hide: status_page not exists\n");
		goto out;
	}

	struct selinux_kernel_status *status = page_address(real_page);
	if (!status->enforcing && !ksu_late_loaded) {
		pr_warn("ksu_selinux_hide: skip not enforcing\n");
		goto out;
	}

	struct page *new_page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (!new_page) {
		pr_err("ksu_selinux_hide: failed to allocate fake status page\n");
		goto out;
	}

	struct selinux_kernel_status *new_status = page_address(new_page);
	memcpy(new_status, status, sizeof(*status));
	if (ksu_late_loaded && !new_status->enforcing) {
		/*
		 * In late_load mode we may be loaded after setenforce 0.
		 * Adjust sequence to look like a normal enforcing boot.
		 * Assumes setenforce 0 was called exactly once.
		 */
		new_status->enforcing = 1;
		new_status->sequence = 4;
	}
	
	WRITE_ONCE(fake_status, new_page);
	pr_info("ksu_selinux_hide: fake status ready: sequence=%d policyload=%d enforcing=%d\n",
		new_status->sequence, new_status->policyload,
		new_status->enforcing);
out:
	mutex_unlock(&fake_status_init_mutex);
}

typedef int (*sel_open_handle_status_fn)(struct inode *inode,
					 struct file *filp);
static sel_open_handle_status_fn orig_sel_open_handle_status = NULL;

static int __nocfi my_sel_open_handle_status(struct inode *inode, struct file *filp)
{
	if (likely(test_thread_flag(TIF_SECCOMP) &&
	current_uid().val >= 10000 &&
		   ksu_selinux_hide_is_enabled)) {
		struct page *data = READ_ONCE(fake_status);
		if (data) {
			filp->private_data = data;
			return 0;
		}
	}

	return orig_sel_open_handle_status(inode, filp);
}

#define FORCE_VOLATILE(x) *(volatile typeof(x) *)&(x)

static int patch_fops_slot(unsigned long addr, void *new_fn)
{
	unsigned long base = addr & PAGE_MASK;
	unsigned long offset = addr & ~PAGE_MASK;

	struct page *page = phys_to_page(__pa(base));
	if (!page)
		return -EFAULT;

	void *writable_addr = vmap(&page, 1, VM_MAP, PAGE_KERNEL);
	if (!writable_addr)
		return -ENOMEM;

	void **target_slot = (void **)((unsigned long)writable_addr + offset);

	preempt_disable();
	local_irq_disable();
	FORCE_VOLATILE(*target_slot) = (void *)new_fn;
	local_irq_enable();
	preempt_enable();

	vunmap(writable_addr);
	smp_mb();
	return 0;
}

static int patch_fops_open(struct file_operations *ops,
			    sel_open_handle_status_fn new_open)
{
	if (!ops || !new_open)
		return -EINVAL;
	return patch_fops_slot((unsigned long)&ops->open, (void *)new_open);
}

static int resolve_path_info(const char *path_str, ino_t *ino_out,
			     struct file_operations **fops_out)
{
	struct path path;
	struct inode *inode;
	int error = kern_path(path_str, LOOKUP_FOLLOW, &path);
	if (error) {
		pr_err("ksu_selinux_hide: kern_path(%s) failed: %d\n", path_str, error);
		return error;
	}

	int ret = -ENOENT;
	inode = d_inode(path.dentry);
	if (!inode)
		goto out;

	if (ino_out)
		*ino_out = inode->i_ino;
	if (fops_out)
		*fops_out = (struct file_operations *)inode->i_fop;
	ret = 0;
out:
	path_put(&path);
	return ret;
}

static int resolve_fops(const char *path_str, struct file_operations **out_fops)
{
	return resolve_path_info(path_str, NULL, out_fops);
}

static void hook_selinux_status_open(void)
{
	if (orig_sel_open_handle_status)
	return;

	struct file_operations *ops = NULL;
	if (resolve_fops("/sys/fs/selinux/status", &ops)) {
		pr_err("ksu_selinux_hide: sel_handle_status_ops not found, fake status disabled\n");
		return;
	}

	if (!ops->open) {
		pr_err("ksu_selinux_hide: sel_handle_status_ops->open is NULL\n");
		return;
	}
	
	orig_sel_open_handle_status = ops->open;
	patch_fops_open(ops, my_sel_open_handle_status);
	pr_info("ksu_selinux_hide: hooked sel_handle_status_ops->open\n");
}

static void unhook_selinux_status_open(void)
{
	if (!orig_sel_open_handle_status)
	return;

	struct file_operations *ops = NULL;
	if (resolve_fops("/sys/fs/selinux/status", &ops)) {
		pr_err("ksu_selinux_hide: sel_handle_status_ops not found on unhook\n");
		return;
}

	patch_fops_open(ops, orig_sel_open_handle_status);
	orig_sel_open_handle_status = NULL;
	pr_info("ksu_selinux_hide: unhooked sel_handle_status_ops->open\n");
}

typedef ssize_t (*ksu_fops_write_fn)(struct file *, const char __user *, size_t, loff_t *);

static const char *const hidden_selinux_tokens[] = {
	KERNEL_SU_DOMAIN, /* "ksu" */
	KERNEL_SU_FILE,   /* "ksu_file" */
	"magisk",
	"zygisk",
	"lsposed",
};

static bool hidden_token_in(const char *buf)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(hidden_selinux_tokens); i++) {
		if (strstr(buf, hidden_selinux_tokens[i]))
			return true;
	}
	return false;
}

static bool probe_can_see(void)
{
	return ksu_selinux_hide_is_enabled && current_uid().val >= 10000;
}

static bool selinuxfs_hooked;
static bool procfs_hooked;
static ino_t selinux_context_ino;
static ino_t selinux_access_ino;
static ksu_fops_write_fn orig_transaction_write;
static ksu_fops_write_fn orig_proc_attr_current_write;

static ssize_t ksu_transaction_write(struct file *file,
				     const char __user *buf, size_t size,
				     loff_t *pos)
{
	if (likely(probe_can_see())) {
		ino_t ino = file_inode(file)->i_ino;
		if (ino == selinux_context_ino || ino == selinux_access_ino) {
			char scan[256];
			size_t n = min(size, sizeof(scan) - 1);
			if (n && copy_from_user(scan, buf, n) == 0) {
				scan[n] = '\0';
				if (hidden_token_in(scan))
					return -EINVAL;
			}
		}
	}
	return orig_transaction_write(file, buf, size, pos);
}

static ssize_t ksu_proc_attr_current_write(struct file *file,
					   const char __user *buf, size_t size,
					   loff_t *pos)
{
	if (likely(probe_can_see())) {
		struct dentry *dentry = file->f_path.dentry;
		if (dentry && dentry->d_name.name &&
		    !strcmp(dentry->d_name.name, "current")) {
			char scan[256];
			size_t n = min(size, sizeof(scan) - 1);
			if (n && copy_from_user(scan, buf, n) == 0) {
				scan[n] = '\0';
				if (hidden_token_in(scan))
					return -EINVAL;
			}
		}
	}
	return orig_proc_attr_current_write(file, buf, size, pos);
}

static void hook_selinuxfs_transaction(void)
{
	if (selinuxfs_hooked)
		return;

	struct file_operations *ops = NULL;
	if (resolve_path_info("/sys/fs/selinux/context", &selinux_context_ino, &ops)) {
		pr_err("ksu_selinux_hide: could not resolve /sys/fs/selinux/context\n");
		return;
	}

	struct file_operations *access_ops = NULL;
	if (resolve_path_info("/sys/fs/selinux/access", &selinux_access_ino, &access_ops)) {
		pr_err("ksu_selinux_hide: could not resolve /sys/fs/selinux/access\n");
		return;
	}

	if (!ops->write || ops->write != access_ops->write) {
		pr_warn("ksu_selinux_hide: unexpected transaction_ops->write\n");
		return;
	}

	orig_transaction_write = ops->write;
	if (patch_fops_slot((unsigned long)&ops->write, (void *)ksu_transaction_write)) {
		pr_err("ksu_selinux_hide: failed to patch transaction_ops->write\n");
		orig_transaction_write = NULL;
		return;
	}
	selinuxfs_hooked = true;
	pr_info("ksu_selinux_hide: hooked transaction_ops->write (context_ino=%lu access_ino=%lu)\n",
		(unsigned long)selinux_context_ino, (unsigned long)selinux_access_ino);
}

static void unhook_selinuxfs_transaction(void)
{
	if (!selinuxfs_hooked || !orig_transaction_write)
		return;

	struct file_operations *ops = NULL;
	if (resolve_path_info("/sys/fs/selinux/context", NULL, &ops)) {
		pr_err("ksu_selinux_hide: transaction_ops not found on unhook\n");
		return;
	}

	patch_fops_slot((unsigned long)&ops->write, (void *)orig_transaction_write);
	orig_transaction_write = NULL;
	selinuxfs_hooked = false;
	pr_info("ksu_selinux_hide: unhooked transaction_ops->write\n");
}

static void hook_proc_attr_current(void)
{
	if (procfs_hooked)
		return;

	struct file_operations *ops = NULL;
	if (resolve_path_info("/proc/1/attr/current", NULL, &ops)) {
		pr_err("ksu_selinux_hide: could not resolve /proc/1/attr/current\n");
		return;
	}

	if (!ops->write) {
		pr_warn("ksu_selinux_hide: proc attr current->write is NULL\n");
		return;
	}

	orig_proc_attr_current_write = ops->write;
	if (patch_fops_slot((unsigned long)&ops->write, (void *)ksu_proc_attr_current_write)) {
		pr_err("ksu_selinux_hide: failed to patch proc attr current->write\n");
		orig_proc_attr_current_write = NULL;
		return;
	}
	procfs_hooked = true;
	pr_info("ksu_selinux_hide: hooked proc attr current->write\n");
}

static void unhook_proc_attr_current(void)
{
	if (!procfs_hooked || !orig_proc_attr_current_write)
		return;

	struct file_operations *ops = NULL;
	if (resolve_path_info("/proc/1/attr/current", NULL, &ops)) {
		pr_err("ksu_selinux_hide: proc attr fops not found on unhook\n");
		return;
	}

	patch_fops_slot((unsigned long)&ops->write, (void *)orig_proc_attr_current_write);
	orig_proc_attr_current_write = NULL;
	procfs_hooked = false;
	pr_info("ksu_selinux_hide: unhooked proc attr current->write\n");
}

static int selinux_hide_status_feature_get(u64 *value)
{
	*value = ksu_selinux_hide_is_enabled ? 1 : 0;
	return 0;
}

static int selinux_hide_status_feature_set(u64 value)
{
	bool enable = !!value;
	if (enable == ksu_selinux_hide_is_enabled) {
		pr_info("ksu_selinux_hide: no need to change\n");
		return 0;
	}
	ksu_selinux_hide_is_enabled = enable;

	if (!ksu_selinux_hide_is_enabled)
		ksu_selinux_hide_disable();
	else
		ksu_selinux_hide_enable();

	pr_info("ksu_selinux_hide: set to %d\n", enable);
	return 0;
}

static const struct ksu_feature_handler selinux_hide_status_handler = {
	.feature_id = KSU_FEATURE_SELINUX_HIDE_STATUS,
	.name = "selinux_hide_status",
	.get_handler = selinux_hide_status_feature_get,
	.set_handler = selinux_hide_status_feature_set,
};

static int ksu_hide_init_thread(void *data)
{
	set_user_nice(current, 19);

	while (READ_ONCE(ksu_input_hook))
		msleep(5000);

	if (ksu_selinux_hide_is_enabled)
		ksu_selinux_hide_enable();

	int tries = 0;
try_again:
	initialize_fake_status();
	hook_selinux_status_open();
	if (READ_ONCE(fake_status))
		goto page_ok;

	msleep(1000);
	if (++tries > 10) {
		pr_warn("ksu_selinux_hide: giving up on fake status page after %d tries\n", tries);
		return 0;
	}
	goto try_again;

page_ok:
	// Probe hooks need both procfs and selinuxfs mounted; retry until available.
	tries = 0;
	while (tries++ < 20 && (!selinuxfs_hooked || !procfs_hooked)) {
		hook_selinuxfs_transaction();
		hook_proc_attr_current();
		if (!selinuxfs_hooked || !procfs_hooked)
			msleep(1000);
	}
	if (!selinuxfs_hooked || !procfs_hooked)
		pr_warn("ksu_selinux_hide: probe hooks not fully installed (selinuxfs=%d proc=%d)\n",
			selinuxfs_hooked, procfs_hooked);
	return 0;
}

void __init ksu_selinux_hide_init(void)
{
	if (ksu_register_feature_handler(&selinux_hide_status_handler))
		pr_err("ksu_selinux_hide: failed to register feature handler\n");

	kthread_run(ksu_hide_init_thread, NULL, "ksu_selinux_hide_init");
}

void __exit ksu_selinux_hide_exit(void)
{
	ksu_unregister_feature_handler(KSU_FEATURE_SELINUX_HIDE_STATUS);
	unhook_selinux_status_open();
	unhook_selinuxfs_transaction();
	unhook_proc_attr_current();
	ksu_selinux_hide_disable();
	mutex_lock(&fake_status_init_mutex);
	if (fake_status) {
		__free_page(fake_status);
		fake_status = NULL;
	}
	mutex_unlock(&fake_status_init_mutex);
}