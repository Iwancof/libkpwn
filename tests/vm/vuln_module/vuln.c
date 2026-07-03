#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("libkpwn test: intentionally vulnerable char device");

#define DEVICE_NAME "vuln"
#define BUF_SIZE 256

static int major;
static struct class *vuln_class;
static struct cdev vuln_cdev;

struct vuln_note {
  char data[BUF_SIZE];
  size_t len;
};

static struct vuln_note *note;

// CMD definitions for ioctl
#define VULN_ALLOC _IO('V', 0)
#define VULN_FREE _IO('V', 1)
#define VULN_WRITE _IOW('V', 2, struct vuln_note)
#define VULN_READ _IOR('V', 3, struct vuln_note)
// UAF: free but keep pointer (for testing UAF exploitation)
#define VULN_UAF_FREE _IO('V', 4)

static int vuln_open(struct inode *inode, struct file *filp) { return 0; }

static int vuln_release(struct inode *inode, struct file *filp) { return 0; }

static long vuln_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
  switch (cmd) {
  case VULN_ALLOC:
    if (note) {
      pr_info("vuln: note already allocated\n");
      return -EEXIST;
    }
    note = kmalloc(sizeof(*note), GFP_KERNEL);
    if (!note)
      return -ENOMEM;
    memset(note, 0, sizeof(*note));
    pr_info("vuln: allocated note at %px (kmalloc-%zu)\n", note, sizeof(*note));
    return 0;

  case VULN_FREE:
    if (!note)
      return -ENOENT;
    kfree(note);
    note = NULL;
    pr_info("vuln: freed note\n");
    return 0;

  case VULN_UAF_FREE:
    if (!note)
      return -ENOENT;
    kfree(note);
    // Intentionally do NOT null the pointer → UAF
    pr_info("vuln: UAF freed note (pointer kept at %px)\n", note);
    return 0;

  case VULN_WRITE:
    if (!note)
      return -ENOENT;
    if (copy_from_user(note, (void __user *)arg, sizeof(*note)))
      return -EFAULT;
    pr_info("vuln: wrote %zu bytes\n", note->len);
    return 0;

  case VULN_READ:
    if (!note)
      return -ENOENT;
    if (copy_to_user((void __user *)arg, note, sizeof(*note)))
      return -EFAULT;
    return 0;

  default:
    return -EINVAL;
  }
}

static const struct file_operations vuln_fops = {
    .owner = THIS_MODULE,
    .open = vuln_open,
    .release = vuln_release,
    .unlocked_ioctl = vuln_ioctl,
};

static int __init vuln_init(void) {
  int ret;

  major = register_chrdev(0, DEVICE_NAME, &vuln_fops);
  if (major < 0)
    return major;

  vuln_class = class_create(DEVICE_NAME);
  if (IS_ERR(vuln_class)) {
    unregister_chrdev(major, DEVICE_NAME);
    return PTR_ERR(vuln_class);
  }

  device_create(vuln_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
  pr_info("vuln: loaded (major=%d, sizeof(note)=%zu)\n", major,
          sizeof(struct vuln_note));
  return 0;
}

static void __exit vuln_exit(void) {
  device_destroy(vuln_class, MKDEV(major, 0));
  class_destroy(vuln_class);
  unregister_chrdev(major, DEVICE_NAME);
  pr_info("vuln: unloaded\n");
}

module_init(vuln_init);
module_exit(vuln_exit);
