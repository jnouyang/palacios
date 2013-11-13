/* 
   Palacios main control interface for Kitten
   (c) Jack Lange, 2013
 */

#include <lwk/kernel.h>
#include <lwk/smp.h>
#include <lwk/pmem.h>
#include <lwk/string.h>
#include <arch/proto.h>
#include <lwk/cpuinfo.h>
#include <lwk/driver.h>
#include <lwk/fdTable.h>
#include <lwk/poll.h>
#include <lwk/kfs.h>

#include <lwk/kthread.h>
#include <arch/unistd.h>
#include <arch/vsyscall.h>

#include <arch/pisces/pisces_file.h>

#include "palacios.h"

//static struct v3_guest * guest_map[MAX_VMS] = {[0 ... MAX_VMS - 1] = 0};
static char * options = NULL;



/*
static int register_vm(struct v3_guest * guest) {
    int i = 0;

    for (i = 0; i < MAX_VMS; i++) {
	if (guest_map[i] == NULL) {
	    guest_map[i] = guest;
	    return i;
	}
    }

    return -1;
}

*/


/**
 * Starts a guest operating system.
 */
#if 0
static int
palacios_run_guest(void *arg)
{
	unsigned int mask = 0;
	struct v3_vm_info * vm_info = v3_create_vm(NULL, NULL, NULL);
	
	if (!vm_info) {
		printk(KERN_ERR "Could not create guest context\n");
		return -1;
	}


	printk(KERN_INFO "Starting Guest OS...\n");

	// set the mask to inclue all available CPUs
	// we assume we will start on CPU 0
	//	mask=~((((signed int)1<<(sizeof(unsigned int)*8-1))>>(sizeof(unsigned int)*8-1))<<cpus_weight(cpu_online_map));

	return v3_start_vm(vm_info, mask);
}

#endif

static long
palacios_ioctl(struct file * filp,
	       unsigned int ioctl, unsigned long arg) 
{
    void __user * argp = (void __user *)arg;

    switch (ioctl) {
	case V3_CREATE_GUEST: {
	    struct vm_path guest_path;
	    struct v3_guest * guest = NULL;
	    u64 img_size = 0;
	    u64 file_handle = 0;
	    u8 * img_ptr = NULL;

	    memset(&guest_path, 0, sizeof(struct vm_path));
	    
	    if (copy_from_user(&guest_path, argp, sizeof(struct vm_path))) {		
		return -EFAULT;
	    }

	    guest = kmem_alloc(sizeof(struct v3_guest));
 
	    if (guest == NULL) {
		printk(KERN_ERR "Palacios: Error allocating Kernel guest\n");
		return -EFAULT;
	    }

	    memset(guest, 0, sizeof(struct v3_guest));
	    
	    file_handle = pisces_file_open(guest_path.file_name, O_RDONLY);
	    
	    if (file_handle == 0) {
		printk("Error: Could not open VM image file (%s)\n", guest_path.file_name);
		return -1;
	    }

	    img_size = pisces_file_size(file_handle);

	    {
		struct pmem_region result;
		int status = 0;

		status = pmem_alloc_umem(img_size, 0, &result);

		if (status) {
		    return -1;
		}

		status = pmem_zero(&result);
		
		if (status) {
		    return -1;
		}

		img_ptr = __va(result.start);
	    }

	    pisces_file_read(file_handle, img_ptr, img_size, 0);


	    printk("%s\n", img_ptr + 50);


	    break;
	}
	default:
	    return -EINVAL;
    }


    return 0;
}


static struct kfs_fops palacios_ctrl_fops = {
    //	.open = palacios_open, 
    //	.write = palacios_write,
    //	.read = palacios_read,
    //	.poll = palacios_poll, 
    //	.close = palacios_close,
    .unlocked_ioctl = palacios_ioctl,
};

/**
 * Initialize the Palacios hypervisor.
 */
static int
palacios_init(void)
{

	printk(KERN_INFO "---- Initializing Palacios hypervisor support\n");
	printk(KERN_INFO "cpus_weight(cpu_online_map)=0x%x\n", cpus_weight(cpu_online_map));

	palacios_vmm_init(options);

	//	syscall_register(__NR_v3_start_guest, (syscall_ptr_t) sys_v3_start_guest);


	kfs_create("./palacios-cmd", 
		   NULL, 
		   &palacios_ctrl_fops,
		   0777, 
		   NULL, 0);


	return 0;
}

DRIVER_INIT( "module", palacios_init );
DRIVER_PARAM(options, charp);
