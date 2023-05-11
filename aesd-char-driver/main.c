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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
# include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Iosif Futerman"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev* dev;
	dev = NULL;
  PDEBUG("open");
  dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
  filp->private_data = dev;
  return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release");

//	filp->private_data = NULL;
  return 0;
}

void printBuffer(void);
void printBuffer(){
	uint8_t index;
	struct aesd_buffer_entry *entry;
	PDEBUG("PRINT BUFFER START");	
	AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.circular_buffer,index) {
		if(entry->buffptr){
			PDEBUG("Entry i:%d buffptr:%s", index, entry->buffptr);
		}
	}  
	PDEBUG("PRINT BUFFER END");	
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
  ssize_t retval;
  struct aesd_dev *dev;
	struct aesd_buffer_entry* entry;
	size_t offset;
	size_t kcount;
	size_t bytes_to_read;
	PDEBUG("AESD_READ!!! loff_t f_pos:%lld; file->f_pos:%lld", *f_pos, filp->f_pos);	
	if(!count)
	{
		return 0;
	}
	kcount = 0;
  retval = 0;
	offset = 0;
  dev = (struct aesd_dev *)filp->private_data;
	retval = mutex_lock_interruptible(&dev->mutex_lock);
	if(retval){
		return -ERESTARTSYS;
	}
	bytes_to_read = 0;

	while(count > 0){
		entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circular_buffer, filp->f_pos, &offset);

		if(!entry){
			break;
		}

		if(entry->size - offset > count){
			bytes_to_read = count;
			count = 0;
		}
		else{
			bytes_to_read = entry->size - offset;
			count -= bytes_to_read;
		}

		retval = copy_to_user(buf + kcount, entry->buffptr + offset, bytes_to_read);
		kcount += bytes_to_read;
		*f_pos += bytes_to_read;
		filp->f_pos += bytes_to_read;
		if(retval){
			mutex_unlock(&dev->mutex_lock);
			return kcount - retval;
		}
	}
	mutex_unlock(&dev->mutex_lock);
  return kcount;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{

  ssize_t retval;
	struct aesd_dev *dev;
	struct aesd_buffer_entry* entry;
	struct aesd_buffer_entry new_entry;
	size_t uncopied;
	size_t offset;
	char* kbuf;
	int entry_index;
	entry_index = -1;
	kbuf = NULL;
	offset = 0;
	entry = NULL;
	retval = -ENOMEM;
	uncopied = 0;
	dev = NULL;

  PDEBUG("write %zu bytes with offset %lld; buf:%s",count,*f_pos, buf);
	dev = (struct aesd_dev *)filp->private_data;
	
	retval = mutex_lock_interruptible(&dev->mutex_lock);
	if(retval){
		return -ERESTARTSYS;
	}	
	if(dev->circular_buffer.in_offs){
		entry_index = dev->circular_buffer.in_offs - 1;
	}
	else if(dev->circular_buffer.full){
		entry_index =	AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1;
	}
	if(entry_index != -1){
		entry = &dev->circular_buffer.entry[entry_index];
		if(*(entry->buffptr + entry->size - 1) == '\n'){
			entry_index = -1;
		} else {
				offset = entry->size;
				entry->size = offset + count;
				entry->buffptr = krealloc(entry->buffptr, sizeof(char) * (entry->size), GFP_KERNEL);
				memset((void*)(entry->buffptr + offset), 0, count);
		}
	}
	if(entry_index == -1){
		new_entry.buffptr = kmalloc(sizeof(char) * count, GFP_KERNEL);
		memset((void*)new_entry.buffptr, 0, count);
		if(dev->circular_buffer.entry[dev->circular_buffer.in_offs].buffptr){
			kfree(dev->circular_buffer.entry[dev->circular_buffer.in_offs].buffptr);
		}
		aesd_circular_buffer_add_entry(&dev->circular_buffer, &new_entry);
		if(dev->circular_buffer.in_offs){
			entry_index = dev->circular_buffer.in_offs - 1;
		}
		else {
			entry_index =	AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1;
		}
		entry = &dev->circular_buffer.entry[entry_index];
		entry->size = count;
	}
	if(!entry->buffptr){
		mutex_unlock(&dev->mutex_lock);
		return retval;
	}
	uncopied = copy_from_user((void*)entry->buffptr + offset, buf, count);
	if(uncopied){
	  PDEBUG("WRITE! uncopied %lu bytes", uncopied);
  }
	entry->size -= uncopied;
	mutex_unlock(&dev->mutex_lock);
//	filp->f_pos = 0;
//  *f_pos += count - uncopied;
	*f_pos = 0;
	return count - uncopied;
}

loff_t aesd_llseek (struct file *filp, loff_t off, int whence){
	loff_t retval;
	struct aesd_dev *dev;
	PDEBUG("LLSEEK!");
	dev = (struct aesd_dev *)filp->private_data;
	retval = mutex_lock_interruptible(&dev->mutex_lock);
	if(retval){
		return -ERESTARTSYS;
	}	
	PDEBUG("LLSEEK! off:%lld; whence:%d; dev->circular_buffer.size:%ld, filp->f_pos:%lld", off, whence, dev->circular_buffer.size, filp->f_pos);
	retval = fixed_size_llseek(filp, off, whence, dev->circular_buffer.size);
	PDEBUG("LLSEEK! after fixed filp->f_pos:%lld", filp->f_pos);
	mutex_unlock(&dev->mutex_lock);
	return retval;
}

long aesd_adjust_file_offset(struct file *filp, uint32_t write_cmd, uint32_t write_cmd_offset){
	struct aesd_dev *dev;
	long retval;
	dev = (struct aesd_dev *)filp->private_data;
	
	retval = mutex_lock_interruptible(&dev->mutex_lock);
	if(retval){
		return -ERESTARTSYS;
	}
	retval = aesd_circular_buffer_get_offset_for_byte(&dev->circular_buffer, write_cmd, write_cmd_offset);
	mutex_unlock(&dev->mutex_lock);
	return retval;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	long retval;
	PDEBUG("BEIGIN IOCTL!!! cmd:%d",cmd);
	switch(cmd){
		case AESDCHAR_IOCSEEKTO: {
			struct aesd_seekto seek_to;
			PDEBUG("AESDCHAR_IOCSEEKTO");
			if(copy_from_user(&seek_to, (const void __user* )arg, sizeof(struct aesd_seekto)))
			{
				PDEBUG("AESDCHAR_IOCSEEKTO!!! copy_from_user FAULT");
				return -EFAULT;
			}
			PDEBUG("AESDCHAR_IOCSEEKTO!!! seek_to.write_cmd:%d; seek_to.write_cmd_offset:%d", seek_to.write_cmd, seek_to.write_cmd_offset);
			retval = aesd_adjust_file_offset(filp, seek_to.write_cmd, seek_to.write_cmd_offset);
			if(retval >= 0){
				filp->f_pos = retval;
			}
			PDEBUG("AESDCHAR_IOCSEEKTO!!! retval:%ld;", retval);
			break;
		}
		default:
			PDEBUG("IOCTL default case!");
			return -ENOTTY;
	}
	if(retval == -1){
		retval = -EINVAL;
	}
	return retval;
}


struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));


		mutex_init(&aesd_device.mutex_lock);
		aesd_circular_buffer_init(&aesd_device.circular_buffer);
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
	uint8_t index;
	struct aesd_buffer_entry *entry;
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);


	AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.circular_buffer,index) {
		if(entry->buffptr){
			kfree(entry->buffptr);
		}
	}  
	mutex_destroy(&aesd_device.mutex_lock);
  unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
