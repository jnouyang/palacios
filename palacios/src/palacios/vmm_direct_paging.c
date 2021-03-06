/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Steven Jaconette <stevenjaconette2007@u.northwestern.edu> 
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Steven Jaconette <stevenjaconette2007@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_direct_paging.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vm.h>
#include <palacios/vmm_telemetry.h>

#ifndef V3_CONFIG_DEBUG_NESTED_PAGING
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


static addr_t 
create_generic_pt_page() 
{
    void * page = 0;
    void *temp;

    temp = V3_AllocPages(1);
    if (!temp) { 
	PrintError("Cannot allocate page\n");
	return 0;
    }

    page = V3_VAddr(temp);
    memset(page, 0, PAGE_SIZE);

    return (addr_t)page;
}

// Inline handler functions for each cpu mode
#include "vmm_direct_paging_32.h"
#include "vmm_direct_paging_32pae.h"
#include "vmm_direct_paging_64.h"

int 
v3_init_passthrough_pts(struct v3_core_info * core) 
{
    core->direct_map_pt = (addr_t)V3_PAddr((void *)create_generic_pt_page());
    return 0;
}


int 
v3_free_passthrough_pts(struct v3_core_info * core) 
{
    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(core);

    // Delete the old direct map page tables
    switch(mode) {
	case REAL:
	case PROTECTED:
	    v3_delete_pgtables_32((pde32_t *)CR3_TO_PDE32_VA(core->direct_map_pt));
	    break;
	case PROTECTED_PAE:
	case LONG:
	case LONG_32_COMPAT:
	    // Long mode will only use 32PAE page tables...
	    v3_delete_pgtables_32pae((pdpe32pae_t *)CR3_TO_PDPE32PAE_VA(core->direct_map_pt));
	    break;
	default:
	    PrintError("Unknown CPU Mode\n");
	    return -1;
	    break;
    }

    return 0;
}



int 
v3_free_nested_pts(struct v3_core_info * core) 
{
    v3_cpu_mode_t mode = v3_get_host_cpu_mode();

    // Delete the old direct map page tables
    switch(mode) {
	case PROTECTED:
	    v3_delete_pgtables_32((pde32_t *)CR3_TO_PDE32_VA(core->direct_map_pt));
	    break;
	case PROTECTED_PAE:
	    v3_delete_pgtables_32pae((pdpe32pae_t *)CR3_TO_PDPE32PAE_VA(core->direct_map_pt));
	    break;
	case LONG:
	    v3_delete_pgtables_64((pml4e64_t *)CR3_TO_PML4E64_VA(core->direct_map_pt));
	    break;
	default:
	    PrintError("Unknown CPU Mode\n");
	    return -1;
	    break;
    }

    return 0;
}



int 
v3_reset_passthrough_pts(struct v3_core_info * core) 
{

    v3_free_passthrough_pts(core);

    // create new direct map page table
    v3_init_passthrough_pts(core);
    
    return 0;
}



int 
v3_activate_passthrough_pt(struct v3_core_info * core) 
{
    // For now... But we need to change this....
    // As soon as shadow paging becomes active the passthrough tables are hosed
    // So this will cause chaos if it is called at that time

    core->ctrl_regs.cr3 = *(addr_t*)&(core->direct_map_pt);
    //PrintError("Activate Passthrough Page tables not implemented\n");
    return 0;
}


int 
v3_handle_passthrough_pagefault(struct v3_core_info * core, 
				addr_t                fault_addr, 
				pf_error_t            error_code) 
{
    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(core);

    switch(mode) {
	case REAL:
	case PROTECTED:
	    return handle_passthrough_pagefault_32(core, fault_addr, error_code);

	case PROTECTED_PAE:
	case LONG:
	case LONG_32_COMPAT:
	    // Long mode will only use 32PAE page tables...
	    return handle_passthrough_pagefault_32pae(core, fault_addr, error_code);

	default:
	    PrintError("Unknown CPU Mode\n");
	    break;
    }
    return -1;
}



int 
v3_handle_nested_pagefault(struct v3_core_info * core, 
			   addr_t                fault_addr, 
			   pf_error_t            error_code) 
{
    v3_cpu_mode_t mode = v3_get_host_cpu_mode();


    PrintDebug("Nested PageFault: fault_addr=%p, error_code=%u\n", (void *)fault_addr, *(uint_t *)&error_code);

    switch(mode) {
	case REAL:
	case PROTECTED:
	    return handle_passthrough_pagefault_32(core, fault_addr, error_code);

	case PROTECTED_PAE:
	    return handle_passthrough_pagefault_32pae(core, fault_addr, error_code);

	case LONG:
	case LONG_32_COMPAT:
	    return handle_passthrough_pagefault_64(core, fault_addr, error_code);	    
	
	default:
	    PrintError("Unknown CPU Mode\n");
	    break;
    }
    return -1;
}

int 
v3_invalidate_passthrough_addr(struct v3_core_info * core, 
			       addr_t                inv_addr) 
{
    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(core);

    switch(mode) {
	case REAL:
	case PROTECTED:
	    return invalidate_addr_32(core, inv_addr);

	case PROTECTED_PAE:
	case LONG:
	case LONG_32_COMPAT:
	    // Long mode will only use 32PAE page tables...
	    return invalidate_addr_32pae(core, inv_addr);

	default:
	    PrintError("Unknown CPU Mode\n");
	    break;
    }
    return -1;
}


int 
v3_invalidate_nested_addr(struct v3_core_info * core, 
			  addr_t                inv_addr) 
{

#ifdef __V3_64BIT__
    v3_cpu_mode_t mode = LONG;
#else 
    v3_cpu_mode_t mode = PROTECTED;
#endif

    switch(mode) {
	case REAL:
	case PROTECTED:
	    return invalidate_addr_32(core, inv_addr);

	case PROTECTED_PAE:
	    return invalidate_addr_32pae(core, inv_addr);

	case LONG:
	case LONG_32_COMPAT:
	    return invalidate_addr_64(core, inv_addr);	    
	
	default:
	    PrintError("Unknown CPU Mode\n");
	    break;
    }

    return -1;
}
