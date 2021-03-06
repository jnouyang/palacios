/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_dev_mgr.h>
#include <palacios/vm.h>
#include <palacios/vmm.h>
#include <palacios/vmm_decoder.h>



#ifndef V3_CONFIG_DEBUG_DEV_MGR
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


static struct hashtable * master_dev_table = NULL;

static uint_t 
dev_hash_fn(addr_t key) 
{
    char * name = (char *)key;
    return v3_hash_buffer((uint8_t *)name, strlen(name));
}

static int 
dev_eq_fn(addr_t key1, addr_t key2) 
{
    char * name1 = (char *)key1;
    char * name2 = (char *)key2;
    
    return (strcmp(name1, name2) == 0);
}


int 
V3_init_devices() 
{
    extern struct v3_device_info   __start__v3_devices[];
    extern struct v3_device_info   __stop__v3_devices[];
    struct        v3_device_info * tmp_dev =  __start__v3_devices;
    int i = 0;

#ifdef V3_CONFIG_DEBUG_DEV_MGR
    {
	int num_devices = (__stop__v3_devices - __start__v3_devices) / sizeof(struct v3_device_info);
	PrintDebug("%d Virtual devices registered with Palacios\n", num_devices);
    }
#endif

    PrintDebug("Start addres=%p, Stop address=%p\n", __start__v3_devices, __stop__v3_devices);

    master_dev_table = v3_create_htable(0, dev_hash_fn, dev_eq_fn);



    while (tmp_dev != __stop__v3_devices) {
	V3_Print("Registering Device: %s\n", tmp_dev->name);

	if (v3_htable_search(master_dev_table, (addr_t)(tmp_dev->name))) {
	    PrintError("Multiple instance of device (%s)\n", tmp_dev->name);
	    return -1;
	}

	if (v3_htable_insert(master_dev_table, 
			     (addr_t)(tmp_dev->name), 
			     (addr_t)(tmp_dev->init)) == 0) {
	    PrintError("Could not add device %s to master list\n", tmp_dev->name);
	    return -1;
	}

	tmp_dev = &(__start__v3_devices[++i]);
    }


    return 0;
}


int 
V3_deinit_devices() 
{    
    v3_free_htable(master_dev_table, 0, 0);
    return 0;
}


int 
v3_init_dev_mgr(struct v3_vm_info * vm) 
{
    struct vmm_dev_mgr * mgr = &(vm->dev_mgr);

    INIT_LIST_HEAD(&(mgr->dev_list));
    mgr->num_devs = 0;

    mgr->dev_table = v3_create_htable(0, dev_hash_fn, dev_eq_fn);

    INIT_LIST_HEAD(&(mgr->blk_list));
    INIT_LIST_HEAD(&(mgr->net_list));
    INIT_LIST_HEAD(&(mgr->char_list));
    INIT_LIST_HEAD(&(mgr->cons_list));

    mgr->blk_table  = v3_create_htable(0, dev_hash_fn, dev_eq_fn);
    mgr->net_table  = v3_create_htable(0, dev_hash_fn, dev_eq_fn);
    mgr->char_table = v3_create_htable(0, dev_hash_fn, dev_eq_fn);
    mgr->cons_table = v3_create_htable(0, dev_hash_fn, dev_eq_fn);
    
    return 0;
}


int 
v3_free_vm_devices(struct v3_vm_info * vm) 
{
    struct vmm_dev_mgr * mgr = &(vm->dev_mgr);
    struct vm_device   * dev = NULL;
    struct vm_device   * tmp = NULL;

    list_for_each_entry_safe(dev, tmp, &(mgr->dev_list), dev_link) {
	V3_Print("Removing Device: %s\n", dev->name); 
	v3_remove_device(dev);
    }

    return 0;
}


static int free_frontends(struct v3_vm_info  * vm,
			  struct vmm_dev_mgr * mgr);

int 
v3_deinit_dev_mgr(struct v3_vm_info * vm) 
{
    struct vmm_dev_mgr * mgr = &(vm->dev_mgr);
    
    // clear frontend lists

    free_frontends(vm, mgr);

    v3_free_htable(mgr->dev_table, 0, 0);

    return 0;
}



int 
v3_create_device(struct v3_vm_info * vm, 
		 const char        * dev_name, 
		 v3_cfg_tree_t     * cfg) 
{
    int (*dev_init)(struct v3_vm_info * vm, void * cfg_data);

    dev_init = (void *)v3_htable_search(master_dev_table, (addr_t)dev_name);

    if (dev_init == NULL) {
	PrintError("Could not find device %s in master device table\n", dev_name);
	return -1;
    }


    if (dev_init(vm, cfg) == -1) {
	PrintError("Could not initialize Device %s\n", dev_name);
	return -1;
    }

    return 0;
}




struct vm_device * 
v3_find_dev(struct v3_vm_info * vm,
	    const char        * dev_name) 
{
    struct vmm_dev_mgr * mgr = &(vm->dev_mgr);

    if (!dev_name) {
	return NULL;
    }

    return (struct vm_device *)v3_htable_search(mgr->dev_table, (addr_t)dev_name);
}


/****************************************************************/
/* The remaining functions are called by the devices themselves */
/****************************************************************/

typedef enum {DEV_IO_HOOK, DEV_MSR_HOOK, DEV_CPUID_HOOK, DEV_MEM_HOOK, DEV_HCALL_HOOK} dev_rsrc_type_t;

struct dev_rsrc {
    dev_rsrc_type_t type;
    uint64_t rsrc;

    struct list_head node;
};



static int 
add_resource(struct vm_device * dev, 
	     dev_rsrc_type_t    type, 
	     uint64_t           rsrc_id) 
{
    struct dev_rsrc * resource = NULL;

    resource = V3_Malloc(sizeof(struct dev_rsrc));

    if (resource == NULL) {
	PrintError("Error: Could not allocate device resource\n");
	return -1;
    }

    resource->rsrc = rsrc_id;
    resource->type = type;

    list_add(&(resource->node), &(dev->res_hooks));
    return 0;
}

static int 
free_resource(struct vm_device * dev, 
	      dev_rsrc_type_t    type, 
	      uint64_t           rsrc_id) 
{
    struct dev_rsrc * resource = NULL;
    struct dev_rsrc * tmp;

    list_for_each_entry_safe(resource, tmp, &(dev->res_hooks), node) {
	if ((resource->type == type) && 
	    (resource->rsrc == rsrc_id)) {

	    list_del(&(resource->node));
	    V3_Free(resource);
	    
	    return 0;
	}
    }

    return -1;
}


int 
v3_dev_hook_io(struct vm_device * dev, 
	       uint16_t           port,
	       int (*read) (struct v3_core_info * core, uint16_t port, void * dst, uint_t length, void * priv_data),
	       int (*write)(struct v3_core_info * core, uint16_t port, void * src, uint_t length, void * priv_data)) 
{
    int ret = 0;
    
    ret = v3_hook_io_port(dev->vm, port, 
			  (int (*)(struct v3_core_info * core, uint16_t, void *, uint_t, void *))read, 
			  (int (*)(struct v3_core_info * core, uint16_t, void *, uint_t, void *))write, 
			  (void *)dev->private_data);

    if (ret == -1) {
	return -1;
    }

    if (add_resource(dev, DEV_IO_HOOK, port) == -1) {
	v3_unhook_io_port(dev->vm, port);
	PrintError("Could not allocate io hook dev state\n");
	return -1;
    }
    
    return 0;
}


int 
v3_dev_unhook_io(struct vm_device * dev, 
		 uint16_t           port) 
{
    if (free_resource(dev, DEV_IO_HOOK, port) == 0) {
	return v3_unhook_io_port(dev->vm, port);	   
    } 
    
    return -1;
}


int 
v3_dev_hook_msr(struct vm_device * dev, 
		uint32_t           msr,
		int (*read) (struct v3_core_info * core, uint32_t msr, struct v3_msr * dst, void * priv_data),
		int (*write)(struct v3_core_info * core, uint32_t msr, struct v3_msr   src, void * priv_data)) 
{
    int ret = 0;

    ret = v3_hook_msr(dev->vm, msr, read, write, dev->private_data);

    if (ret == -1) {
	return -1;
    }

    if (add_resource(dev, DEV_MSR_HOOK, msr) == -1) {
	v3_unhook_msr(dev->vm, msr);
	return -1;
    }

    return 0;
}
		  
int 
v3_dev_unhook_msr(struct vm_device * dev, 
		  uint32_t           msr) 
{
    if (free_resource(dev, DEV_MSR_HOOK, msr) == 0) {
	return v3_unhook_msr(dev->vm, msr);
    }

    return -1;
}




int 
v3_remove_device(struct vm_device * dev) 
{
    struct vmm_dev_mgr * mgr      = &(dev->vm->dev_mgr);
    struct dev_rsrc    * resource = NULL;
    struct dev_rsrc    * tmp      = NULL;

    list_for_each_entry_safe(resource, tmp, &(dev->res_hooks), node) {
	if (resource->type == DEV_IO_HOOK) {
	    v3_unhook_io_port(dev->vm, (uint16_t)(resource->rsrc));
	} else if (resource->type == DEV_MSR_HOOK) {
	    v3_unhook_msr(dev->vm, (uint32_t)(resource->rsrc));
	}

	list_del(&(resource->node));
	V3_Free(resource);    
    }

    if (dev->ops->free) {
	dev->ops->free(dev->private_data);
    } else {
	PrintError("Error: %s free() not implemented\n",  dev->name);
    }

    list_del(&(dev->dev_link));
    mgr->num_devs--;

    dev->vm = NULL;

    V3_Free(dev);
    return -1;
}


struct vm_device * 
v3_add_device(struct v3_vm_info    * vm,
	      char                 * name, 
	      struct v3_device_ops * ops, 
	      void                 * private_data) 
{
    struct vmm_dev_mgr * mgr = &(vm->dev_mgr);
    struct vm_device   * dev = NULL;


    // Check if we already registered a device of the same name
    if (v3_htable_search(mgr->dev_table, (addr_t)name) != (addr_t)NULL) {
	PrintError("Device with name (%s) already registered with VM\n", name); 
	return NULL;
    }

    dev = (struct vm_device *)V3_Malloc(sizeof(struct vm_device));

    if (dev == NULL) {
	PrintError("Cannot allocate in adding a device\n");
	return NULL;
    }

    INIT_LIST_HEAD(&(dev->res_hooks));

    strncpy(dev->name, name, 32);
    dev->ops          = ops;
    dev->private_data = private_data;
    dev->vm           = vm;

    list_add(&(dev->dev_link), &(mgr->dev_list));
    mgr->num_devs++;

    v3_htable_insert(mgr->dev_table, (addr_t)(dev->name), (addr_t)dev);

    return dev;
}


void 
v3_print_dev_mgr(struct v3_vm_info * vm) 
{
    struct vmm_dev_mgr * mgr = &(vm->dev_mgr);
    struct vm_device   * dev = NULL;

    V3_Print("%d devices registered with manager\n", mgr->num_devs);

    list_for_each_entry(dev, &(mgr->dev_list), dev_link) {
	V3_Print("Device: %s\n", dev->name);
    }

    return;
}




struct blk_frontend {
    int (*connect)(struct v3_vm_info      * vm, 
		    void                  * frontend_data, 
		    struct v3_dev_blk_ops * ops, 
		    v3_cfg_tree_t         * cfg, 
		    void                  * priv_data);
	

    struct list_head blk_node;

    void * priv_data;
};



int 
v3_dev_add_blk_frontend(struct v3_vm_info * vm, 
			char              * name, 
			int              (*connect)(struct v3_vm_info     * vm, 
						    void                  * frontend_data, 
						    struct v3_dev_blk_ops * ops, 
						    v3_cfg_tree_t         * cfg, 
						    void                  * priv_data), 
			void              * priv_data) 
{

    struct blk_frontend * frontend = NULL;

    frontend = (struct blk_frontend *)V3_Malloc(sizeof(struct blk_frontend));

    if (!frontend) {
	PrintError("Cannot allocate in adding a block front end\n");
	return -1;
    }

    memset(frontend, 0, sizeof(struct blk_frontend));
    
    frontend->connect   = connect;
    frontend->priv_data = priv_data;
	
    list_add(&(frontend->blk_node), &(vm->dev_mgr.blk_list));
    v3_htable_insert(vm->dev_mgr.blk_table, (addr_t)(name), (addr_t)frontend);

    return 0;
}

int 
v3_dev_connect_blk(struct v3_vm_info     * vm, 
		   char                  * frontend_name, 
		   struct v3_dev_blk_ops * ops, 
		   v3_cfg_tree_t         * cfg, 
		   void                  * private_data) 
{

    struct blk_frontend * frontend = NULL;

    frontend = (struct blk_frontend *)v3_htable_search(vm->dev_mgr.blk_table,
						       (addr_t)frontend_name);
    
    if (frontend == NULL) {
	PrintError("Could not find frontend blk device %s\n", frontend_name);
	return 0;
    }

    if (frontend->connect(vm, frontend->priv_data, ops, cfg, private_data) == -1) {
	PrintError("Error connecting to block frontend %s\n", frontend_name);
	return -1;
    }

    return 0;
}



struct net_frontend {
    int (*connect)(struct v3_vm_info      * vm, 
		    void                  * frontend_data, 
		    struct v3_dev_net_ops * ops, 
		    v3_cfg_tree_t         * cfg, 
		    void                  * priv_data);
    

    struct list_head net_node;

    void * priv_data;
};


int 
v3_dev_add_net_frontend(struct v3_vm_info * vm,
			char              * name, 
			int              (*connect)(struct v3_vm_info     * vm, 
						    void                  * frontend_data, 
						    struct v3_dev_net_ops * ops, 
						    v3_cfg_tree_t         * cfg, 
						    void                  * private_data), 
			void              * priv_data)
{
    struct net_frontend * frontend = NULL;

    frontend = (struct net_frontend *)V3_Malloc(sizeof(struct net_frontend));

    if (!frontend) {
	PrintError("Cannot allocate in adding a net front end\n");
	return -1;
    }

    memset(frontend, 0, sizeof(struct net_frontend));
    
    frontend->connect   = connect;
    frontend->priv_data = priv_data;
	
    list_add(&(frontend->net_node), &(vm->dev_mgr.net_list));
    v3_htable_insert(vm->dev_mgr.net_table, (addr_t)(name), (addr_t)frontend);

    return 0;
}


int 
v3_dev_connect_net(struct v3_vm_info     * vm, 
		   char                  * frontend_name, 
		   struct v3_dev_net_ops * ops, 
		   v3_cfg_tree_t         * cfg, 
		   void                  * private_data)
{
    struct net_frontend * frontend = NULL;

    frontend = (struct net_frontend *)v3_htable_search(vm->dev_mgr.net_table,
						       (addr_t)frontend_name);
    
    if (frontend == NULL) {
	PrintError("Could not find frontend net device %s\n", frontend_name);
	return 0;
    }

    if (frontend->connect(vm, frontend->priv_data, ops, cfg, private_data) == -1) {
	PrintError("Error connecting to net frontend %s\n", frontend_name);
	return -1;
    }

    return 0;
}


struct cons_frontend {
    int (*connect)(struct v3_vm_info         * vm, 
		   void                      * frontend_data, 
		   struct v3_dev_console_ops * ops, 
		   v3_cfg_tree_t             * cfg, 
		   void                      * priv_data);
    

    struct list_head cons_node;

    void * priv_data;
};

int 
v3_dev_add_console_frontend(struct v3_vm_info * vm, 
			    char              * name, 
			    int              (*connect)(struct v3_vm_info         * vm, 
							void                      * frontend_data, 
							struct v3_dev_console_ops * ops, 
							v3_cfg_tree_t             * cfg, 
							void                      * private_data), 
			    void              * priv_data)
{
    struct cons_frontend * frontend = NULL;

    frontend = (struct cons_frontend *)V3_Malloc(sizeof(struct cons_frontend));

    if (!frontend) {
	PrintError("Cannot allocate in adding a console front end\n");
	return -1;
    }

    memset(frontend, 0, sizeof(struct cons_frontend));
    
    frontend->connect   = connect;
    frontend->priv_data = priv_data;
	
    list_add(&(frontend->cons_node), &(vm->dev_mgr.cons_list));
    v3_htable_insert(vm->dev_mgr.cons_table, (addr_t)(name), (addr_t)frontend);

    return 0;
}


int 
v3_dev_connect_console(struct v3_vm_info         * vm, 
		       char                      * frontend_name, 
		       struct v3_dev_console_ops * ops, 
		       v3_cfg_tree_t             * cfg, 
		       void                      * private_data)
{
    struct cons_frontend * frontend = NULL;

    frontend = (struct cons_frontend *)v3_htable_search(vm->dev_mgr.cons_table,
							(addr_t)frontend_name);
    
    if (frontend == NULL) {
	PrintError("Could not find frontend console device %s\n", frontend_name);
	return 0;
    }
    
    if (frontend->connect(vm, frontend->priv_data, ops, cfg, private_data) == -1) {
	PrintError("Error connecting to console frontend %s\n", frontend_name);
	return -1;
    }

    return 0;
}

struct char_frontend {
    int (*connect)(struct v3_vm_info      * vm, 
		   void                   * frontend_data, 
		   struct v3_dev_char_ops * ops, 
		   v3_cfg_tree_t          * cfg, 
		   void                   * priv_data, 
		   void                  ** push_fn_arg);
    

    struct list_head char_node;

    void * priv_data;
};

int 
v3_dev_add_char_frontend(struct v3_vm_info * vm, 
			 char              * name, 
			 int              (*connect)(struct v3_vm_info      * vm, 
						     void                   * frontend_data, 
						     struct v3_dev_char_ops * ops, 
						     v3_cfg_tree_t          * cfg, 
						     void                   * private_data, 
						     void                  ** push_fn_arg), 
			 void              * priv_data)
{
    struct char_frontend * frontend = NULL;

    frontend = (struct char_frontend *)V3_Malloc(sizeof(struct char_frontend));

    if (!frontend) {
	PrintError("Cannot allocate in adding a char front end\n");
	return -1;
    }

    memset(frontend, 0, sizeof(struct char_frontend));
    
    frontend->connect   = connect;
    frontend->priv_data = priv_data;
	
    list_add(&(frontend->char_node), &(vm->dev_mgr.char_list));
    v3_htable_insert(vm->dev_mgr.char_table, (addr_t)(name), (addr_t)frontend);

    return 0;
}


int 
v3_dev_connect_char(struct v3_vm_info      * vm, 
		    char                   * frontend_name, 
		    struct v3_dev_char_ops * ops, 
		    v3_cfg_tree_t          * cfg, 
		    void                   * private_data, 
		    void                  ** push_fn_arg)
{
    struct char_frontend * frontend = NULL;

    frontend = (struct char_frontend *)v3_htable_search(vm->dev_mgr.char_table,
							(addr_t)frontend_name);
    
    if (frontend == NULL) {
	PrintError("Could not find frontend char device %s\n", frontend_name);
	return 0;
    }
    
    if (frontend->connect(vm, frontend->priv_data, ops, cfg, private_data, push_fn_arg) == -1) {
	PrintError("Error connecting to char frontend %s\n", frontend_name);
	return -1;
    }

    return 0;
}



static int 
free_frontends(struct v3_vm_info  * vm,
	       struct vmm_dev_mgr * mgr) 
{
    struct char_frontend * chr       = NULL;
    struct char_frontend * tmp_chr   = NULL;
    struct cons_frontend * cons      = NULL;
    struct cons_frontend * tmp_cons  = NULL;
    struct net_frontend  * net       = NULL;
    struct net_frontend  * tmp_net   = NULL;
    struct blk_frontend  * blk       = NULL;
    struct blk_frontend  * tmp_blk   = NULL;



    list_for_each_entry_safe(chr,  tmp_chr,  &(mgr->char_list), char_node) {
	list_del(&(chr->char_node));
	V3_Free(chr);
    }

    list_for_each_entry_safe(cons, tmp_cons, &(mgr->cons_list), cons_node) {
	list_del(&(cons->cons_node));
	V3_Free(cons);
    }

    list_for_each_entry_safe(net,  tmp_net,  &(mgr->net_list),  net_node) {
	list_del(&(net->net_node));
	V3_Free(net);
    }

    list_for_each_entry_safe(blk,  tmp_blk,  &(mgr->blk_list),  blk_node) {
	list_del(&(blk->blk_node));
	V3_Free(blk);
    }

    v3_free_htable(mgr->blk_table,  0, 0);
    v3_free_htable(mgr->net_table,  0, 0);
    v3_free_htable(mgr->char_table, 0, 0);
    v3_free_htable(mgr->cons_table, 0, 0);


    return 0;
}
