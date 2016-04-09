/* my_module.c: Example char device module.
 *
 */
/* Kernel Programming */
#define MODULE
#define LINUX
#define __KERNEL__

#include <linux/kernel.h>  	
#include <linux/module.h>
#include <linux/fs.h>       		
#include <asm/uaccess.h>
#include <linux/errno.h>  
#include <linux/slab.h>	

#include "my_module.h"

#define MY_DEVICE "my_device"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anonymous");

/* globals */
int my_major = 0; /* will hold the major # of my device driver */

struct file_operations my_fops = {
    .open = my_open,
    .release = my_release,
    .read = my_read,
    .write = my_write,
    .ioctl = my_ioctl
};

typedef struct my_dev {
	int id; //id of the device - minor number
	void* data; //pointer to a 4KB data block. allocated once per device file.
	int read_ptr; //reading location within data block
	int write_ptr; //writing location within data block
	struct my_dev* next; //pointer to the next device
} mydev;

mydev* devlist_ptr; //pointer to the head of the device list


int init_module(void)
{
    my_major = register_chrdev(my_major, MY_DEVICE, &my_fops);

    if (my_major < 0)
    {
		printk(KERN_WARNING "can't get dynamic major\n");
		return my_major;
    }

	//
    // do_init();
	devlist_ptr = NULL;
	//
    return 0;
}


void cleanup_module(void)
{
    unregister_chrdev(my_major, MY_DEVICE);

    //
    // do clean_up();
	mydev *devptr, *next;
	for (devptr = devlist_ptr; devptr; devptr = next){
		kfree(devptr->data);
		next = devptr->next;
		kfree(devptr);
	}
	//
    return;
}


int my_open(struct inode *inode, struct file *filp)
{
	int minor = MINOR(inode->i_rdev);
	MOD_INC_USE_COUNT;
	mydev *devptr;
	for (devptr = devlist_ptr; devptr; devptr = devptr->next){
		if (devptr->id == minor){ //buffer already exeists for device file
			break;
		}
	}
	
	if (devptr == NULL) { //buffer does not exist for device file, create new
		devptr = kmalloc(sizeof(mydev), GFP_KERNEL);
		if (devptr == NULL) {
			printk(KERN_WARNING "can't get dynamic memory\n");
			return -ENOMEM;
		}
		devptr->id = minor;
		devptr->data = kmalloc(MY_BLOCK_SIZE, GFP_KERNEL);
		if (devptr->data == NULL) {
			kfree(devptr);
			printk(KERN_WARNING "can't get dynamic memory\n");
			return -ENOMEM;
		}
		memset(devptr->data, 0, MY_BLOCK_SIZE);
		devptr->read_ptr = 0;
		devptr->write_ptr = 0;
		devptr->next = devlist_ptr;
		devlist_ptr = devptr;
	}
	filp->private_data = devptr;
    return 0;

}


int my_release(struct inode *inode, struct file *filp)
{
	MOD_DEC_USE_COUNT;
    return 0;
}


ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	mydev *devptr = filp->private_data;
	size_t actual_count;
	if (devptr->read_ptr + count > devptr->write_ptr) {
		printk ("Trying to read too much data, reading until write_ptr\n");
		actual_count = devptr->write_ptr - devptr->read_ptr;
	}
	else{
		actual_count = count;
	}
	if (copy_to_user(buf, devptr->data + devptr->read_ptr, actual_count)){
		return -EFAULT;
	}
	devptr->read_ptr += actual_count;
	return actual_count;
}


ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	mydev *devptr = filp->private_data;
	if (devptr->write_ptr + count > MY_BLOCK_SIZE) {
		printk(KERN_WARNING "not enough free memory in buffer\n");
		return -ENOMEM;
	}
	if (copy_from_user(devptr->data + devptr->write_ptr, buf, count)){
		return -EFAULT;
	}
	devptr->write_ptr += count;
	return count;
}


int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{	
	mydev *devptr = filp->private_data;
	switch(cmd)
    {
		case MY_RESET:
			printk("MY_RESET\n");
			devptr->read_ptr = 0;
			devptr->write_ptr = 0;
			break;
		case MY_RESTART:
			printk("MY_RESTART\n");
			devptr->read_ptr = 0;
			break;
		default:
			return -ENOTTY;
    }
    return 0;
}
