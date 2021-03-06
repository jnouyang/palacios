/* 
 * V3 Control library functions
 * (c) 2015, Jack lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "v3vee.h"
#include "v3_ioctl.h"

#include <pet_mem.h>
#include <pet_log.h>
#include <pet_ioctl.h>
#include <pet_numa.h>
#include <pet_pci.h>

int 
v3_is_vmm_present()
{
    int fd = 0;

    fd = open(V3_DEV_FILENAME, O_RDWR);

    if (fd < 0) {
	return 0;
    }

    close(fd);
    return 1;
}

int 
v3_shutdown(void)
{
    return pet_ioctl_path(V3_DEV_FILENAME, V3_SHUTDOWN, IOCTL_ARG(0));
}


int 
v3_add_cpu(int cpu_id)
{
    return pet_ioctl_path(V3_DEV_FILENAME, V3_ADD_CPU, IOCTL_ARG(cpu_id));
}

int 
v3_remove_cpu(int cpu_id)
{
    return pet_ioctl_path(V3_DEV_FILENAME, V3_REMOVE_CPU, IOCTL_ARG(cpu_id));
}




int 
v3_add_mem_node(int numa_zone)
{
    struct v3_mem_region   mem_range;
    struct mem_block    * block_arr = NULL;
    int ret = 0;
    int i   = 0;
    int numa_num_blocks = pet_num_blocks(numa_zone);
    
    block_arr = calloc(numa_num_blocks, sizeof(struct mem_block));
 
    ret = pet_offline_mem_node(numa_zone, block_arr);

    for (i = 0; i < numa_num_blocks; i++) {
	mem_range.base_addr = block_arr[i].base_addr;
	mem_range.num_pages = block_arr[i].pages;
	
	printf("Adding memory range (%p) to Palacios\n", 
	       (void *)mem_range.base_addr);
	
	if (pet_ioctl_path(V3_DEV_FILENAME, V3_ADD_MEM, IOCTL_ARG(&mem_range)) == -1) {
	    ERROR("Could not add memory to Palacios\n");
	}
    }
    
    free(block_arr);
    
    return ret;
}

int
v3_remove_mem_node(int numa_zone) 
{



    return -1;
}


int 
v3_add_mem(int num_blocks, 
	   int numa_zone) 
{
    struct v3_mem_region   mem_range;
    struct mem_block    * block_arr = NULL;
    int i   = 0;
    int ret = 0;
    
    block_arr = calloc(num_blocks, sizeof(struct mem_block));
    ret       = pet_offline_blocks(num_blocks, numa_zone, block_arr);
    
    if (ret != num_blocks) {
	ERROR("Could not allocate %d memory blocks (ret = %d)\n", num_blocks, ret);
	
	pet_online_blocks(ret, block_arr);
	free(block_arr);
	
	return -1;
    }
    
    for (i = 0; i < num_blocks; i++) {
	mem_range.base_addr = block_arr[i].base_addr;
	mem_range.num_pages = block_arr[i].pages;
	
	printf("Adding memory range (%p) to Palacios\n", 
	       (void *)mem_range.base_addr);
	
	if (pet_ioctl_path(V3_DEV_FILENAME, V3_ADD_MEM, IOCTL_ARG(&mem_range)) == -1) {
	    ERROR("Could not add memory to Palacios\n");
	}
    }
    
    free(block_arr);
    
    return 0;
}



int 
v3_remove_mem(int num_blocks,
	      int numa_zone)
{
    int blocks_freed = 0;

    if (num_blocks == 0) {
	return 0;
    }

    if (numa_zone >= 0) {
	char * proc_filename = NULL;
	FILE * proc_file     = NULL;
	char * line          = NULL;
	size_t size          = 0;

	if (asprintf(&proc_filename, "/proc/v3vee/v3-mem%d", numa_zone) == -1) {
	    ERROR("asprintf failed\n");
	    return -1;
	}

	proc_file = fopen(proc_filename, "r");
	
	free(proc_filename);

	if (proc_file == NULL) {
	    ERROR("Could not open proc file for numa zone %d\n", numa_zone);
	    return -1;
	}
	
	printf("Searching for memory pools\n");

	while (getline(&line, &size, proc_file) != -1) {
	    if (strstr(line, "memory pools") != NULL) break;
	}


	printf("iterating over blocks\n");
	while (getline(&line, &size, proc_file) != -1) {
	    u64 base_addr = 0;
	    u32 order     = 0;
	    u32 size      = 0;
	    u32 freed     = 0;
	    
	    int matched   = 0;


	    matched = sscanf(line, "    Base Addr=%llx, order=%u, size=%u, free=%u", 
			     &base_addr, &order, &size, &freed);

	    if (matched == 0) {
		ERROR("Could not match memory pool string\n");
		break;
	    }

	    if (pet_block_size() != (1 << order)) {
		printf("Pool (%p) is not a block\n", (void *)base_addr);
		continue;
	    }


	    printf("base_addr=%llx, order=%u, size=%u, free=%u\n", 
		   base_addr, order, size, freed);


	    if (freed == size) {
		// block is clear, remove it
		printf("attempting to remove block at %p\n", (void *)base_addr);

		
		if (pet_ioctl_path(V3_DEV_FILENAME, V3_REMOVE_MEM, IOCTL_ARG(base_addr)) == -1) {
		    ERROR("Could not remove memory from Palacios\n");
		    continue;
		}
		
		blocks_freed++;

		if (pet_online_block(base_addr / pet_block_size()) == -1) {
		    ERROR("Block removed from Palacios, but not onlined\n");
		}
	    }


	    if (blocks_freed == num_blocks) break;
	}


	fclose(proc_file);


    } else {
	int i = 0;
	
	for (i = 0; i < pet_num_numa_nodes(); i++) {
	    
	    blocks_freed += v3_remove_mem(num_blocks - blocks_freed, i);
	    
	    if (blocks_freed == num_blocks) break;
	}

    }

    return blocks_freed;
}

int 
v3_add_mem_explicit(int block_id)
{
    struct v3_mem_region mem_range;
  
    if (pet_offline_block(block_id) == -1) {
	ERROR("Could not offline memory block %d\n", block_id);
	return -1;
    }

    mem_range.base_addr = pet_block_size() * block_id;
    mem_range.num_pages = pet_block_size() / 4096;
    
    if (pet_ioctl_path(V3_DEV_FILENAME, V3_ADD_MEM, IOCTL_ARG(&mem_range)) == -1) {
	ERROR("Could not add explicit memory block (block_id: %d)\n", block_id); 
	pet_online_block(block_id);
	return -1;
    }
    
    return 0;
}


int 
v3_add_pci(char * name, 
	   u32    bus, 
	   u32    dev,
	   u32    fn,
	   u8     force)
{
    struct v3_hw_pci_dev dev_spec;

    memset(&dev_spec, 0, sizeof(struct v3_hw_pci_dev));

    dev_spec.bus  = bus;
    dev_spec.dev  = dev;
    dev_spec.func = fn;
    strncpy(dev_spec.url, name, 128);

   if (pet_offline_pci(bus, dev, fn) != 0) {
       ERROR("Could not offline PCI device\n");
       if (!force) return -1;
   }


   if (pet_ioctl_path(V3_DEV_FILENAME, V3_ADD_PCI,  IOCTL_ARG(&dev_spec)) != 0) {
       ERROR("Could not add device to Palacios\n");
       pet_online_pci(bus, dev, fn);
       return -1;
   }
   
   
   return 0;

}


int 
v3_remove_pci(char * name, 
	      u32    bus, 
	      u32    dev, 
	      u32    fn,
	      u8     force)
{
    struct v3_hw_pci_dev dev_spec;

    memset(&dev_spec, 0, sizeof(struct v3_hw_pci_dev));

    dev_spec.bus  = bus;
    dev_spec.dev  = dev;
    dev_spec.func = fn;
    strncpy(dev_spec.url, name, 128);

    if (pet_ioctl_path(V3_DEV_FILENAME, V3_REMOVE_PCI,  IOCTL_ARG(&dev_spec)) != 0) {
	ERROR("Could not remove device from Palacios\n");

	if (!force) return -1;
    }
    
    if (pet_online_pci(bus, dev, fn) != 0) {
	ERROR("Could not online PCI device [%s] (%d:%d.%d)\n", name, bus, dev, fn);
	return -1;
    }


 return 0;

}


int
v3_launch_vm(int vm_id) 
{
    char * dev_path = get_vm_dev_path(vm_id);
    int ret = 0;

    ret = pet_ioctl_path(dev_path, V3_VM_LAUNCH, IOCTL_ARG(NULL)); 

    free(dev_path);

    if (ret < 0) {
	ERROR("Could not launch vm (%d)\n", vm_id);
	return -1;
    }
    
    return 0;
}


int
v3_stop_vm(int vm_id)
{
    char * dev_path = get_vm_dev_path(vm_id);
    int ret = 0;

    ret = pet_ioctl_path(dev_path, V3_VM_STOP, IOCTL_ARG(NULL)); 

    free(dev_path);

    if (ret < 0) {
	ERROR("Could not stop vm (%d)\n", vm_id);
	return -1;
    }
    
    return 0;
}

int 
v3_free_vm(int vm_id)
{
    return pet_ioctl_path(V3_DEV_FILENAME, V3_FREE_GUEST, IOCTL_ARG(vm_id));

}

int 
v3_continue_vm(int vm_id)
{
    char * dev_path = get_vm_dev_path(vm_id);
    int ret = 0;

    ret = pet_ioctl_path(dev_path, V3_VM_CONTINUE, IOCTL_ARG(NULL)); 

    free(dev_path);

    if (ret < 0) {
	ERROR("Could not continue vm (%d)\n", vm_id);
	return -1;
    }
    
    return 0;
}


int 
v3_pause_vm(int vm_id)
{
    char * dev_path = get_vm_dev_path(vm_id);
    int ret = 0;

    ret = pet_ioctl_path(dev_path, V3_VM_PAUSE, IOCTL_ARG(NULL)); 

    free(dev_path);

    if (ret < 0) {
	ERROR("Could not pause vm (%d)\n", vm_id);
	return -1;
    }
    
    return 0;
}


int
v3_simulate_vm(int vm_id, 
	       u32 msecs)
{
    char * dev_path = get_vm_dev_path(vm_id);
    int ret = 0;

    ret = pet_ioctl_path(dev_path, V3_VM_SIMULATE, IOCTL_ARG(msecs)); 

    free(dev_path);

    if (ret < 0) {
	ERROR("Could not simulate vm (%d)\n", vm_id);
	return -1;
    }
    
    return 0;


}


int 
v3_move_vcore(int vm_id,
	      int vcore,
	      int target_pcore)
{
    char * dev_path = get_vm_dev_path(vm_id);
    int ret = 0;

    struct v3_core_move_cmd cmd; 

    memset(&cmd, 0, sizeof(struct v3_core_move_cmd));

    cmd.vcore_id = vcore;
    cmd.pcore_id = target_pcore;

    ret = pet_ioctl_path(dev_path, V3_VM_MOVE_CORE, IOCTL_ARG(&cmd)); 

    free(dev_path);

    if (ret < 0) {
	ERROR("Could not move vm %d's vcore (%d)\n", vm_id, vcore);
	return -1;
    }
    
    return 0;


}

int
v3_debug_vm(int vm_id,
	    u32 core,
	    u32 flags)
{

    char * dev_path = get_vm_dev_path(vm_id);
    int    ret      = 0;

    struct v3_debug_cmd cmd;

    memset(&cmd, 0, sizeof(struct v3_debug_cmd));

    cmd.core = core;
    cmd.cmd  = flags;

    ret = pet_ioctl_path(dev_path, V3_VM_DEBUG, IOCTL_ARG(&cmd)); 

    free(dev_path);

    if (ret < 0) {
	ERROR("Could not send debug command to VM (%d)\n", vm_id);
	return -1;
    }
    
    return 0;


}


int 
v3_load_vm(int    vm_id,
	   char * store,
	   char * url)
{
    char * dev_path = get_vm_dev_path(vm_id);
    int    ret      = 0;

    struct v3_chkpt_info chkpt;

    memset(&chkpt, 0, sizeof(struct v3_chkpt_info));

    if (strlen(store) >= MAX_CHKPT_STORE_LEN) {
	ERROR("Checkpoint store name longer than maximum size (%d)\n",
	       MAX_CHKPT_STORE_LEN);
	return -1;
    }

    if (strlen(url) >= MAX_CHKPT_URL_LEN) {
	ERROR("Checkpoint URL longer than maximum size (%d)\n", 
	       MAX_CHKPT_URL_LEN);
	return -1;
    }    

    strncpy(chkpt.store, store, MAX_CHKPT_STORE_LEN);
    strncpy(chkpt.url,   url,   MAX_CHKPT_URL_LEN);

    ret = pet_ioctl_path(dev_path, V3_VM_LOAD, IOCTL_ARG(&chkpt));

    free(dev_path);

    return ret;
}



int 
v3_save_vm(int    vm_id,
	   char * store,
	   char * url)
{
    char * dev_path = get_vm_dev_path(vm_id);
    int    ret      = 0;

    struct v3_chkpt_info chkpt;

    memset(&chkpt, 0, sizeof(struct v3_chkpt_info));

    if (strlen(store) >= MAX_CHKPT_STORE_LEN) {
	ERROR("Checkpoint store name longer than maximum size (%d)\n",
	       MAX_CHKPT_STORE_LEN);
	return -1;
    }

    if (strlen(url) >= MAX_CHKPT_URL_LEN) {
	ERROR("Checkpoint URL longer than maximum size (%d)\n", 
	       MAX_CHKPT_URL_LEN);
	return -1;
    }    

    strncpy(chkpt.store, store, MAX_CHKPT_STORE_LEN);
    strncpy(chkpt.url,   url,   MAX_CHKPT_URL_LEN);

    ret = pet_ioctl_path(dev_path, V3_VM_SAVE, IOCTL_ARG(&chkpt));

    free(dev_path);

    return ret;
}


#define PROC_PATH   "/proc/v3vee/"


#define VM_HDR_STR "V3 GUESTS (%d)"

struct v3_vm_info * 
v3_get_vms(u32 * num_vms)
{
    struct v3_vm_info * vms = NULL;
    
    char * proc_filename    = NULL;
    FILE * proc_file        = NULL;
    char * line             = NULL;
    size_t line_size        = 0;

    int vm_cnt  = 0;
    int matched = 0;
    int i       = 0;

    if (asprintf(&proc_filename, PROC_PATH "v3-guests") == -1) {
	ERROR("asprintf failed\n");
	goto err1;
    }

    proc_file = fopen(proc_filename, "r");
    
    free(proc_filename);

    if (proc_file == NULL) {
	ERROR("Could not open v3-guests proc file\n");
	goto err1;
    }
    
    if (getline(&line, &line_size, proc_file) == -1) {
	ERROR("Could not read v3-guests proc file\n");
	goto err1;
    }

    matched = sscanf(line, VM_HDR_STR, &vm_cnt);

    if (matched != 1) {
	ERROR("Could not parse v3-guests proc file header\n");
	goto err2;
    }

    vms = calloc(vm_cnt, sizeof(struct v3_vm_info));

    for (i = 0; i < vm_cnt; i++) {
	if (getline(&line, &line_size, proc_file) == -1) {
	    ERROR("Could not read v3-guests proc file\n");
	    goto err3;
	}

	matched = sscanf(line, "%s: [vm_id=%d]",
			 vms[i].name, &(vms[i].vm_id));
	
	if (matched != 2) {
	    ERROR("parsing error in v3-guests proc file (matched=%d)\n", matched);
	    goto err3;
	}
    }

    free(line);

    *num_vms = vm_cnt;
    return vms;

 err3:
    free(vms);
 err2:
    free(line);
 err1:
    return NULL;
}

#define CPU_HDR_STR "VM CORES (%d)"

struct v3_vcpu_info *
v3_get_vm_cpus(int vm_id, u32 * num_cores)
{
    struct v3_vcpu_info * cpus = NULL;

    char * proc_filename       = NULL;
    FILE * proc_file           = NULL;
    char * line                = NULL;
    size_t line_size           = 0;

    int num_cpus = 0;
    int matched  = 0;
    int i        = 0;


    if (asprintf(&proc_filename, PROC_PATH "v3-vm%d/cpus", vm_id) == -1) {
	ERROR("asprintf failed\n");
	goto err1;
    }
    
    proc_file = fopen(proc_filename, "r");
    
    free(proc_filename);

    if (proc_file == NULL) {
	ERROR("Could not open CPU proc file for VM [%d]\n", vm_id);
	goto err1;
    }

    if (getline(&line, &line_size, proc_file) == -1) {
	ERROR("Could not read CPU proc file for VM [%d]\n", vm_id);
	goto err1;
    }


    matched = sscanf(line, CPU_HDR_STR, &num_cpus);

    if (matched != 1) {
	ERROR("Could not parse CPU proc file header for VM [%d]\n", vm_id);
	goto err2;
    }

    cpus = calloc(num_cpus, sizeof(struct v3_vcpu_info));

    for (i = 0; i < num_cpus; i++) {

	if (getline(&line, &line_size, proc_file) == -1) {
	    ERROR("Could not read CPU proc file for VM [%d]\n", vm_id);
	    goto err3;
	}

	matched = sscanf(line, "\tVCPU %d: [PCPU=%d] [PID=%d] [TID=%d]", 
			 &(cpus[i].vcpu_id),
			 &(cpus[i].pcpu_id),
			 &(cpus[i].pid),
			 &(cpus[i].tid));

	if (matched != 4) {
	    ERROR("Parsing error in CPU proc file for VM [%d] (matched=%d)\n", vm_id, matched);
	    goto err3;
	}
    }

    free(line);

    *num_cores = num_cpus;
    return cpus;

 err3:
    free(cpus);
 err2: 
    free(line);
 err1: 
    return NULL;

}


#define MEM_HDR_STR       "BASE MEMORY REGIONS (%d)"
#define MEM_REGEX_STR     "[0-9]+: ([0-9A-Fa-f]{16}) - ([0-9A-Fa-f]{16})"


struct v3_vmem_region *
v3_get_vm_mem(int vm_id, u32 * num_regions)
{
    struct v3_vmem_region * regs = NULL;
    
    char * proc_filename   = NULL;
    FILE * proc_file       = NULL;
    char * line            = NULL;
    size_t line_size       = 0;

    int num_blks = 0;
    int matched  = 0;
    int i        = 0;

    /* grab memory in VM */
    if (asprintf(&proc_filename, PROC_PATH "v3-vm%d/mem", vm_id) == -1) {
	ERROR("asprintf failed\n");
	goto err1;
    }

    proc_file = fopen(proc_filename, "r");

    free(proc_filename);
    
    if (proc_file == NULL) {
	ERROR("Could not open MEM proc file for VM [%d]\n", vm_id);
	goto err1;
    }

    if (getline(&line, &line_size, proc_file) == -1) {
	ERROR("Could not read MEM proc file for VM [%d]\n", vm_id);
	goto err1;
    }
	
    matched = sscanf(line, MEM_HDR_STR, &num_blks);
	
    if (matched != 1) {
	ERROR("Could not parse MEM proc file header for VM [%d]\n", vm_id);
	goto err2;
    }


    regs = calloc(num_blks, sizeof(struct v3_vmem_region));

    for (i = 0; i < num_blks; i++) {
	u32 blk_size   = 0;
	    
	if (getline(&line, &line_size, proc_file) == -1) {
	    ERROR("Could not read MEM proc file for VM [%d]\n", vm_id);
	    goto err3;
	}

	matched = sscanf(line, "       0x%llx - 0x%llx  (size=%uMB) [NUMA ZONE=%d]", 
			 &(regs[i].start_paddr), 
			 &(regs[i].end_paddr), 
			 &blk_size, 
			 &(regs[i].numa_zone));
	
	if (matched != 4) {
	    ERROR("Parsing error in MEM proc file for VM [%d] (matched=%d)\n", vm_id, matched);
	    goto err3;
	}
    }

    free(line);

    *num_regions = num_blks;
    return regs;

 err3:
    free(regs);
 err2:
    free(line);
 err1:
    return NULL;
}


