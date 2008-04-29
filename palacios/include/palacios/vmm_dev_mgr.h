#ifndef _VMM_DEV_MGR
#define _VMM_DEV_MGR

#include <palacios/vmm_types.h>
#include <palacios/vmm_list.h>
#include <palacios/vmm_string.h>

struct vm_device;
struct guest_info;


struct vmm_dev_mgr {
  uint_t num_devs;
  struct list_head dev_list;

  uint_t num_io_hooks;
  struct list_head io_hooks;
  
  uint_t num_mem_hooks;
  struct list_head mem_hooks;
  /*
  uint_t num_irq_hooks;
  struct list_head irq_hooks;
  */
};



struct dev_io_hook {
  ushort_t port;
  
  int (*read)(ushort_t port, void * dst, uint_t length, struct vm_device * dev);
  int (*write)(ushort_t port, void * src, uint_t length, struct vm_device * dev);

  struct vm_device * dev;

  // Do not touch anything below this  
  /*
    struct dev_io_hook *dev_next, *dev_prev;
    struct dev_io_hook *mgr_next, *mgr_prev;
  */
  struct list_head dev_list;
  struct list_head mgr_list;
};

struct dev_mem_hook {
  void  *addr_start;
  void  *addr_end;

  struct vm_device * dev;

  // Do not touch anything below this
  struct list_head dev_list;
  struct list_head mgr_list;
};

/*
struct dev_irq_hook {
  uint_t irq;

  int (*handler)(uint_t irq, struct vm_device * dev);

  struct vm_device * dev;

  struct list_head dev_list;
  struct list_head mgr_list;
};

*/
// Registration of devices

//
// The following device manager functions should only be called
// when the guest is stopped
//

int dev_mgr_init(struct vmm_dev_mgr *mgr);
int dev_mgr_deinit(struct vmm_dev_mgr * mgr);



int attach_device(struct guest_info *vm, struct vm_device * dev);
int unattach_device(struct vm_device *dev);


int dev_mgr_add_device(struct vmm_dev_mgr * mgr, struct vm_device * dev);
int dev_mgr_remove_device(struct vmm_dev_mgr * mgr, struct vm_device * dev);


/*
  int dev_mgr_add_io_hook(struct vmm_dev_mgr * mgr, struct dev_io_hook * hook);
  int dev_mgr_remove_io_hook(struct vmm_dev_mgr * mgr, struct dev_io_hook * hook);
  int dev_add_io_hook(struct vmm_dev_mgr * mgr, struct dev_io_hook * hook);
  int dev_remove_io_hook(struct vmm_dev_mgr * mgr, struct dev_io_hook * hook);
  struct dev_io_hook * dev_find_io_hook(struct vm_device * dev, ushort_t port);
  struct dev_io_hook * dev_mgr_find_io_hook(struct vmm_dev_mgr * mgr, ushort_t port)
*/

void PrintDebugDevMgr(struct vmm_dev_mgr * mgr);
void PrintDebugDev(struct vm_device * dev);
void PrintDebugDevIO(struct vm_device * dev);
void PrintDebugDevMgrIO(struct vmm_dev_mgr * mgr);

#endif