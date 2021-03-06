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



#include <palacios/vmm.h>
#include <palacios/vm.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmcb.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_lowlevel.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_xed.h>
#include <palacios/vmm_direct_paging.h>
#include <palacios/vmm_barrier.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_dev_mgr.h>

#ifdef V3_CONFIG_CHECKPOINT
#include <palacios/vmm_checkpoint.h>
#endif


v3_cpu_mode_t 
v3_get_vm_cpu_mode(struct v3_core_info * core) 
{
    struct cr0_32     * cr0  = NULL;
    struct efer_64    * efer = NULL;
    struct cr4_32     * cr4  = (struct cr4_32 *)&(core->ctrl_regs.cr4);
    struct v3_segment * cs   = &(core->segments.cs);


    if (core->shdw_pg_mode      == SHADOW_PAGING) 
    {
	cr0  = (struct cr0_32  *)&(core->shdw_pg_state.guest_cr0);
	efer = (struct efer_64 *)&(core->shdw_pg_state.guest_efer);
    } 
    else if (core->shdw_pg_mode == NESTED_PAGING) 
    {
	cr0  = (struct cr0_32  *)&(core->ctrl_regs.cr0);
	efer = (struct efer_64 *)&(core->ctrl_regs.efer);
    } 
    else {
	PrintError("Invalid Paging Mode...\n");
	V3_ASSERT(0);
	return -1;
    }

    if (cr0->pe == 0) {
	return REAL;
    } else if ((cr4->pae == 0) && (efer->lme == 0)) {
	return PROTECTED;
    } else if (efer->lme == 0) {
	return PROTECTED_PAE;
    } else if ((efer->lme == 1) && (cs->long_mode == 1)) {
	return LONG;
    } else {
	// What about LONG_16_COMPAT???
	return LONG_32_COMPAT;
    }
}

// Get address width in bytes
uint_t 
v3_get_addr_width(struct v3_core_info * core) 
{
    struct cr0_32     * cr0  = NULL;
    struct efer_64    * efer = NULL;
    struct cr4_32     * cr4  = (struct cr4_32 *)&(core->ctrl_regs.cr4);
    struct v3_segment * cs   = &(core->segments.cs);


    if (core->shdw_pg_mode == SHADOW_PAGING) 
    {
	cr0  = (struct cr0_32  *)&(core->shdw_pg_state.guest_cr0);
	efer = (struct efer_64 *)&(core->shdw_pg_state.guest_efer);
    } 
    else if (core->shdw_pg_mode == NESTED_PAGING)
    {
	cr0  = (struct cr0_32  *)&(core->ctrl_regs.cr0);
	efer = (struct efer_64 *)&(core->ctrl_regs.efer);
    } 
    else {
	PrintError("Invalid Paging Mode...\n");
	V3_ASSERT(0);
	return -1;
    }

    if (cr0->pe == 0) {
	return 2;
    } else if ((cr4->pae == 0)  && (efer->lme == 0)) {
	return 4;
    } else if (efer->lme == 0) {
	return 4;
    } else if ((efer->lme == 1) && (cs->long_mode == 1)) {
	return 8;
    } else {
	// What about LONG_16_COMPAT???
	return 4;
    }
}


static const char REAL_STR[]           = "Real";
static const char PROTECTED_STR[]      = "Protected";
static const char PROTECTED_PAE_STR[]  = "Protected+PAE";
static const char LONG_STR[]           = "Long";
static const char LONG_32_COMPAT_STR[] = "32bit Compat";
static const char LONG_16_COMPAT_STR[] = "16bit Compat";

const char * 
v3_cpu_mode_to_str(v3_cpu_mode_t mode) 
{
    switch (mode) {
	case REAL:
	    return REAL_STR;
	case PROTECTED:
	    return PROTECTED_STR;
	case PROTECTED_PAE:
	    return PROTECTED_PAE_STR;
	case LONG:
	    return LONG_STR;
	case LONG_32_COMPAT:
	    return LONG_32_COMPAT_STR;
	case LONG_16_COMPAT:
	    return LONG_16_COMPAT_STR;
	default:
	    return NULL;
    }
}

v3_mem_mode_t 
v3_get_vm_mem_mode(struct v3_core_info * core) 
{
    struct cr0_32 * cr0;

    if (core->shdw_pg_mode == SHADOW_PAGING) 
    {
	cr0 = (struct cr0_32 *)&(core->shdw_pg_state.guest_cr0);
    } 
    else if (core->shdw_pg_mode == NESTED_PAGING) 
    {
	cr0 = (struct cr0_32 *)&(core->ctrl_regs.cr0);
    } 
    else {
	PrintError("Invalid Paging Mode...\n");
	V3_ASSERT(0);
	return -1;
    }


    if (cr0->pg == 0) {
	return PHYSICAL_MEM;
    } else {
	return VIRTUAL_MEM;
    }
}

static const char PHYS_MEM_STR[] = "Physical Memory";
static const char VIRT_MEM_STR[] = "Virtual Memory";

const char * 
v3_mem_mode_to_str(v3_mem_mode_t mode) 
{
    switch (mode) {
	case PHYSICAL_MEM:
	    return PHYS_MEM_STR;
	case VIRTUAL_MEM:
	    return VIRT_MEM_STR;
	default:
	    return NULL;
    }
}


#ifdef V3_CONFIG_APIC
#include <devices/apic.h>
#endif

/* The BSP flag is housed in the APIC Base address MSR... 
   With no APIC we default to BSP, otherwise we have to check
 */
int 
v3_is_core_bsp(struct v3_core_info* core) 
{
    struct vm_device * apic_dev =  v3_find_dev(core->vm_info, "apic");

    if (apic_dev == NULL) {
	return 1;
    } 

#ifdef V3_CONFIG_APIC
    return v3_apic_is_bsp(core, apic_dev);
#else 
    return 0;
#endif
}





#include <palacios/vmcs.h>
#include <palacios/vmcb.h>
static int 
info_hcall(struct v3_core_info * core, 
	   uint_t                hcall_id, 
	   void                * priv_data) 
{
    extern v3_cpu_arch_t v3_mach_type;
    int cpu_valid = 0;

    V3_Print("************** Guest State ************\n");
    v3_print_guest_state(core);
    
    // init SVM/VMX
#ifdef V3_CONFIG_SVM
    if ((v3_mach_type == V3_SVM_CPU) || (v3_mach_type == V3_SVM_REV3_CPU)) {
	cpu_valid = 1;
	v3_print_vmcb((vmcb_t *)(core->vmm_data));
    }
#endif
#ifdef V3_CONFIG_VMX
    if ((v3_mach_type == V3_VMX_CPU) || (v3_mach_type == V3_VMX_EPT_CPU) || (v3_mach_type == V3_VMX_EPT_UG_CPU)) {
	cpu_valid = 1;
	v3_print_vmcs();
    }
#endif
    if (!cpu_valid) {
	PrintError("Invalid CPU Type 0x%x\n", v3_mach_type);
	return -1;
    }
    

    return 0;
}



static int
yield_to_pid_hcall(struct v3_core_info * core, 
		   uint32_t              hcall_id,
		   void                * priv_data)
{
    uint32_t pid = core->vm_regs.rbx;
    uint32_t tid = core->vm_regs.rcx;
	
    v3_yield_to_pid(core, pid, tid);

    return 0;
}


static int
yield_to_core_hcall(struct v3_core_info * core, 
		    uint32_t              hcall_id,
		    void                * priv_data)
{
    uint32_t vcpu_id = core->vm_regs.rbx;
    
    if (vcpu_id > core->vm_info->num_cores) {
	PrintError("Tried to yield to invalid virtual core (%u)\n", vcpu_id);
	return -1;
    }
    
    v3_yield_to_thread(core, core->vm_info->cores[vcpu_id].core_thread);
    
    return 0;
}


#ifdef V3_CONFIG_SVM
#include <palacios/svm.h>
#include <palacios/svm_io.h>
#include <palacios/svm_msr.h>
#endif

#ifdef V3_CONFIG_VMX
#include <palacios/vmx.h>
#include <palacios/vmx_io.h>
#include <palacios/vmx_msr.h>
#endif


int 
v3_init_vm(struct v3_vm_info * vm) 
{
    extern v3_cpu_arch_t v3_mach_type;

#ifdef V3_CONFIG_CHECKPOINT
    v3_init_chkpt(vm);
#endif

    v3_init_hypercall_map(vm);

#ifdef V3_CONFIG_TELEMETRY
    v3_init_telemetry(vm);
#endif

    v3_init_io_map(vm);
    v3_init_msr_map(vm);
    v3_init_cpuid_map(vm);
    v3_init_host_events(vm);
    v3_init_intr_routers(vm);
    v3_init_ext_manager(vm);

    v3_init_barrier(vm);

    // Initialize the memory map
    if (v3_init_mem_map(vm) == -1) {
	PrintError("Could not initialize shadow map\n");
	return -1;
    }

    v3_init_mem_hooks(vm);

    if (v3_init_shdw_impl(vm) == -1) {
	PrintError("VM initialization error in shadow implementaion\n");
	return -1;
    }


    v3_init_time_vm(vm);

    v3_init_vm_debugging(vm);



    v3_init_dev_mgr(vm);


    // init SVM/VMX
    switch (v3_mach_type) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    v3_init_svm_io_map(vm);
	    v3_init_svm_msr_map(vm);
	    break;
#endif
#ifdef V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    v3_init_vmx_io_map(vm);
	    v3_init_vmx_msr_map(vm);
	    break;
#endif
	default:
	    PrintError("Invalid CPU Type 0x%x\n", v3_mach_type);
	    return -1;
    }
    
    v3_register_hypercall(vm, VM_INFO_HCALL,       info_hcall,          NULL);
    v3_register_hypercall(vm, YIELD_TO_PID_HCALL,  yield_to_pid_hcall,  NULL);
    v3_register_hypercall(vm, YIELD_TO_CORE_HCALL, yield_to_core_hcall, NULL);

    return 0;
}


int 
v3_free_vm_internal(struct v3_vm_info * vm) 
{
    extern v3_cpu_arch_t v3_mach_type;

    v3_remove_hypercall(vm, VM_INFO_HCALL);
    v3_remove_hypercall(vm, YIELD_TO_PID_HCALL);
    v3_remove_hypercall(vm, YIELD_TO_CORE_HCALL);


    v3_deinit_dev_mgr(vm);

    v3_deinit_time_vm(vm);

    v3_deinit_mem_hooks(vm);
    v3_delete_mem_map(vm);
    v3_deinit_shdw_impl(vm);

    v3_deinit_ext_manager(vm);
    v3_deinit_intr_routers(vm);
    v3_deinit_host_events(vm);

    v3_deinit_barrier(vm);

    v3_deinit_cpuid_map(vm);
    v3_deinit_msr_map(vm);
    v3_deinit_io_map(vm);
    v3_deinit_hypercall_map(vm);

#ifdef V3_CONFIG_TELEMETRY
    v3_deinit_telemetry(vm);
#endif

#ifdef V3_CONFIG_CHECKPOINT
    v3_deinit_chkpt(vm);
#endif

    // init SVM/VMX
    switch (v3_mach_type) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    v3_deinit_svm_io_map(vm);
	    v3_deinit_svm_msr_map(vm);
	    break;
#endif
#ifdef V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    v3_deinit_vmx_io_map(vm);
	    v3_deinit_vmx_msr_map(vm);
	    break;
#endif
	default:
	    PrintError("Invalid CPU Type 0x%x\n", v3_mach_type);
	    return -1;
    }


    return 0;
}


int 
v3_init_core(struct v3_core_info * core) 
{
    extern v3_cpu_arch_t v3_mach_type;
    struct v3_vm_info  * vm = core->vm_info;


    core->core_run_state = CORE_STOPPED;

    /*
     * Initialize the subsystem data strutures
     */
#ifdef V3_CONFIG_TELEMETRY
    v3_init_core_telemetry(core);
#endif

    if (core->shdw_pg_mode == SHADOW_PAGING) {
	v3_init_shdw_pg_state(core);
    }

    v3_init_time_core(core);
    v3_init_intr_controllers(core);
    v3_init_exception_state(core);

    v3_init_decoder(core);



    // init SVM/VMX


    switch (v3_mach_type) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    if (v3_init_svm_vmcb(core, vm->vm_class) == -1) {
		PrintError("Error in SVM initialization\n");
		return -1;
	    }
	    break;
#endif
#ifdef V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    if (v3_init_vmx_core(core, vm->vm_class) == -1) {
		PrintError("Error in VMX initialization\n");
		return -1;
	    }
	    break;
#endif
	default:
	    PrintError("Invalid CPU Type 0x%x\n", v3_mach_type);
	    return -1;
    }

    return 0;
}



int 
v3_free_core(struct v3_core_info * core) 
{
    extern v3_cpu_arch_t v3_mach_type;

    

    v3_deinit_decoder(core);

    v3_deinit_intr_controllers(core);
    v3_deinit_time_core(core);

    if (core->shdw_pg_mode      == SHADOW_PAGING) {
	v3_deinit_shdw_pg_state(core);
	v3_free_passthrough_pts(core);
    } 
    else if (core->shdw_pg_mode == NESTED_PAGING)
    {
	v3_free_nested_pts(core);
    }



#ifdef V3_CONFIG_TELEMETRY
    v3_deinit_core_telemetry(core);
#endif

    switch (v3_mach_type) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    if (v3_deinit_svm_vmcb(core) == -1) {
		PrintError("Error in SVM initialization\n");
		return -1;
	    }
	    break;
#endif
#ifdef V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    if (v3_free_vmx_core(core) == -1) {
		PrintError("Error in VMX initialization\n");
		return -1;
	    }
	    break;
#endif
	default:
	    PrintError("Invalid CPU Type 0x%x\n", v3_mach_type);
	    return -1;
    }

    return 0;
}



