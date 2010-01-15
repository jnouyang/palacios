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

#include <palacios/vmm_config.h>
#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_msr.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_telemetry.h>
#include <palacios/vmm_mem.h>
#include <palacios/vmm_hypercall.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_cpuid.h>
#include <palacios/vmm_xml.h>

#include <palacios/svm.h>
#include <palacios/vmx.h>

#ifdef CONFIG_SYMBIOTIC
#include <palacios/vmm_sym_iface.h>

#ifdef CONFIG_SYMBIOTIC_SWAP
#include <palacios/vmm_sym_swap.h>
#endif

#endif


#include <palacios/vmm_host_events.h>
#include <palacios/vmm_socket.h>

#include "vmm_config_class.h"

// This is used to access the configuration file index table
struct file_hdr {
    uint32_t index;
    uint32_t size;
    uint64_t offset;
};

struct file_idx_table {
    uint64_t num_files;
    struct file_hdr hdrs[0];
};




static int setup_memory_map(struct v3_vm_info * vm, v3_cfg_tree_t * cfg);
static int setup_devices(struct v3_vm_info * vm, v3_cfg_tree_t * cfg);



char * v3_cfg_val(v3_cfg_tree_t * tree, char * tag) {
    char * attrib = (char *)v3_xml_attr(tree, tag);
    v3_cfg_tree_t * child_entry = v3_xml_child(tree, tag);
    char * val = NULL;

    if ((child_entry != NULL) && (attrib != NULL)) {
	PrintError("Duplicate Configuration parameters present for %s\n", tag);
	return NULL;
    }

    val = (attrib == NULL) ? v3_xml_txt(child_entry): attrib;

    return val; 
}

v3_cfg_tree_t * v3_cfg_subtree(v3_cfg_tree_t * tree, char * tag) {
    return v3_xml_child(tree, tag);
}

v3_cfg_tree_t * v3_cfg_next_branch(v3_cfg_tree_t * tree) {
    return v3_xml_next(tree);
}



struct v3_cfg_file * v3_cfg_get_file(struct v3_vm_info * vm, char * tag) {
    struct v3_cfg_file * file = NULL;

    file = (struct v3_cfg_file *)v3_htable_search(vm->cfg_data->file_table, (addr_t)tag);

    return file;
}


static uint_t file_hash_fn(addr_t key) {
    char * name = (char *)key;
    return v3_hash_buffer((uchar_t *)name, strlen(name));
}

static int file_eq_fn(addr_t key1, addr_t key2) {
    char * name1 = (char *)key1;
    char * name2 = (char *)key2;

    return (strcmp(name1, name2) == 0);
}

static struct v3_config * parse_config(void * cfg_blob) {
    struct v3_config * cfg = NULL;
    int offset = 0;
    uint_t xml_len = 0; 
    struct file_idx_table * files = NULL;
    v3_cfg_tree_t * file_tree = NULL;

    V3_Print("cfg data at %p\n", cfg_blob);

    if (memcmp(cfg_blob, "v3vee\0\0\0", 8) != 0) {
	PrintError("Invalid Configuration Header\n");
	return NULL;
    }

    offset += 8;

    cfg = (struct v3_config *)V3_Malloc(sizeof(struct v3_config));
    memset(cfg, 0, sizeof(struct v3_config));

    cfg->blob = cfg_blob;
    INIT_LIST_HEAD(&(cfg->file_list));
    cfg->file_table = v3_create_htable(0, file_hash_fn, file_eq_fn);
    
    xml_len = *(uint32_t *)(cfg_blob + offset);
    offset += 4;

    cfg->cfg = (v3_cfg_tree_t *)v3_xml_parse((uint8_t *)(cfg_blob + offset));
    offset += xml_len;
   
    offset += 8;

    files = (struct file_idx_table *)(cfg_blob + offset);

    V3_Print("Number of files in cfg: %d\n", (uint32_t)(files->num_files));

    file_tree = v3_cfg_subtree(v3_cfg_subtree(cfg->cfg, "files"), "file");

    while (file_tree) {
	char * id = v3_cfg_val(file_tree, "id");
	char * index = v3_cfg_val(file_tree, "index");
	int idx = atoi(index);
	struct file_hdr * hdr = &(files->hdrs[idx]);
	struct v3_cfg_file * file = NULL;

	file = (struct v3_cfg_file *)V3_Malloc(sizeof(struct v3_cfg_file));
	
	if (!file) {
	    PrintError("Could not allocate file structure\n");
	    return NULL;
	}


	V3_Print("File index=%d id=%s\n", idx, id);

	strncpy(file->tag, id, 256);
	file->size = hdr->size;
	file->data = cfg_blob + hdr->offset;

	V3_Print("Storing file data offset = %d, size=%d\n", (uint32_t)hdr->offset, hdr->size);
	V3_Print("file data at %p\n", file->data);
	list_add( &(file->file_node), &(cfg->file_list));

	V3_Print("Keying file to name\n");
	v3_htable_insert(cfg->file_table, (addr_t)(file->tag), (addr_t)(file));

	V3_Print("Iterating to next file\n");

	file_tree = v3_cfg_next_branch(file_tree);
    }

    V3_Print("Configuration parsed successfully\n");

    return cfg;
}

static int pre_config_vm(struct v3_vm_info * vm, v3_cfg_tree_t * vm_cfg) {
    char * memory_str = v3_cfg_val(vm_cfg, "memory");
    char * schedule_hz_str = v3_cfg_val(vm_cfg, "schedule_hz");
    char * vm_class = v3_cfg_val(vm_cfg, "class");
    uint32_t sched_hz = 100; 	// set the schedule frequency to 100 HZ
    
    if (!memory_str) {
	PrintError("Memory is a required configuration parameter\n");
	return -1;
    }
    
    PrintDebug("Memory=%s\n", memory_str);

    // Amount of ram the Guest will have, always in MB
    vm->mem_size = atoi(memory_str) * 1024 * 1024;
    
    if (strcasecmp(vm_class, "PC") == 0) {
	vm->vm_class = V3_PC_VM;
    } else {
	PrintError("Invalid VM class\n");
	return -1;
    }

#ifdef CONFIG_TELEMETRY
    {
	char * telemetry = v3_cfg_val(vm_cfg, "telemetry");

	// This should go first, because other subsystems will depend on the guest_info flag    
	if ((telemetry) && (strcasecmp(telemetry, "enable") == 0)) {
	    vm->enable_telemetry = 1;
	} else {
	    vm->enable_telemetry = 0;
	}
    }
#endif

    v3_init_hypercall_map(vm);
    v3_init_io_map(vm);
    v3_init_msr_map(vm);
    v3_init_cpuid_map(vm);
    v3_init_host_events(vm);
    v3_init_intr_routers(vm);

    // Initialize the memory map
    if (v3_init_mem_map(vm) == -1) {
	PrintError("Could not initialize shadow map\n");
	return -1;
    }

#ifdef CONFIG_SYMBIOTIC
    v3_init_sym_iface(vm);
#endif

    v3_init_dev_mgr(vm);


#ifdef CONFIG_SYMBIOTIC_SWAP
    PrintDebug("initializing symbiotic swap\n");
    v3_init_sym_swap(vm);
#endif

   if (schedule_hz_str) {
	sched_hz = atoi(schedule_hz_str);
    }

    PrintDebug("CPU_KHZ = %d, schedule_freq=%p\n", V3_CPU_KHZ(), 
	       (void *)(addr_t)sched_hz);

    vm->yield_cycle_period = (V3_CPU_KHZ() * 1000) / sched_hz;
    
    return 0;
}

static int pre_config_core(struct guest_info * info, v3_cfg_tree_t * core_cfg) {
    extern v3_cpu_arch_t v3_cpu_types[];
    char * paging = v3_cfg_val(core_cfg, "paging");

    /*
     * Initialize the subsystem data strutures
     */
#ifdef CONFIG_TELEMETRY
    if (info->vm_info->enable_telemetry) {
	v3_init_telemetry(info);
    }
#endif

    
    if ((v3_cpu_types[info->cpu_id] == V3_SVM_REV3_CPU) && 
	(paging) && (strcasecmp(paging, "nested") == 0)) {
	PrintDebug("Guest Page Mode: NESTED_PAGING\n");
	info->shdw_pg_mode = NESTED_PAGING;
    } else {
	PrintDebug("Guest Page Mode: SHADOW_PAGING\n");
	v3_init_shadow_page_state(info);
	info->shdw_pg_mode = SHADOW_PAGING;
    }



    v3_init_time(info);
    v3_init_intr_controllers(info);
    v3_init_exception_state(info);

    v3_init_decoder(info);
    


    if (info->vm_info->vm_class == V3_PC_VM) {
	if (pre_config_pc_core(info, core_cfg) == -1) {
	    PrintError("PC Post configuration failure\n");
	    return -1;
	}
    } else {
	PrintError("Invalid VM Class\n");
	return -1;
    }

    return 0;
}



static int post_config_vm(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    
    vm->run_state = VM_STOPPED;

    // Configure the memory map for the guest
    if (setup_memory_map(vm, cfg) == -1) {
	PrintError("Setting up guest memory map failed...\n");
	return -1;
    }
    
    //v3_hook_io_port(info, 1234, &IO_Read, NULL, info);
  
    if (setup_devices(vm, cfg) == -1) {
	PrintError("Failed to setup devices\n");
	return -1;
    }


    //    v3_print_io_map(info);
    v3_print_msr_map(vm);


    if (vm->vm_class == V3_PC_VM) {
	if (post_config_pc(vm, cfg) == -1) {
	    PrintError("PC Post configuration failure\n");
	    return -1;
	}
    } else {
	PrintError("Invalid VM Class\n");
	return -1;
    }

    return 0;
}



static int post_config_core(struct guest_info * info, v3_cfg_tree_t * cfg) {


 
    if (info->vm_info->vm_class == V3_PC_VM) {
	if (post_config_pc_core(info, cfg) == -1) {
	    PrintError("PC Post configuration failure\n");
	    return -1;
	}
    } else {
	PrintError("Invalid VM Class\n");
	return -1;
    }


    return 0;
}



static struct v3_vm_info * allocate_guest(int num_cores) {
    int guest_state_size = sizeof(struct v3_vm_info) + (sizeof(struct guest_info) * num_cores);
    struct v3_vm_info * vm = V3_Malloc(guest_state_size);

    memset(vm, 0, guest_state_size);

    vm->num_cores = num_cores;

    return vm;
}



struct v3_vm_info * v3_config_guest(void * cfg_blob) {
    v3_cpu_arch_t cpu_type = v3_get_cpu_type(v3_get_cpu_id());
    struct v3_config * cfg_data = NULL;
    struct v3_vm_info * vm = NULL;
    int num_cores = 0;
    int i = 0;
    v3_cfg_tree_t * cores_cfg = NULL;
    v3_cfg_tree_t * per_core_cfg = NULL;

    if (cpu_type == V3_INVALID_CPU) {
	PrintError("Configuring guest on invalid CPU\n");
	return NULL;
    }

    cfg_data = parse_config(cfg_blob);

    if (!cfg_data) {
	PrintError("Could not parse configuration\n");
	return NULL;
    }

    cores_cfg = v3_cfg_subtree(cfg_data->cfg, "cores");

    if (!cores_cfg) {
	PrintError("Could not find core configuration (new config format required)\n");
	return NULL;
    }

    num_cores = atoi(v3_cfg_val(cores_cfg, "count"));

    if (num_cores == 0) {
	PrintError("No cores specified in configuration\n");
	return NULL;
    }

    V3_Print("Configuring %d cores\n", num_cores);

    vm = allocate_guest(num_cores);    

    if (!vm) {
	PrintError("Could not allocate %d core guest\n", vm->num_cores);
	return NULL;
    }

    vm->cfg_data = cfg_data;

    V3_Print("Preconfiguration\n");

    if (pre_config_vm(vm, vm->cfg_data->cfg) == -1) {
	PrintError("Error in preconfiguration\n");
	return NULL;
    }


    V3_Print("Per core configuration\n");
    per_core_cfg = v3_cfg_subtree(cores_cfg, "core");

    // per core configuration
    for (i = 0; i < vm->num_cores; i++) {
	struct guest_info * info = &(vm->cores[i]);

	
	info->cpu_id = i;
	info->vm_info = vm;

	pre_config_core(info, per_core_cfg);

	// init SVM/VMX
#ifdef CONFIG_SVM
	if ((cpu_type == V3_SVM_CPU) || (cpu_type == V3_SVM_REV3_CPU)) {
	    if (v3_init_svm_vmcb(info, vm->vm_class) == -1) {
		PrintError("Error in SVM initialization\n");
		return NULL;
	    }
	}
#endif
#ifdef CONFIG_VMX
	else if ((cpu_type == V3_VMX_CPU) || (cpu_type == V3_VMX_EPT_CPU)) {
	    if (v3_init_vmx_vmcs(info, vm->vm_class) == -1) {
		PrintError("Error in VMX initialization\n");
		return NULL;
	    }
	}
#endif
	else {
	    PrintError("Invalid CPU Type\n");
	    return NULL;
	}
	
	per_core_cfg = v3_cfg_next_branch(per_core_cfg);
    }


    V3_Print("Post Configuration\n");

    if (post_config_vm(vm, vm->cfg_data->cfg) == -1) {
	PrintError("Error in postconfiguration\n");
	return NULL;
    }


    per_core_cfg = v3_cfg_subtree(cores_cfg, "core");

    // per core configuration
    for (i = 0; i < vm->num_cores; i++) {
	struct guest_info * info = &(vm->cores[i]);

	post_config_core(info, per_core_cfg);

	per_core_cfg = v3_cfg_next_branch(per_core_cfg);
    }

    V3_Print("Configuration successfull\n");

    return vm;
}





static int setup_memory_map(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    v3_cfg_tree_t * mem_region = v3_cfg_subtree(v3_cfg_subtree(cfg, "memmap"), "region");

    while (mem_region) {
	addr_t start_addr = atox(v3_cfg_val(mem_region, "start"));
	addr_t end_addr = atox(v3_cfg_val(mem_region, "end"));
	addr_t host_addr = atox(v3_cfg_val(mem_region, "host_addr"));

    
	if (v3_add_shadow_mem(vm, start_addr, end_addr, host_addr) == -1) {
	    PrintError("Could not map memory region: %p-%p => %p\n", 
		       (void *)start_addr, (void *)end_addr, (void *)host_addr);
	    return -1;
	}

	mem_region = v3_cfg_next_branch(mem_region);
    }

    return 0;
}




static int setup_devices(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    v3_cfg_tree_t * device = v3_cfg_subtree(v3_cfg_subtree(cfg, "devices"), "device");

    
    while (device) {
	char * id = v3_cfg_val(device, "id");

	V3_Print("configuring device %s\n", id);

	if (v3_create_device(vm, id, device) == -1) {
	    PrintError("Error creating device %s\n", id);
	    return -1;
	}
	
	device = v3_cfg_next_branch(device);
    }

    v3_print_dev_mgr(vm);

    return 0;
}




