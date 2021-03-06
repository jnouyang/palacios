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

#ifndef __VMM_H__
#define __VMM_H__


/*#include <palacios/vm.h>*/

struct v3_core_info;


#ifdef __V3VEE__
#include <palacios/vm.h>




/* utility definitions */
#define V3_Get_CPU() ({  				                \
            int ret = 0;                                                \
            extern struct v3_os_hooks * os_hooks;                       \
            if ((os_hooks) && (os_hooks)->get_cpu) {                    \
                ret = (os_hooks)->get_cpu();                            \
            }                                                           \
            ret;                                                        \
        })





#define V3_Print(fmt, args...)						\
    do {								\
	extern struct v3_core_info * v3_cores_current[];		\
	extern struct v3_os_hooks * os_hooks;				\
									\
	if ((os_hooks) && (os_hooks)->print) {				\
	    if (v3_cores_current[V3_Get_CPU()] != NULL) {		\
		(os_hooks)->print("V3> [%s-%d.%d] " fmt, v3_cores_current[V3_Get_CPU()]->vm_info->name, v3_cores_current[V3_Get_CPU()]->vcpu_id, v3_cores_current[V3_Get_CPU()]->pcpu_id, ##args); \
	    } else {							\
		(os_hooks)->print("V3> (%d) " fmt, V3_Get_CPU(), ##args); \
	    }								\
	}								\
    } while (0)

#define PrintDebug(fmt, args...)					\
    do {								\
	extern struct v3_core_info * v3_cores_current[];		\
	extern struct v3_os_hooks * os_hooks;				\
									\
	if ((os_hooks) && (os_hooks)->print) {				\
	    if (v3_cores_current[V3_Get_CPU()] != NULL) {		\
		(os_hooks)->print("V3> [%s-%d.%d] "fmt, v3_cores_current[V3_Get_CPU()]->vm_info->name, v3_cores_current[V3_Get_CPU()]->vcpu_id, v3_cores_current[V3_Get_CPU()]->pcpu_id, ##args); \
	    } else {							\
		(os_hooks)->print("V3> (%d) " fmt, V3_Get_CPU(), ##args); \
	    }								\
	}								\
    } while (0)



#define PrintError(fmt, args...)					\
    do {								\
	extern struct v3_core_info * v3_cores_current[];		\
	extern struct v3_os_hooks * os_hooks;				\
									\
	if ((os_hooks) && (os_hooks)->print) {				\
	    if (v3_cores_current[V3_Get_CPU()] != NULL) {		\
		(os_hooks)->print("V3> [%s-%d.%d] %s(%d): "fmt, v3_cores_current[V3_Get_CPU()]->vm_info->name, v3_cores_current[V3_Get_CPU()]->vcpu_id, v3_cores_current[V3_Get_CPU()]->pcpu_id, __FILE__, __LINE__, ##args); \
	    } else {							\
		(os_hooks)->print("V3> (%d) -- %s(%d): "fmt, V3_Get_CPU(), __FILE__, __LINE__, ##args); \
	    }								\
	}								\
    } while (0)						



/* 4KB-aligned */
#define V3_AllocPages(num_pages)			        	\
    ({							        	\
	extern struct v3_os_hooks * os_hooks;		        	\
	void * ptr = 0;					        	\
	if ((os_hooks) && (os_hooks)->allocate_pages) {	        	\
	    ptr = (os_hooks)->allocate_pages(num_pages, PAGE_SIZE_4KB, -1);	\
	}						        	\
	ptr;						        	\
    })


#define V3_AllocAlignedPages(num_pages, align)		        	\
    ({							        	\
	extern struct v3_os_hooks * os_hooks;		        	\
	void * ptr = 0;					        	\
	if ((os_hooks) && (os_hooks)->allocate_pages) {	        	\
	    ptr = (os_hooks)->allocate_pages(num_pages, align, -1);  	\
	}						        	\
	ptr;						        	\
    })


#define V3_AllocPagesNode(num_pages, node_id)				\
    ({									\
	extern struct v3_os_hooks * os_hooks;				\
	void * ptr = 0;							\
	if ((os_hooks) && (os_hooks)->allocate_pages) {			\
	    ptr = (os_hooks)->allocate_pages(num_pages, PAGE_SIZE_4KB, node_id); \
	}								\
	ptr;								\
    })


#define V3_FreePages(page, num_pages)			\
    do {						\
	extern struct v3_os_hooks * os_hooks;		\
	if ((os_hooks) && (os_hooks)->free_pages) {	\
	    (os_hooks)->free_pages(page, num_pages);	\
	}						\
    } while(0)


#define V3_VAddr(addr) ({					\
	    extern struct v3_os_hooks * os_hooks;		\
	    void * var = 0;					\
	    if ((os_hooks) && (os_hooks)->paddr_to_vaddr) {	\
		var = (os_hooks)->paddr_to_vaddr(addr);		\
	    }							\
	    var;						\
	})


#define V3_PAddr(addr) ({					\
	    extern struct v3_os_hooks * os_hooks;		\
	    void * var = 0;					\
	    if ((os_hooks) && (os_hooks)->vaddr_to_paddr) {	\
		var = (os_hooks)->vaddr_to_paddr(addr);		\
	    }							\
	    var;						\
	})



#define V3_Malloc(size) ({				\
	    extern struct v3_os_hooks * os_hooks;	\
	    void * var = 0;				\
	    if ((os_hooks) && (os_hooks)->malloc) {	\
		var = (os_hooks)->malloc(size);		\
	    }						\
	    if (!var) PrintError("MALLOC FAILURE. Memory LEAK!!\n");	\
	    var;					\
	})

// We need to check the hook structure at runtime to ensure its SAFE
#define V3_Free(addr)				\
    do {					\
	extern struct v3_os_hooks * os_hooks;	\
	if ((os_hooks) && (os_hooks)->free) {	\
	    (os_hooks)->free(addr);		\
	}					\
    } while (0)

// uint_t V3_CPU_KHZ();
#define V3_CPU_KHZ() ({							\
	    unsigned int khz = 0;					\
	    extern struct v3_os_hooks * os_hooks;			\
	    if ((os_hooks) && (os_hooks)->get_cpu_khz) {		\
		khz = (os_hooks)->get_cpu_khz();			\
	    }								\
	    khz;							\
	})								\
	






#define V3_CREATE_THREAD(fn, arg, name)	({				\
	    void * thread = NULL;					\
	    extern struct v3_os_hooks * os_hooks;			\
	    if ((os_hooks) && (os_hooks)->create_kernel_thread) {	\
		thread = (os_hooks)->create_kernel_thread(fn, arg, name); \
	    }								\
	    thread;							\
	})

#define V3_CREATE_THREAD_ON_CPU(cpu, fn, arg, name) ({			\
	    void * thread = NULL;					\
	    extern struct v3_os_hooks * os_hooks;			\
	    if ((os_hooks) && (os_hooks)->create_thread_on_cpu) {	\
		thread = (os_hooks)->create_thread_on_cpu(cpu, fn, arg, name); \
	    }								\
	    thread;							\
	})

#define V3_START_THREAD(thread)	({				\
	    extern struct v3_os_hooks * os_hooks;			\
	    if ((os_hooks) && (os_hooks)->start_thread) {	\
		(os_hooks)->start_thread(thread); \
	    }								\
	})




#define V3_Call_On_CPU(cpu, fn, arg)    		\
    do {						\
        extern struct v3_os_hooks * os_hooks;           \
        if ((os_hooks) && (os_hooks)->call_on_cpu) {    \
            (os_hooks)->call_on_cpu(cpu, fn, arg);      \
        }                                               \
    } while (0)




#define V3_MOVE_THREAD_TO_CPU(pcpu, thread) ({				\
	int ret = -1;							\
	extern struct v3_os_hooks * os_hooks;				\
	if((os_hooks) && (os_hooks)->move_thread_to_cpu) {		\
	    ret = (os_hooks)->move_thread_to_cpu(pcpu, thread);		\
	}								\
	ret;								\
    })
    

/* ** */


#define V3_ASSERT(x)							\
    do {								\
	extern struct v3_os_hooks * os_hooks; 				\
	if (!(x)) {							\
	    PrintDebug("Failed assertion in %s: %s at %s, line %d, RA=%lx\n", \
		       __func__, #x, __FILE__, __LINE__,		\
		       (ulong_t) __builtin_return_address(0));		\
	    while(1){							\
		if ((os_hooks) && (os_hooks)->yield_cpu) {		\
	    		(os_hooks)->yield_cpu();			\
		}							\
	    }								\
	}								\
    } while(0)								\
	


#define V3_Yield()					\
    do {						\
	extern struct v3_os_hooks * os_hooks;		\
	if ((os_hooks) && (os_hooks)->yield_cpu) {	\
	    (os_hooks)->yield_cpu();			\
	}						\
    } while (0)						\


#define V3_Sleep(usec)				\
    do {						\
	extern struct v3_os_hooks * os_hooks;		\
	if ((os_hooks) && (os_hooks)->sleep_cpu) {\
	    (os_hooks)->sleep_cpu(usec);		\
	} else {					\
	    V3_Yield();                                 \
        }                                               \
    }  while (0)                                        \

#define V3_Wakeup(cpu)					\
    do {						\
	extern struct v3_os_hooks * os_hooks;		\
	if ((os_hooks) && (os_hooks)->wakeup_cpu) {	\
	    (os_hooks)->wakeup_cpu(cpu);			\
	}						\
    } while (0)						\


#define V3_SaveFPU()					\
	do {						\
	    extern struct v3_os_hooks * os_hooks;	\
	    if ((os_hooks) && (os_hooks)->save_fpu) {	\
		(os_hooks)->save_fpu();			\
	    }						\
	} while (0)

#define V3_RestoreFPU()						\
	do {							\
	    extern struct v3_os_hooks * os_hooks;		\
	    if ((os_hooks) && (os_hooks)->restore_fpu) {	\
		(os_hooks)->restore_fpu();			\
	    }							\
	} while (0)

#include <palacios/vmm_mem.h>
#include <palacios/vmm_types.h>

//#include <palacios/vmm_types.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm_telemetry.h>




// Maybe make this a define....
typedef enum v3_cpu_arch { V3_INVALID_CPU, 
			   V3_SVM_CPU, 
			   V3_SVM_REV3_CPU, 
			   V3_VMX_CPU, 
			   V3_VMX_EPT_CPU, 
			   V3_VMX_EPT_UG_CPU } v3_cpu_arch_t;


v3_cpu_mode_t v3_get_host_cpu_mode( void );

void v3_yield(struct v3_core_info * core, int usec);
void v3_yield_cond(struct v3_core_info * core, int usec);
void v3_yield_to_pid(struct v3_core_info * core, uint32_t pid, uint32_t tid);
void v3_yield_to_thread(struct v3_core_info * core, void * thread);

void v3_print_cond(const char * fmt, ...);

void v3_interrupt_cpu(struct v3_vm_info * vm, int logical_cpu, int vector);

struct v3_core_info * v3_get_current_core(void);


v3_cpu_arch_t v3_get_cpu_type(int cpu_id);


int v3_vm_enter(struct v3_core_info * core);
int v3_reset_vm_core(struct v3_core_info * core, addr_t rip);


#endif /*!__V3VEE__ */



struct v3_vm_info;

/* This will contain function pointers that provide OS services */
struct v3_os_hooks {
    void (*print)(const char * format, ...)
  	__attribute__ ((format (printf, 1, 2)));
  
    void *(*allocate_pages)(int num_pages, unsigned int alignment, int node_id);
    void (*free_pages)(void * page, int num_pages);

    void *(*malloc)(unsigned int size);
    void (*free)(void * addr);

    void *(*paddr_to_vaddr)(void * addr);
    void *(*vaddr_to_paddr)(void * addr);

    unsigned int (*get_cpu_khz)(void);

    
    void (*yield_cpu)(void); 
    void (*yield_to_pid)(unsigned int pid, unsigned int tid);  /* Optional: If not set, will default to regular yield */
    void (*yield_to_thread)(void * thread);                    /* Optional: If not set, will default to regular yield */
    void (*sleep_cpu)(unsigned int usec);
    void (*wakeup_cpu)(void *cpu);

    void (*save_fpu)(void);
    void (*restore_fpu)(void);

    void *(*mutex_alloc)(void);
    void (*mutex_free)(void * mutex);
    void (*mutex_lock)(void * mutex);
    void (*mutex_unlock)(void * mutex);

    unsigned int (*get_cpu)(void);



    void * (*create_thread)(int (*fn)(void * arg), void * arg, char * thread_name); 
    void * (*create_thread_on_cpu)(int cpu_id, int (*fn)(void * arg), void * arg, char * thread_name);
    void (*start_thread)(void * thread);
    void (*interrupt_cpu)(struct v3_vm_info * vm, int logical_cpu, int vector);
    void (*call_on_cpu)(int logical_cpu, void (*fn)(void * arg), void * arg);
    int (*move_thread_to_cpu)(int cpu_id,  void * thread);

};
  




/* 
 * Memory region definition
 */
struct v3_guest_mem_region {
    uint64_t start;
    uint64_t end;
};

/* 
 * CPU Thread info
 */
struct v3_thread_info {
    void    * host_thread;
    uint32_t  phys_cpu_id;
};


int Init_V3(struct v3_os_hooks * hooks, char * cpus, int num_cpus, char * options);
int Shutdown_V3( void );


struct v3_vm_info * v3_create_vm(void * cfg, void * priv_data, char * name);
int v3_start_vm(struct v3_vm_info * vm, unsigned int cpu_mask);
int v3_stop_vm(struct v3_vm_info * vm);
int v3_pause_vm(struct v3_vm_info * vm);
int v3_continue_vm(struct v3_vm_info * vm);
int v3_simulate_vm(struct v3_vm_info * vm, unsigned int msecs);


int v3_save_vm(struct v3_vm_info * vm, char * store, char * url);
int v3_load_vm(struct v3_vm_info * vm, char * store, char * url);

int v3_send_vm(struct v3_vm_info * vm, char * store, char * url);
int v3_receive_vm(struct v3_vm_info * vm, char * store, char * url);

int v3_move_vm_core(struct v3_vm_info * vm, int vcore_id, int target_cpu);


int v3_free_vm(struct v3_vm_info * vm);


int v3_add_cpu(int cpu_id);
int v3_remove_cpu(int cpu_id);
int * v3_get_cpu_usage(int * num_cpus);


/* 
 * Returns an array of v3_guest_mem_regions specifying a VM's base memory region layout
 *  - implemented in vmm_mem.c
 */
struct v3_guest_mem_region * 
v3_get_guest_memory_regions(struct v3_vm_info * vm, int * num_regions);

/* 
 * Returns an array of host thread pointers, indexed by VCPU ID
 */
struct v3_thread_info *
v3_get_vm_thread_info(struct v3_vm_info * vm, int * num_threads);



#endif
