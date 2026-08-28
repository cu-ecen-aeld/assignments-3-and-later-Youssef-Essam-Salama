/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#undef PDEBUG /* undef it, just in case */
#ifdef AESD_DEBUG
#ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
#define PDEBUG(fmt, args...) printk(KERN_DEBUG "aesdchar: " fmt, ##args)
#else
/* This one for user space */
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#endif
#else
#define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#include "aesd-circular-buffer.h"
#include <linux/cdev.h>
#include <linux/rwsem.h>

struct aesd_dev {
	/**
	 * TODO: Add structure(s) and locks needed to complete assignment
	 * requirements
	 */
	struct rw_semaphore rw_semaphore;   /* Read-write semaphore */
	struct aesd_circular_buffer buffer; /* Circular buffer structure */
	struct cdev cdev;		    /* Char device structure      */
};

#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
