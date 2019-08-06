#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>	
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/list.h>

#include <asm/switch_to.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include "linux/stddef.h"

#define init_MUTEX(LOCKNAME) sema_init(LOCKNAME,1);

static int testmod_open( struct inode*, struct file* );
static int testmod_release( struct inode*, struct file* );
static ssize_t testmod_read( struct file*, char*, size_t, loff_t* );
static ssize_t testmod_write( struct file*, const char*, size_t, loff_t* );
static void testmod_cleanup_module(void);
long testmod_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

#define DEVICE_NAME "testmod" /* name shown in /proc/devices */
#define CLASS_NAME "testmod"
#define TESTMOD_MAJOR 0      /* dynamic major by default */
#define TESTMOD_NR_DEVS 3    /* testmod0 through testmod3 */
#define BUFFER_SIZE 1024   /* requirement */
/* IOCTL */
#define TESTMOD_IOC_MAGIC  82
#define TESTMOD_IOCBUFFERRESET    _IO(TESTMOD_IOC_MAGIC, 0)
#define TESTMOD_IOCBUFFERGET    _IO(TESTMOD_IOC_MAGIC, 1)
#define TESTMOD_IOCLASTSTRDROP    _IO(TESTMOD_IOC_MAGIC, 2)
#define TESTMOD_IOC_MAXNR 3

MODULE_AUTHOR( "Yevhen" );
MODULE_LICENSE("GPL");


/*
 * Ioctl definitions
 */

struct my_message                 /* String tracking */
{ 
	unsigned int str_startpos;
	unsigned int str_endpos;                  /* Needed in case of non-nulltermonated str */
	struct list_head list;
};

struct my_buffer
{
	char *buffer, *end;             /* pointers to the beginning and end of the buffer */
	int buffersize;
	int count;                      /* keeps track of how far into the buffer we are */ 
	int numreaders, numwriters;     /* number of readers and writers */
	struct semaphore sem;
	struct list_head strlist_head;  /* list for buffer indexing */
	wait_queue_head_t queue;        /* queue of sleeping processes waiting for access to buffer */
};

struct my_device {
	struct my_buffer *wbuf;
	struct semaphore sem;
	struct cdev cdev;
};

/* Globals */
int ret;
int dev_buffer_size = BUFFER_SIZE;
int testmod_minor;
int testmod_nr_devs = TESTMOD_NR_DEVS;
dev_t testmod_p_devno;			/* first device number */
int testmod_major =   TESTMOD_MAJOR;
static struct my_device *testmod_device;
static struct device* my_device;
static struct class* my_class;
static struct my_buffer *buffers;
LIST_HEAD(messageList);

/* file operations struct */
static struct file_operations testmod_fops = {
	.owner   = THIS_MODULE,
	.read    = testmod_read,
	.write   = testmod_write,
	.open    = testmod_open,
	.release = testmod_release,
	.unlocked_ioctl   = testmod_ioctl
};


static void setup_cdev(struct my_device *dev, int count) {

	int err, devno = testmod_p_devno + count;

	cdev_init(&dev->cdev, &testmod_fops);
	dev->cdev.owner = THIS_MODULE;
	//dev->cdev.ops = &testmod_fops;

	err = cdev_add(&dev->cdev, devno, 1);
	if (err) {
		printk(KERN_ALERT "TESTMOD: Unable to add cdev to kernel.\n");
	}

	printk(KERN_ALERT "Creating device testmod%u \n", count);
	my_device = device_create(my_class, NULL, MKDEV(testmod_major, count), NULL, "testmod%u", count);
	if (IS_ERR(my_device)) {
		testmod_cleanup_module();
		printk(KERN_ALERT "Failed to create testmod device.\n");
	}
}

/* called when module is loaded */
int testmod_init_module( void ) {
	int i;
	dev_t dev = 0;
	/* initialization code belongs here */
	printk(KERN_INFO "Running init\n");

	ret = alloc_chrdev_region(&dev, testmod_minor, testmod_nr_devs,
			"testmod");
	testmod_major = MAJOR(dev);
	if (ret < 0) {
		printk(KERN_ALERT "TESTMOD: Failed to allocate a device region. Error %d\n", ret);
		return ret;
	}

	my_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(my_class)) {
		printk(KERN_WARNING "testmod: can't create class\n");
		ret = -ENOMEM;
	}

	testmod_p_devno = dev; 

	/* allocate memory for the devices */
	testmod_device = kmalloc(testmod_nr_devs * sizeof(struct my_device), GFP_KERNEL);
	if (testmod_device == NULL) {
		printk(KERN_NOTICE "Unable to allocate memory!\n");
		printk("errno = -12 ENOMEM\n");
		unregister_chrdev_region(testmod_major, testmod_nr_devs);
		return -ENOMEM;
	}
	memset(testmod_device, 0, testmod_nr_devs*sizeof(struct my_device));

	printk("Region allocated! Setting up cdev.\n");

	/* allocate memory for the buffers */
	buffers = kmalloc(testmod_nr_devs*sizeof(struct my_buffer), GFP_KERNEL);
	if (buffers == NULL) {
		printk(KERN_NOTICE "Unable to allocate memory!\n");
		printk("errno = -12 ENOMEM\n");
		return -ENOMEM;
	}

	for (i = 0; i < testmod_nr_devs; i++) {
		buffers[i].buffer = kmalloc(dev_buffer_size, GFP_KERNEL);
		//buffers[i].strlist = kmalloc(sizeof(struct my_message), GFP_KERNEL);
		memset(buffers[i].buffer, 0, dev_buffer_size*sizeof(char));
		setup_cdev(&testmod_device[i], i);
		if (!buffers[i].buffer) {
			printk(KERN_NOTICE "Unable to allocate memory!\n");
			printk("errno = -12 ENOMEM\n");
			return -ENOMEM;
		}
		buffers[i].count = 0;
		buffers[i].numwriters = 0;
		buffers[i].numreaders = 0;
		buffers[i].buffersize = dev_buffer_size;
		buffers[i].end = buffers[i].buffer + buffers[i].buffersize;
		INIT_LIST_HEAD(&buffers[i].strlist_head);
		init_waitqueue_head(&buffers[i].queue);
		init_MUTEX(&buffers[i].sem);
		init_MUTEX(&testmod_device[i].sem);
		testmod_device[i].wbuf = &buffers[i];
	}

	printk(KERN_INFO "TESTMOD: Device initialized\n");
	return 0;
}

void testmod_cleanup_module( void ) {

	int i;
	dev_t devno = MKDEV(testmod_major, testmod_minor);

	if(!testmod_device) {
		return;
	}
	for(i = 0; i<testmod_nr_devs; i++) {
		cdev_del(&testmod_device[i].cdev);
		kfree(buffers[i].buffer);
		device_destroy(my_class, MKDEV(testmod_major, i));
	}
	class_unregister(my_class);
	class_destroy(my_class);
	kfree(buffers);
	kfree(testmod_device);
	unregister_chrdev_region(devno, testmod_nr_devs);
	testmod_device = NULL;

	printk(KERN_INFO "TESTMOD: Module unloaded.\n");
}


static int testmod_open( struct inode *inode, struct file *filp ) {

	/* device claiming code belongs here */
	struct my_device *dev; /* device information */

	dev = container_of(inode->i_cdev, struct my_device, cdev);

	if(down_interruptible(&dev->sem)) {
		return -ERESTARTSYS;
	}

	filp->private_data = dev; /* for other methods */

	filp->private_data = dev;

	if(filp->f_mode & FMODE_READ) {
		dev->wbuf->numreaders++;
	}
	if(filp->f_mode & FMODE_WRITE) {
		dev->wbuf->numwriters++;
	}
	up(&dev->sem);
	printk(KERN_INFO "TESTMOD: Device opened.\n");

	return nonseekable_open(inode,filp);
}


static int testmod_release( struct inode *inode, struct file *filp ) {

	struct my_device *dev = filp->private_data;

	down(&dev->sem);
	if (filp->f_mode & FMODE_READ) {
		dev->wbuf->numreaders--;
	}
	if (filp->f_mode & FMODE_WRITE) {
		dev->wbuf->numwriters--;
	}
	up(&dev->sem);

	printk(KERN_INFO "TESTMOD: Released device.\n");

	return 0;
}


static ssize_t testmod_read( struct file *filp,
		char *buf,      /* The buffer to fill with data     */
		size_t count,   /* The max number of bytes to read  */
		loff_t *f_pos )  /* The offset in the file           */
{

	int data = 0;
	unsigned long rval;
	struct my_device *dev = filp->private_data;
	size_t cnt;


	if(down_interruptible(&(dev->wbuf->sem))) {
		return -ERESTARTSYS;
	}

	while (dev->wbuf->count == 0) {
		printk(KERN_INFO "TESTMOD: Buffer is empty.\n");
		/* buffer is empty, sleep */
		up(&(dev->wbuf->sem));
		wake_up_interruptible(&(dev->wbuf->queue));
		if (filp->f_flags & O_NONBLOCK) {
			return -EAGAIN;
		}
		/* Wait for more data. */
		if (wait_event_interruptible(dev->wbuf->queue, (dev->wbuf->count != 0))) {
			return -ERESTARTSYS;
		}
		/* reacquire lock and loop */
		if(down_interruptible(&(dev->wbuf->sem))) {
			return -ERESTARTSYS;
		}
	}

	if (count > (dev->wbuf->count - *f_pos))
		count = dev->wbuf->count - *f_pos;
	rval = copy_to_user(buf, dev->wbuf->buffer + *f_pos, count);
	if (rval < 0)
		return -EFAULT;
	cnt = count - rval;
	*f_pos += cnt;

	up(&(dev->wbuf->sem));
	/* Data has been cleared from buffer! Wake sleeping processes */
	wake_up_interruptible(&(dev->wbuf->queue));

	return cnt;
}


static ssize_t testmod_write( struct file *filp,
		const char *buf,/* The buffer to get data from      */
		size_t count,   /* The max number of bytes to write */
		loff_t *f_pos )  /* The offset in the file           */
{

	int data = 0;
	struct my_device *dev = filp->private_data;
	struct my_message *str = kmalloc(sizeof(struct my_message), GFP_KERNEL), *ptr;
	struct list_head *pos;
	str->str_startpos = dev->wbuf->count;

	if(down_interruptible(&(dev->wbuf->sem))) {
		return -ERESTARTSYS;
	}

	while (data < count) {
		/* not all data is written to buffer yet */ 
		while(dev->wbuf->count == dev_buffer_size) {
			/* buffer is full, prepare for sleep */
			up(&(dev->wbuf->sem));
			/* data is in buffer, wake sleeping processes */
			wake_up_interruptible(&(dev->wbuf->queue));

			if(filp->f_flags & O_NONBLOCK) {
				return -EAGAIN;
			}
			/* Wait for more space in buffer. */
			if(wait_event_interruptible(dev->wbuf->queue, (dev->wbuf->count < dev_buffer_size))) {
				return -ERESTARTSYS;
			}
			/* reacquire lock and loop */
			if (down_interruptible(&(dev->wbuf->sem))) {
				return -ERESTARTSYS;
			}
		}
		/* loop and write until there's no more data or no more space, one character at a time */
		ret = copy_from_user(dev->wbuf->buffer + dev->wbuf->count, buf + data, 1);
		if(ret) {
			up(&(dev->wbuf->sem));
			return -EFAULT;
		}
		data++;
		dev->wbuf->count++;
		//if(dev->wbuf->count == dev_buffer_size) {
		//  dev->wbuf->count = 0;
		//}
	}
	str->str_endpos = dev->wbuf->count;
	list_add_tail(&str->list, &dev->wbuf->strlist_head);

	/* DEBUG
	 *
	 * last
	 ptr = list_last_entry(&dev->wbuf->strlist_head, struct my_message, list);
	 printk(KERN_INFO "entry: %d %d.\n", ptr->str_startpos, ptr->str_endpos);
	 * all
	 list_for_each(pos, &dev->wbuf->strlist_head){
	 ptr = list_entry(pos, struct my_message, list);
	 printk(KERN_INFO "entry: %d %d.\n", qwe);
	 printk(KERN_INFO "entry: %d %d.\n", ptr->str_startpos, ptr->str_endpos);
	 }
	 */
	char qwe[10];
	ptr = list_last_entry(&dev->wbuf->strlist_head, struct my_message, list);
	printk(KERN_INFO "entry: %d %d.\n", ptr->str_startpos, ptr->str_endpos);
	//memcpy(qwe, dev->wbuf->buffer + ptr->str_startpos, (ptr->str_endpos - ptr->str_startpos)); 
	//printk(KERN_INFO "Removing entry qwe: %s\n", qwe);

	up(&(dev->wbuf->sem));
	/* data has been written to buffer, wake up any processes */
	wake_up_interruptible(&dev->wbuf->queue);

	return data;
}

long testmod_ioctl( 
		struct file *filp, 
		unsigned int cmd,   /* command passed from the user */
		unsigned long arg ) /* argument of the command */
{
	struct my_device *dev = filp->private_data;
	struct my_message *ptr;

	printk(KERN_INFO "TESTMOD: ioctl called.\n");
	int i;
	int err = 0;
	if (_IOC_TYPE(cmd) != TESTMOD_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > TESTMOD_IOC_MAXNR) return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd)); 
	if (err) return -EFAULT;

	switch(cmd) {

		case TESTMOD_IOCBUFFERRESET:    /* reset buffer */
			printk(KERN_NOTICE "Clearing buffer\n");
			dev->wbuf->count = 0;
			dev->wbuf->numwriters = 0;
			dev->wbuf->numreaders = 0;
			dev->wbuf->buffersize = dev_buffer_size;
			if (memset(dev->wbuf->buffer, 0, dev_buffer_size*sizeof(char)))
				return 0;
			else 
				return -EFAULT;


		case TESTMOD_IOCBUFFERGET:
			printk(KERN_NOTICE "Buffer size: %d\n", (dev->wbuf->buffersize - dev->wbuf->count));
			return (dev->wbuf->buffersize - dev->wbuf->count);

		case TESTMOD_IOCLASTSTRDROP:
			ptr = list_last_entry(&dev->wbuf->strlist_head, struct my_message, list);
			dev->wbuf->count = dev->wbuf->count - (ptr->str_endpos - ptr->str_startpos);
			memset(dev->wbuf->buffer + ptr->str_startpos, 0, (ptr->str_endpos - ptr->str_startpos));
			list_del(&ptr->list);
			kfree(ptr);

	}
	//return retval;

	return 0; //has to be changed
}

module_init(testmod_init_module);
module_exit(testmod_cleanup_module );
