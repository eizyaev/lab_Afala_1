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

#define MY_DEVICE "MY_DEVICE"

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
	struct my_dev* next; //pointer to the next device
} mydev;

typedef struct fileinfo {
	void* data; //pointer to the 4KB data block of the device file
	int read_ptr; //reading location within data block
	int write_ptr; //writing location within data block
} fileinfo;

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
	printk ("printk test - my_open\n");
	int minor = MINOR(inode->i_rdev);
	//MOD_INC_USE_COUNT;
	fileinfo *fileinfo_ptr = kmalloc(sizeof(fileinfo), GFP_KERNEL);
	if (fileinfo_ptr == NULL) {
		MOD_DEC_USE_COUNT;
		printk(KERN_WARNING "can't get dynamic memory\n");
		return -ENOMEM;
	}
	printk ("printk test1 - my_open\n");
	fileinfo_ptr->read_ptr = 0;
	fileinfo_ptr->write_ptr = 0;

	printk ("printk test2 - my_open\n");
	mydev *devptr;
	for (devptr = devlist_ptr; devptr; devptr = devptr->next){
		if (devptr->id == minor){ //buffer already exeists for device file
			fileinfo_ptr->data = devptr->data;
			break;
		}
	}
	
	printk ("printk test3 - my_open\n");
	if (devptr == NULL) { //buffer does not exist for device file, create new
	printk ("printk test4 - my_open\n");
		mydev *new_devptr = kmalloc(sizeof(mydev), GFP_KERNEL);
	printk ("printk test5 - my_open\n");
		if (new_devptr == NULL) {
			MOD_DEC_USE_COUNT;
			printk(KERN_WARNING "can't get dynamic memory\n");
			kfree(fileinfo_ptr);
			return -ENOMEM;
		}
	printk ("printk test6 - my_open\n");
		new_devptr->id = minor;
		new_devptr->data = kmalloc(MY_BLOCK_SIZE, GFP_KERNEL);
		if (new_devptr->data == NULL) {
			kfree(fileinfo_ptr);
			kfree(new_devptr);
			printk(KERN_WARNING "can't get dynamic memory\n");
			MOD_DEC_USE_COUNT;
			return -ENOMEM;
		}
	printk ("printk test7 - my_open\n");
		memset(new_devptr->data, 0, MY_BLOCK_SIZE);
	printk ("printk test8 - my_open\n");
		fileinfo_ptr->data = new_devptr->data;
	printk ("printk test9 - my_open\n");
		new_devptr->next = devlist_ptr;
		devlist_ptr = new_devptr;
	}
	printk ("printk test10 - my_open\n");
	filp->private_data = fileinfo_ptr;
	
	printk ("printk test11 - my_open\n");
	if (filp->f_mode & FMODE_READ)
    {
	//
	// handle read opening
	//
    }
    
    if (filp->f_mode & FMODE_WRITE)
    {
        //
        // handle write opening
        //
    }

    return 0;

}


int my_release(struct inode *inode, struct file *filp)
{
	kfree(filp->private_data);
	MOD_DEC_USE_COUNT;
	if (filp->f_mode & FMODE_READ)
    {
	//
	// handle read closing
	// 
    }
    
    if (filp->f_mode & FMODE_WRITE)
    {
        //
        // handle write closing
        //
    }

    return 0;
}


ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	fileinfo *fileinfo_ptr = filp->private_data;
	size_t actual_count;
	if (fileinfo_ptr->read_ptr + count > fileinfo_ptr->write_ptr) {
		actual_count = fileinfo_ptr->write_ptr - fileinfo_ptr->read_ptr;
	}
	else{
		actual_count = count;
	}
	if (copy_to_user(buf, fileinfo_ptr->data + fileinfo_ptr->read_ptr, actual_count)){
		return -EFAULT;
	}
	fileinfo_ptr->read_ptr += actual_count;
	return actual_count;
}


ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	fileinfo *fileinfo_ptr = filp->private_data;
	printk ("fileinfo_ptr->write_ptr = %d", fileinfo_ptr->write_ptr);
	if (fileinfo_ptr->write_ptr + count > MY_BLOCK_SIZE) {
		printk(KERN_WARNING "not enough free memory in buffer\n");
		return -ENOMEM;
	}
	if (copy_from_user(fileinfo_ptr->data + fileinfo_ptr->write_ptr, buf, count)){
		return -EFAULT;
	}
	fileinfo_ptr->write_ptr += count;
	printk ("fileinfo_ptr->write_ptr = %d", fileinfo_ptr->write_ptr);
	return count;
}


int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	fileinfo *fileinfo_ptr = filp->private_data;
	switch(cmd)
    {
		case MY_RESET:
			printk("MY_RESET\n");
			fileinfo_ptr->read_ptr = 0;
			fileinfo_ptr->write_ptr = 0;
			break;
		case MY_RESTART:
			printk("MY_RESTART\n");
			fileinfo_ptr->read_ptr = 0;
			break;
		default:
			return -ENOTTY;
    }
    return 0;
}
