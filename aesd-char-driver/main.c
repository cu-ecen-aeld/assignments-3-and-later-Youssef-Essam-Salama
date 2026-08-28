/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include "aesdchar.h"
#include <linux/fs.h> // file_operations
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>

static int aesd_open(struct inode *inode, struct file *filp);
static int aesd_release(struct inode *inode, struct file *filp);
static ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
			 loff_t *f_pos);
static ssize_t aesd_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *f_pos);
static int aesd_setup_cdev(struct aesd_dev *dev);
static void aesd_circular_buffer_cleanup(struct aesd_circular_buffer *buffer);

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Youssef Essam Salama"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

static int aesd_open(struct inode *inode, struct file *filp)
{
	PDEBUG("open");

	/**
	 * TODO: handle open
	 */
	return 0;
}

static int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release");
	/**
	 * TODO: handle release
	 */
	return 0;
}

static ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
			 loff_t *f_pos)
{
	ssize_t retval;
	size_t entry_offset;
	size_t copy_size;
	struct aesd_buffer_entry *entry;
	unsigned long k_retval;

	loff_t char_offset = *f_pos;

	PDEBUG("read %zu bytes with offset %lld", count, *f_pos);
	/**
	 * TODO: handle read
	 */

	retval = down_read_killable(&aesd_device.rw_semaphore);
	if (0 != retval) {
		goto fn_return;
	}

	while (count > 0) {
		entry = aesd_circular_buffer_find_entry_offset_for_fpos(
			&aesd_device.buffer, char_offset, &entry_offset);
		if (NULL == entry) {
			break; /* No more data to read */
		}
		copy_size = min(count, entry->size - entry_offset);
		k_retval = copy_to_user(
			(void *)buf + retval,
			(const void *)entry->buffptr + entry_offset, copy_size);
		if (0 != k_retval) {
			retval = -EFAULT;
			break;
		}
		count -= copy_size;
		retval += copy_size;
		char_offset += copy_size;
	}

	up_read(&aesd_device.rw_semaphore);

	*f_pos = char_offset;
fn_return:
	return retval;
}

static ssize_t aesd_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *f_pos)
{
	ssize_t retval;
	unsigned long k_retval;
	struct aesd_buffer_entry new_entry;
	const char *returned_entry_buff_ptr;

	PDEBUG("write %zu bytes with offset %lld", count, *f_pos);
	/**
	 * TODO: handle write
	 */

	new_entry.buffptr = kmalloc(count, GFP_KERNEL);
	if (NULL == new_entry.buffptr) {
		retval = -ENOMEM;
		goto fn_return;
	}
	new_entry.size = count;

	k_retval = copy_from_user((void *)new_entry.buffptr, (const void *)buf,
				  count);
	if (0 != k_retval) {
		retval = -EFAULT;
		goto k_copy_from_user_fail;
	}

	retval = down_write_killable(&aesd_device.rw_semaphore);
	if (0 != retval) {
		goto lock_fail;
	}

	returned_entry_buff_ptr =
		aesd_circular_buffer_add_entry(&aesd_device.buffer, &new_entry);

	up_write(&aesd_device.rw_semaphore);

	kfree(returned_entry_buff_ptr);

	retval = count;
	goto fn_return;

lock_fail:
k_copy_from_user_fail:
	kfree(new_entry.buffptr);
fn_return:
	return retval;
}
struct file_operations aesd_fops = {
	.owner = THIS_MODULE,
	.read = aesd_read,
	.write = aesd_write,
	.open = aesd_open,
	.release = aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);

	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;

	err = cdev_add(&dev->cdev, devno, 1);
	if (err) {
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}

static int __init aesd_init_module(void)
{
	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device, 0, sizeof(struct aesd_dev));

	/**
	 * TODO: initialize the AESD specific portion of the device
	 */
	init_rwsem(&aesd_device.rw_semaphore);
	aesd_circular_buffer_init(&aesd_device.buffer);

	result = aesd_setup_cdev(&aesd_device);

	if (result) {
		unregister_chrdev_region(dev, 1);
	}

	return result;
}

static void aesd_circular_buffer_cleanup(struct aesd_circular_buffer *buffer)
{
	uint8_t index;
	struct aesd_buffer_entry *entry;

	AESD_CIRCULAR_BUFFER_FOREACH(entry, buffer, index)
	{
		kfree(entry->buffptr);
		entry->buffptr = NULL;
	}
}

static void __exit aesd_cleanup_module(void)
{
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

	/**
	 * TODO: cleanup AESD specific poritions here as necessary
	 */
	down_write(&aesd_device.rw_semaphore);
	aesd_circular_buffer_cleanup(&aesd_device.buffer);
	up_write(&aesd_device.rw_semaphore);

	unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
