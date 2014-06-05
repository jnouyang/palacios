/*
 * Palacios XPMEM interface 
 * (c) Brian Kocoloski, 2014
 */

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/poll.h>
#include <linux/anon_inodes.h>

#include <xpmem_ns.h>

#include "palacios.h"
#include "vm.h"
#include "mm.h"
#include "linux-exts.h"

#define sint64_t int64_t
#include <interfaces/vmm_xpmem.h>


struct xpmem_cmd_state {
    struct list_head xpmem_list;

    spinlock_t lock;
    wait_queue_head_t waitq;
};

struct xpmem_cmd_iter {
    struct xpmem_cmd_ex * cmd;
    struct list_head node;
};

struct host_xpmem_state {
    struct v3_guest * guest;

    struct xpmem_cmd_state cmd_state;

    spinlock_t lock;
    int connected;

    /* Pointer to internal Palacios state */
    struct v3_xpmem_state * v3_xpmem;
}; 

static struct file_operations xpmem_fops;

static int xpmem_connect(struct v3_guest * guest, unsigned int cmd, unsigned long arg,
        void * priv_data) {

    struct host_xpmem_state * state = (struct host_xpmem_state *)priv_data;
    unsigned long flags;
    int xpmem_fd;
    int acquired = 0;

    spin_lock_irqsave(&(state->lock), flags);
    if (state->connected == 0) {
        state->connected = 1;
        acquired = 1;
    }
    spin_unlock_irqrestore(&(state->lock), flags);

    if (acquired == 0) {
        ERROR("XPMEM already connected\n");
        return -1;
    }

    xpmem_fd = anon_inode_getfd("v3-xpmem", &xpmem_fops, state, O_RDWR);
    if (xpmem_fd < 0) {
        ERROR("Error creating XPMEM inode\n");
        return xpmem_fd;
    }

    return xpmem_fd;
}

static void * palacios_xpmem_host_connect(void * private_data, struct v3_xpmem_state * v3_xpmem) {
    struct v3_guest * guest = (struct v3_guest *)private_data;
    struct host_xpmem_state * state = NULL;
    struct xpmem_cmd_state * cmd_state = NULL;

    if (!guest) {
        ERROR("Cannot initialize XPMEM channel for NULL guest\n");
        return NULL;
    }

    state = palacios_kmalloc(sizeof(struct host_xpmem_state), GFP_KERNEL);
    if (!state) {
        ERROR("Cannot allocate memory for host_xpmem_state\n");
        return NULL;
    }

    cmd_state = &(state->cmd_state);
    INIT_LIST_HEAD(&(cmd_state->xpmem_list));
    init_waitqueue_head(&(cmd_state->waitq));
    spin_lock_init(&(cmd_state->lock));
    spin_lock_init(&(state->lock));

    state->connected = 0;
    state->guest = guest;
    state->v3_xpmem = v3_xpmem;

    add_guest_ctrl(guest, V3_VM_XPMEM_CONNECT, xpmem_connect, state);

    v3_lnx_printk("Guest initialized XPMEM host channel (Guest=%s)\n", guest->name);

    return state;

}

static int palacios_xpmem_host_disconnect(void * private_data) {
    struct host_xpmem_state * state = (struct host_xpmem_state *)private_data;
    struct xpmem_cmd_state * cmd_state = &(state->cmd_state);

    if (!state) {
        ERROR("Cannot disconnect NULL XPMEM state\n");
        return -1;
    }

    if (!state->guest) {
        ERROR("Cannot disconnect XPMEM state for NULL guest\n");
        return -1;
    }

    remove_guest_ctrl(state->guest, V3_VM_XPMEM_CONNECT);

    /* Free lists */
    {
        struct xpmem_cmd_iter * req_iter, * req_next;
        unsigned long flags;

        spin_lock_irqsave(&(cmd_state->lock), flags);
        list_for_each_entry_safe(req_iter, req_next, &(cmd_state->xpmem_list), node) {
            list_del(&(req_iter->node));
            palacios_kfree(req_iter);
        }
        spin_unlock_irqrestore(&(cmd_state->lock), flags);
    }

    palacios_kfree(state);

    return 0;
}


static int palacios_xpmem_command(void * private_data, struct xpmem_cmd_ex * cmd) {
    struct host_xpmem_state * state = (struct host_xpmem_state *)private_data;
    struct xpmem_cmd_state * cmd_state = &(state->cmd_state);
    struct xpmem_cmd_iter * iter = NULL;
    unsigned long flags;

    iter = palacios_kmalloc(sizeof(struct xpmem_cmd_iter), GFP_KERNEL);
    if (!iter) {
        ERROR("Cannot allocate memory for list iterator\n");
        return -1;
    }
    
    iter->cmd = cmd;
    spin_lock_irqsave(&(cmd_state->lock), flags);
    list_add_tail(&(iter->node), &(cmd_state->xpmem_list));
    wake_up_interruptible(&(cmd_state->waitq));
    spin_unlock_irqrestore(&(cmd_state->lock), flags);

    return 0;
}

static struct v3_xpmem_hooks palacios_xpmem_hooks = {
    .xpmem_host_connect     = palacios_xpmem_host_connect,
    .xpmem_host_disconnect  = palacios_xpmem_host_disconnect,
    .xpmem_command          = palacios_xpmem_command,
};

static int xpmem_open_op(struct inode * inodep, struct file * filp) {
    return 0;
}

static int xpmem_release_op(struct inode * inodep, struct file * filp) {
    struct host_xpmem_state * state = (struct host_xpmem_state *)filp->private_data;
    unsigned long flags;

    spin_lock_irqsave(&(state->lock), flags);
    state->connected = 0;
    spin_unlock_irqrestore(&(state->lock), flags);

    return 0;
}

static ssize_t xpmem_read_op(struct file * filp, char __user * buffer, size_t size, loff_t * offp) {
    struct host_xpmem_state * state = (struct host_xpmem_state *)filp->private_data;
    struct xpmem_cmd_state * cmd_state = &(state->cmd_state);
    struct xpmem_cmd_iter * iter;
    ssize_t ret = 0;
    unsigned long flags;

    spin_lock_irqsave(&(cmd_state->lock), flags);
    if (list_empty(&(cmd_state->xpmem_list))) {
        spin_unlock_irqrestore(&(cmd_state->lock), flags);
        return 0;
    }

    iter = list_first_entry(&(cmd_state->xpmem_list), struct xpmem_cmd_iter, node);
    list_del(&(iter->node));
    spin_unlock_irqrestore(&(cmd_state->lock), flags);

    if (size > sizeof(struct xpmem_cmd_ex)) {
        size = sizeof(struct xpmem_cmd_ex);
    }

    ret = size;

    if (copy_to_user(buffer, (void *)iter->cmd, size)) {
        ERROR("Cannot copy XPMEM request to user\n");
        ret = -EFAULT;
    }   
    
    palacios_kfree(iter->cmd);
    palacios_kfree(iter);
    return ret;
}


static ssize_t xpmem_write_op(struct file * filp, const char __user * buffer, size_t size, loff_t * offp) {
    struct host_xpmem_state * state = (struct host_xpmem_state *)filp->private_data;
    struct xpmem_cmd_ex * cmd = palacios_kmalloc(sizeof(struct xpmem_cmd_ex), GFP_KERNEL);

    if (!cmd) {
        ERROR("Cannot allocate memory for XPMEM command\n");
        return -ENOMEM;
    }

    if (size != sizeof(struct xpmem_cmd_ex)) {
        palacios_kfree(cmd);
        ERROR("Invalid command size\n");
        return -EFAULT;
    }

    if (copy_from_user((void *)cmd, buffer, size)) {
        palacios_kfree(cmd);
        ERROR("Cannot copy XPMEM request from user\n");
        return -EFAULT;
    }

    if (cmd->type == XPMEM_ATTACH_COMPLETE) {
        printk("Palacios PFN list:\n");
        {
            u64 i = 0;
            for (i = 0; i < cmd->attach.num_pfns; i++) {
                printk("%llu  ", (unsigned long long)cmd->attach.pfns[i]);
            }
            printk("\n");
        }
    }

    switch (cmd->type) {
        case XPMEM_GET:
        case XPMEM_RELEASE:
        case XPMEM_ATTACH:
        case XPMEM_DETACH:
        case XPMEM_MAKE_COMPLETE: 
        case XPMEM_REMOVE_COMPLETE:
        case XPMEM_GET_COMPLETE:
        case XPMEM_RELEASE_COMPLETE:
        case XPMEM_ATTACH_COMPLETE: 
        case XPMEM_DETACH_COMPLETE: 
            V3_xpmem_command(state->v3_xpmem, cmd);
            break;
        
        default:
            palacios_kfree(cmd);
            ERROR("Cannot handle XPMEM write - not a valid command structure (%d)\n", cmd->type);
            return -EFAULT;
    }
    
    return size;
}

static unsigned int xpmem_poll_op(struct file * filp, struct poll_table_struct * pollp) {
    struct host_xpmem_state * state = (struct host_xpmem_state *)filp->private_data;
    struct xpmem_cmd_state * cmd_state = &(state->cmd_state);
    unsigned int ret = 0;
    unsigned long flags;

    poll_wait(filp, &(cmd_state->waitq), pollp);

    ret = POLLOUT | POLLWRNORM;

    spin_lock_irqsave(&(cmd_state->lock), flags);
    if (!list_empty(&(cmd_state->xpmem_list))) {
        ret |= (POLLIN | POLLRDNORM);
    }
    spin_unlock_irqrestore(&(cmd_state->lock), flags);

    return ret;
}


static struct file_operations xpmem_fops = {
    .open       = xpmem_open_op,
    .release    = xpmem_release_op,
    .read       = xpmem_read_op,
    .write      = xpmem_write_op,
    .poll       = xpmem_poll_op,
};


static int init_xpmem(void) {
    V3_Init_Xpmem(&palacios_xpmem_hooks);
    return 0;
}


static struct linux_ext xpmem_ext = {
    .name = "XPMEM_INTERFACE",
    .init = init_xpmem,
    .deinit = NULL,
    .guest_init = NULL,
    .guest_deinit = NULL,
};

register_extension(&xpmem_ext);
