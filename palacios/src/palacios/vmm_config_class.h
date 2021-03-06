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

#include <palacios/vm_guest_mem.h>


static int 
pre_config_pc_core(struct v3_core_info * core, 
		   v3_cfg_tree_t       * cfg) 
{ 

    core->mem_mode    = PHYSICAL_MEM;

    core->vm_regs.rdi = 0;
    core->vm_regs.rsi = 0;
    core->vm_regs.rbp = 0;
    core->vm_regs.rsp = 0;
    core->vm_regs.rbx = 0;
    core->vm_regs.rdx = 0;
    core->vm_regs.rcx = 0;
    core->vm_regs.rax = 0;

    return 0;
}

static int 
post_config_pc_core(struct v3_core_info * core, 
		    v3_cfg_tree_t       * cfg) 
{ 

    v3_print_mem_map(core->vm_info);
    return 0;
}

static int 
post_config_pc(struct v3_vm_info * vm, 
	       v3_cfg_tree_t     * cfg)
{

#define VGABIOS_START  0x000c0000
#define ROMBIOS_START  0x000e0000

    /* layout vgabios */
    {
	extern uint8_t v3_vgabios_start[];
	extern uint8_t v3_vgabios_end[];
	void * vgabios_dst = 0;

	if (v3_gpa_to_hva(&(vm->cores[0]), VGABIOS_START, (addr_t *)&vgabios_dst) == -1) {
	    PrintError("Could not find VGABIOS destination address\n");
	    return -1;
	}

	memcpy(vgabios_dst, v3_vgabios_start, v3_vgabios_end - v3_vgabios_start);	
    }
    
    /* layout rombios */
    {
	extern uint8_t v3_rombios_start[];
	extern uint8_t v3_rombios_end[];
	void * rombios_dst = 0;
	
	if (v3_gpa_to_hva(&(vm->cores[0]), ROMBIOS_START, (addr_t *)&rombios_dst) == -1) {
	    PrintError("Could not find ROMBIOS destination address\n");
	    return -1;
	}

	memcpy(rombios_dst, v3_rombios_start, v3_rombios_end - v3_rombios_start);

	// SEABIOS gets mapped into end of 4GB region
	if (v3_add_shadow_mem(vm, V3_MEM_CORE_ANY, 
			      V3_MEM_RD | V3_MEM_WR | V3_MEM_EXEC, 
			      0xfffe0000, 0xffffffff, 
			      (addr_t)V3_PAddr(rombios_dst)) == -1) {
	    PrintError("Error mapping SEABIOS to end of memory\n");
	    return -1;
	}
    }
    



    return 0;
}

