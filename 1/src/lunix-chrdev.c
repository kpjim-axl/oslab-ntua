/*
 * lunix-chrdev.c
 *
 * Implementation of character devices
 * for Lunix:TNG
 *
 * Konstantinos Papadimitriou < k.s.papadimitriou@gmail.com >
 *
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>

#include "lunix.h"
#include "lunix-chrdev.h"
#include "lunix-lookup.h"
/*
 * Global data
 */
struct cdev lunix_chrdev_cdev;

/*
 * Just a quick [unlocked] check to see if the cached
 * chrdev state needs to be updated from sensor measurements.
 */
static int lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	sensor = state->sensor;
	WARN_ON (!sensor);
	/* ? */
	if ( sensor->msr_data[state->type]->last_update != state->buf_timestamp )
	    return 1;

	/* The following return is bogus, just for the stub to compile */
	return 0; /* ? */
}

/*
 * Updates the cached state of a character device
 * based on sensor data. Must be called with the
 * character device state lock held.
 */
static int lunix_chrdev_state_update(struct lunix_chrdev_state_struct *state)
{
    /* TODO: forward to userspace raw data as well */
	
	unsigned long spin_flags;
	uint32_t data;
	uint32_t timestamp;
	long *search[N_LUNIX_MSR] = { lookup_voltage, lookup_temperature, lookup_light };	//lookup-tables
	long cooked;
	int have_new_data;
	int integer_part;
	int fractional_part;
	char sign;
	int ret;
	
	struct lunix_sensor_struct *sensor;
	sensor = state->sensor;
	
	WARN_ON (!sensor);
	
	/*
	 * Grab the raw data quickly, hold the
	 * spinlock for as little as possible.
	 */
	/* Why use spinlocks? See LDD3, p. 119 */
	
	spin_lock_irqsave(&sensor->lock, spin_flags);
	have_new_data = lunix_chrdev_state_needs_refresh(state);
	if (have_new_data == 1){
	    data = sensor->msr_data[state->type]->values[0];
	    timestamp = sensor->msr_data[state->type]->last_update;
	}
	spin_unlock_irqrestore(&sensor->lock, spin_flags);

	/*
	 * Now we can take our time to format them,
	 * holding only the private state semaphore
	 */
	
	/* TODO: use ioctl to choose between raw and cooked */
	if (have_new_data == 1){
	    cooked = search[state->type][data];
	    integer_part = cooked / 1000;
	    fractional_part = cooked % 1000;
	    if (cooked >= 0)
		sign = ' ';
	    else
		sign = '-';
	    snprintf(state->buf_data, LUNIX_CHRDEV_BUFSZ, "%c%d.%d\n", sign, integer_part, fractional_part);
	    debug("smells good: %s\n", state->buf_data);
	    state->buf_data[LUNIX_CHRDEV_BUFSZ - 1]='\0';
	    state->buf_lim = strnlen(state->buf_data, LUNIX_CHRDEV_BUFSZ);
	    state->buf_timestamp = timestamp;
	    
	    ret = 0;
	    goto out;
	}
	else {
	    ret = -EAGAIN;
	    goto out;
	}


out:
	debug("leaving, ret: %d\n", ret);
	return ret;
}

/*************************************
 * Implementation of file operations
 * for the Lunix character device
 *************************************/

static int lunix_chrdev_open(struct inode *inode, struct file *filp)
{
	/* Declarations */
	unsigned int minor;
	struct lunix_chrdev_state_struct *state;
	int type;
	int ret;
	
	debug("entering\n");
	ret = -ENODEV;
	if ((ret = nonseekable_open(inode, filp)) < 0)
		goto out;

	/*
	 * Associate this open file with the relevant sensor based on
	 * the minor number of the device node [/dev/sensor<NO>-<TYPE>]
	 */
	
	minor = iminor(inode);
	type = minor & 7;
	// type = minor % 8;

	if (type >= N_LUNIX_MSR )
	    goto out;
	
	
	
	/* Allocate a new Lunix character device private state structure */
	state = kmalloc( sizeof(struct lunix_chrdev_state_struct), GFP_KERNEL );
	if (!state){
	    printk(KERN_ERR "lunix_chrdev_open: Failed to allocate memory\n");
	    ret = -EFAULT;
	    goto out;
	}
	
	state->type = type;
	state->sensor = &lunix_sensors[minor >> 3];	// right_shift to find the sensor chrdev is refering to..
	state->buf_lim = 0;
	state->buf_timestamp = 0;
	
	sema_init(&state->lock, 1);
	
	filp->private_data = state;
out:
	debug("leaving, with ret = %d\n", ret);
	return ret;
}

static int lunix_chrdev_release(struct inode *inode, struct file *filp)
{
	kfree( filp->private_data );
	return 0;
}

static long lunix_chrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* 
	 * raw or cooked?
	 * TIP: see LDD3 -> p.135
	 */
	return -EINVAL;
}

static ssize_t lunix_chrdev_read(struct file *filp, char __user *usrbuf, size_t cnt, loff_t *f_pos)
{
	ssize_t ret;
	ssize_t rem;
	struct lunix_sensor_struct *sensor;
	struct lunix_chrdev_state_struct *state;

	state = filp->private_data;
	WARN_ON(!state);

	sensor = state->sensor;
	WARN_ON(!sensor);

	/* Lock? */
	if (down_interruptible(&state->lock)){
	    ret = -ERESTARTSYS;
	    goto out;
	}
	
	/*
	 * If the cached character device state needs to be
	 * updated by actual sensor data (i.e. we need to report
	 * on a "fresh" measurement), do so
	 */
	while ((ret = lunix_chrdev_state_update(state)) == -EAGAIN && *f_pos == 0)
	{
		/* ? */
		up(&state->lock);
		
		/* The process needs to sleep */
		/* See LDD3, page 153 for a hint */
		printk(KERN_DEBUG "read: No data yet.. Going to sleep..\n");
		if ( wait_event_interruptible(sensor->wq, lunix_chrdev_state_needs_refresh(state)) ){
		    ret = -ERESTARTSYS;
		    goto out;
		}
		if (down_interruptible(&state->lock)){
		    ret = -ERESTARTSYS;
		    goto out;
		}
	}
	
	if (ret == -ERESTARTSYS)
	    goto out_with_unlock;
	debug("reading\n");

	/* End of file */
	if (state->buf_lim == 0) {
	    ret = 0;
	    goto out_with_unlock;
	}
	/* Determine the number of cached bytes to copy to userspace */
	rem = state->buf_lim - *f_pos;
	if (cnt > rem)
	    cnt = rem;
	
	/* If I cant copy cnt bytes, unlock and return fault,
	 * else fix *f_pos
	 */
	if ( (copy_to_user(usrbuf, state->buf_data + *f_pos, cnt)) ){
	    ret = -EFAULT;
	    goto out_with_unlock;
	}
	
	ret = cnt;
	*f_pos += cnt;
	
	/* Auto-rewind on EOF mode? */
	if (*f_pos >= state->buf_lim){
	    debug("read: Buffer limit reached\n");
	    *f_pos = 0;
	    goto out_with_unlock;
	}

	/* Unlock? */
out_with_unlock:
	up(&state->lock);
out:
	return ret;
}

static int lunix_chrdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return -EINVAL;
}

static struct file_operations lunix_chrdev_fops = 
{
        .owner          = THIS_MODULE,
	.open           = lunix_chrdev_open,
	.release        = lunix_chrdev_release,
	.read           = lunix_chrdev_read,
	.unlocked_ioctl = lunix_chrdev_ioctl,
	.mmap           = lunix_chrdev_mmap
};

int lunix_chrdev_init(void)
{
	/*
	 * Register the character device with the kernel, asking for
	 * a range of minor numbers (number of sensors * 8 measurements / sensor)
	 * beginning with LINUX_CHRDEV_MAJOR:0
	 */
	int ret;
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;
	
	debug("initializing character device\n");
	cdev_init(&lunix_chrdev_cdev, &lunix_chrdev_fops);
	lunix_chrdev_cdev.owner = THIS_MODULE;
	
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	/* register_chrdev_region */
	if ((ret = register_chrdev_region(dev_no, lunix_minor_cnt, "lunix")) < 0){
	    debug("Failed to register region: ret = %d\n", ret);
	    goto out;
	}
	
// 	if (ret < 0) {
// 		debug("failed to register region, ret = %d\n", ret);
// 		goto out;
// 	}	

	/* cdev_add */
	if ((ret = cdev_add(&lunix_chrdev_cdev, dev_no, lunix_minor_cnt)) < 0){
	    debug("Failed to add character device\n");
	    goto out_with_chrdev_region;
	}
// 	if (ret < 0) {
// 		debug("failed to add character device\n");
// 		goto out_with_chrdev_region;
// 	}
	debug("completed successfully\n");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
out:
	return ret;
}

void lunix_chrdev_destroy(void)
{
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;
		
	debug("entering\n");
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	cdev_del(&lunix_chrdev_cdev);
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
	debug("leaving\n");
}
