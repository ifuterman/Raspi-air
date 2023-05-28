#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/fs.h> // file_operations

static int bm680_major =   0; // use dynamic major
staic int bm680_minor =   0;

int bme680_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "bme680-driver");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", bm680_major);
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

void bme680_cleanup_module(void)
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



module_init(bme680_init_module);
module_exit(bme680_cleanup_module);
