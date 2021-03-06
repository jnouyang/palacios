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


#ifndef __VMM_PAGING_H__
#define __VMM_PAGING_H__


#ifdef __V3VEE__

#include <palacios/vmm_types.h>
#include <palacios/vmm_util.h>





#define MAX_PDE32_ENTRIES          1024
#define MAX_PTE32_ENTRIES          1024

#define MAX_PDPE32PAE_ENTRIES      4
#define MAX_PDE32PAE_ENTRIES       512
#define MAX_PTE32PAE_ENTRIES       512

#define MAX_PML4E64_ENTRIES        512
#define MAX_PDPE64_ENTRIES         512
#define MAX_PDE64_ENTRIES          512
#define MAX_PTE64_ENTRIES          512


typedef enum {PAGE_4KB, PAGE_2MB, PAGE_4MB, PAGE_1GB, 
	      PAGE_NOT_PRESENT,
	      PAGE_PT32, PAGE_PD32, 
	      PAGE_PDP32PAE, PAGE_PD32PAE, PAGE_PT32PAE,
	      PAGE_PML464, PAGE_PDP64, PAGE_PD64, PAGE_PT64} page_type_t;


/* Converts an address into a page table index */
#define PDE32_INDEX(x)  ((((uint32_t)x) >> 22) & 0x3ff)
#define PTE32_INDEX(x)  ((((uint32_t)x) >> 12) & 0x3ff)


#define PDPE32PAE_INDEX(x) ((((uint32_t)x) >> 30) & 0x3)
#define PDE32PAE_INDEX(x)  ((((uint32_t)x) >> 21) & 0x1ff)
#define PTE32PAE_INDEX(x)  ((((uint32_t)x) >> 12) & 0x1ff)

#define PML4E64_INDEX(x) ((((uint64_t)x) >> 39) & 0x1ff)
#define PDPE64_INDEX(x) ((((uint64_t)x) >> 30) & 0x1ff)
#define PDE64_INDEX(x) ((((uint64_t)x) >> 21) & 0x1ff)
#define PTE64_INDEX(x) ((((uint64_t)x) >> 12) & 0x1ff)


/* Gets the base address needed for a Page Table entry */
#define PAGE_BASE_ADDR(x) ((x) >> 12)
#define PAGE_BASE_ADDR_4KB(x) ((x) >> 12)
#define PAGE_BASE_ADDR_2MB(x) ((x) >> 21)
#define PAGE_BASE_ADDR_4MB(x) ((x) >> 22)
#define PAGE_BASE_ADDR_1GB(x) ((x) >> 30)

#define BASE_TO_PAGE_ADDR(x) (((addr_t)x) << 12)
#define BASE_TO_PAGE_ADDR_4KB(x) (((addr_t)x) << 12)
#define BASE_TO_PAGE_ADDR_2MB(x) (((addr_t)x) << 21)
#define BASE_TO_PAGE_ADDR_4MB(x) (((addr_t)x) << 22)
#define BASE_TO_PAGE_ADDR_1GB(x) (((addr_t)x) << 30)
/* *** */


#define PAGE_OFFSET(x) ((x) & 0xfffULL)
#define PAGE_OFFSET_4KB(x) ((x) & 0xfffULL)
#define PAGE_OFFSET_2MB(x) ((x) & 0x1fffffULL)
#define PAGE_OFFSET_4MB(x) ((x) & 0x3fffffULL)
#define PAGE_OFFSET_1GB(x) ((x) & 0x3fffffffULL)

#define PAGE_POWER 12
#define PAGE_POWER_4KB 12
#define PAGE_POWER_2MB 21
#define PAGE_POWER_4MB 22
#define PAGE_POWER_1GB 30

// We shift instead of mask because we don't know the address size
#define PAGE_ADDR(x) (((x) >> PAGE_POWER) << PAGE_POWER)
#define PAGE_ADDR_4KB(x) (((x) >> PAGE_POWER_4KB) << PAGE_POWER_4KB)
#define PAGE_ADDR_2MB(x) (((x) >> PAGE_POWER_2MB) << PAGE_POWER_2MB)
#define PAGE_ADDR_4MB(x) (((x) >> PAGE_POWER_4MB) << PAGE_POWER_4MB)
#define PAGE_ADDR_1GB(x) (((x) >> PAGE_POWER_1GB) << PAGE_POWER_1GB)

#define PAGE_SIZE     0x00001000ULL
#define PAGE_SIZE_4KB PAGE_SIZE
#define PAGE_SIZE_2MB 0x00200000ULL
#define PAGE_SIZE_4MB 0x00400000ULL
#define PAGE_SIZE_1GB 0x40000000ULL

/* *** */





#define CR3_TO_PDE32_PA(cr3) ((addr_t)(((uint_t)cr3) & 0xfffff000))
#define CR3_TO_PDPE32PAE_PA(cr3) ((addr_t)(((uint_t)cr3) & 0xffffffe0))
#define CR3_TO_PML4E64_PA(cr3)  ((addr_t)(((uint64_t)cr3) & 0x000ffffffffff000LL))

#define CR3_TO_PDE32_VA(cr3) ((pde32_t *)V3_VAddr((void *)(addr_t)(((uint_t)cr3) & 0xfffff000)))
#define CR3_TO_PDPE32PAE_VA(cr3) ((pdpe32pae_t *)V3_VAddr((void *)(addr_t)(((uint_t)cr3) & 0xffffffe0)))
#define CR3_TO_PML4E64_VA(cr3)  ((pml4e64_t *)V3_VAddr((void *)(addr_t)(((uint64_t)cr3) & 0x000ffffffffff000LL)))





/* We'll use the general form for now.... 
   typedef enum {PDE32_ENTRY_NOT_PRESENT, PDE32_ENTRY_PTE32, PDE32_ENTRY_LARGE_PAGE} pde32_entry_type_t;
   typedef enum {PTE32_ENTRY_NOT_PRESENT, PTE32_ENTRY_PAGE} pte32_entry_type_t;
   
   typedef enum {PDPE32PAE_ENTRY_NOT_PRESENT, PDPE32PAE_ENTRY_PAGE} pdpe32pae_entry_type_t;
   typedef enum {PDE32PAE_ENTRY_NOT_PRESENT, PDE32PAE_ENTRY_PTE32, PDE32PAE_ENTRY_LARGE_PAGE} pde32pae_entry_type_t;
   typedef enum {PTE32PAE_ENTRY_NOT_PRESENT, PTE32PAE_ENTRY_PAGE} pte32pae_entry_type_t;
   
   typedef enum {PML4E64_ENTRY_NOT_PRESENT, PML4E64_ENTRY_PAGE} pml4e64_entry_type_t;
   typedef enum {PDPE64_ENTRY_NOT_PRESENT, PDPE64_ENTRY_PTE32, PDPE64_ENTRY_LARGE_PAGE} pdpe64_entry_type_t;
   typedef enum {PDE64_ENTRY_NOT_PRESENT, PDE64_ENTRY_PTE32, PDE64_ENTRY_LARGE_PAGE} pde64_entry_type_t;
   typedef enum {PTE64_ENTRY_NOT_PRESENT, PTE64_ENTRY_PAGE} pte64_entry_type_t;
*/


typedef enum {PT_ENTRY_NOT_PRESENT, PT_ENTRY_LARGE_PAGE, PT_ENTRY_PAGE} pt_entry_type_t;

typedef enum {PT_ACCESS_OK, PT_ACCESS_NOT_PRESENT, PT_ACCESS_WRITE_ERROR, PT_ACCESS_USER_ERROR} pt_access_status_t;

/* Page table flag values */
#define V3_LARGE_PG 0x2


typedef struct gen_pt {
    uint32_t present        : 1;
    uint32_t writable       : 1;
    uint32_t user_page      : 1;
} __attribute__((packed)) gen_pt_t;

typedef struct pde32 {
    uint32_t present         : 1;
    uint32_t writable        : 1;
    uint32_t user_page       : 1;
    uint32_t write_through   : 1;
    uint32_t cache_disable   : 1;
    uint32_t accessed        : 1;
    uint32_t reserved        : 1;
    uint32_t large_page      : 1;
    uint32_t global_page     : 1;
    uint32_t vmm_info        : 3;
    uint32_t pt_base_addr    : 20;
} __attribute__((packed))  pde32_t;

typedef struct pde32_4MB {
    uint32_t present         : 1;
    uint32_t writable        : 1;
    uint32_t user_page       : 1;
    uint32_t write_through   : 1;
    uint32_t cache_disable   : 1;
    uint32_t accessed        : 1;
    uint32_t dirty           : 1;
    uint32_t large_page      : 1;
    uint32_t global_page     : 1;
    uint32_t vmm_info        : 3;
    uint32_t pat             : 1;
    uint32_t rsvd            : 9;
    uint32_t page_base_addr  : 10;

} __attribute__((packed))  pde32_4MB_t;

typedef struct pte32 {
    uint32_t present         : 1;
    uint32_t writable        : 1;
    uint32_t user_page       : 1;
    uint32_t write_through   : 1;
    uint32_t cache_disable   : 1;
    uint32_t accessed        : 1;
    uint32_t dirty           : 1;
    uint32_t pte_attr        : 1;
    uint32_t global_page     : 1;
    uint32_t vmm_info        : 3;
    uint32_t page_base_addr  : 20;
}  __attribute__((packed)) pte32_t;
/* ***** */

/* 32 bit PAE PAGE STRUCTURES */
typedef struct pdpe32pae {
    uint64_t present       : 1;
    uint64_t rsvd          : 2; // MBZ
    uint64_t write_through : 1;
    uint64_t cache_disable : 1;
    uint64_t accessed      : 1; 
    uint64_t avail         : 1;
    uint64_t rsvd2         : 2;  // MBZ
    uint64_t vmm_info      : 3;
    uint64_t pd_base_addr  : 24;
    uint64_t rsvd3         : 28; // MBZ
} __attribute__((packed)) pdpe32pae_t;



typedef struct pde32pae {
    uint64_t present         : 1;
    uint64_t writable        : 1;
    uint64_t user_page       : 1;
    uint64_t write_through   : 1;
    uint64_t cache_disable   : 1;
    uint64_t accessed        : 1;
    uint64_t avail           : 1;
    uint64_t large_page      : 1;
    uint64_t global_page     : 1;
    uint64_t vmm_info        : 3;
    uint64_t pt_base_addr    : 24;
    uint64_t rsvd            : 28;
} __attribute__((packed)) pde32pae_t;

typedef struct pde32pae_2MB {
    uint64_t present         : 1;
    uint64_t writable        : 1;
    uint64_t user_page       : 1;
    uint64_t write_through   : 1;
    uint64_t cache_disable   : 1;
    uint64_t accessed        : 1;
    uint64_t dirty           : 1;
    uint64_t one             : 1;
    uint64_t global_page     : 1;
    uint64_t vmm_info        : 3;
    uint64_t pat             : 1;
    uint64_t rsvd            : 8;
    uint64_t page_base_addr  : 15;
    uint64_t rsvd2           : 28;

} __attribute__((packed)) pde32pae_2MB_t;

typedef struct pte32pae {
    uint64_t present         : 1;
    uint64_t writable        : 1;
    uint64_t user_page       : 1;
    uint64_t write_through   : 1;
    uint64_t cache_disable   : 1;
    uint64_t accessed        : 1;
    uint64_t dirty           : 1;
    uint64_t pte_attr        : 1;
    uint64_t global_page     : 1;
    uint64_t vmm_info        : 3;
    uint64_t page_base_addr  : 24;
    uint64_t rsvd            : 28;
} __attribute__((packed)) pte32pae_t;





/* ********** */


/* LONG MODE 64 bit PAGE STRUCTURES */
typedef struct pml4e64 {
    uint64_t present        : 1;
    uint64_t writable       : 1;
    uint64_t user_page      : 1;
    uint64_t write_through  : 1;
    uint64_t cache_disable  : 1;
    uint64_t accessed       : 1;
    uint64_t reserved       : 1;
    uint64_t zero           : 2;
    uint64_t vmm_info       : 3;
    uint64_t pdp_base_addr  : 40;
    uint64_t available      : 11;
    uint64_t no_execute     : 1;
} __attribute__((packed)) pml4e64_t;


typedef struct pdpe64 {
    uint64_t present        : 1;
    uint64_t writable       : 1;
    uint64_t user_page      : 1;
    uint64_t write_through  : 1;
    uint64_t cache_disable  : 1;
    uint64_t accessed       : 1;
    uint64_t avail          : 1;
    uint64_t large_page     : 1;
    uint64_t zero           : 1;
    uint64_t vmm_info       : 3;
    uint64_t pd_base_addr   : 40;
    uint64_t available      : 11;
    uint64_t no_execute     : 1;
} __attribute__((packed)) pdpe64_t;


// We Don't support this
typedef struct pdpe64_1GB {
    uint64_t present        : 1;
    uint64_t writable       : 1;
    uint64_t user_page      : 1;
    uint64_t write_through  : 1;
    uint64_t cache_disable  : 1;
    uint64_t accessed       : 1;
    uint64_t dirty          : 1;
    uint64_t large_page     : 1;
    uint64_t global_page    : 1;
    uint64_t vmm_info       : 3;
    uint64_t pat            : 1;
    uint64_t rsvd           : 17;
    uint64_t page_base_addr : 22;
    uint64_t available      : 11;
    uint64_t no_execute     : 1;
} __attribute__((packed)) pdpe64_1GB_t;



typedef struct pde64 {
    uint64_t present         : 1;
    uint64_t writable        : 1;
    uint64_t user_page       : 1;
    uint64_t write_through   : 1;
    uint64_t cache_disable   : 1;
    uint64_t accessed        : 1;
    uint64_t avail           : 1;
    uint64_t large_page      : 1;
    uint64_t global_page     : 1;
    uint64_t vmm_info        : 3;
    uint64_t pt_base_addr    : 40;
    uint64_t available       : 11;
    uint64_t no_execute      : 1;
} __attribute__((packed)) pde64_t;

typedef struct pde64_2MB {
    uint64_t present           : 1;
    uint64_t writable          : 1;
    uint64_t user_page         : 1;
    uint64_t write_through     : 1;
    uint64_t cache_disable     : 1;
    uint64_t accessed          : 1;
    uint64_t dirty             : 1;
    uint64_t large_page        : 1;
    uint64_t global_page       : 1;
    uint64_t vmm_info          : 3;
    uint64_t pat               : 1;
    uint64_t rsvd              : 8;
    uint64_t page_base_addr    : 31;
    uint64_t available         : 11;
    uint64_t no_execute        : 1;
} __attribute__((packed)) pde64_2MB_t;


typedef struct pte64 {
    uint64_t present          : 1;
    uint64_t writable         : 1;
    uint64_t user_page        : 1;
    uint64_t write_through    : 1;
    uint64_t cache_disable    : 1;
    uint64_t accessed         : 1;
    uint64_t dirty            : 1;
    uint64_t pte_attr         : 1;
    uint64_t global_page      : 1;
    uint64_t vmm_info         : 3;
    uint64_t page_base_addr   : 40;
    uint64_t available        : 11;
    uint64_t no_execute       : 1;
} __attribute__((packed)) pte64_t;

/* *************** */

typedef struct pf_error_code {
    uint32_t present           : 1; // if 0, fault due to page not present
    uint32_t write             : 1; // if 1, faulting access was a write
    uint32_t user              : 1; // if 1, faulting access was in user mode
    uint32_t rsvd_access       : 1; // if 1, fault from reading a 1 from a reserved field (?)
    uint32_t ifetch            : 1; // if 1, faulting access was an instr fetch (only with NX)
    uint32_t rsvd              : 27;
} __attribute__((packed)) pf_error_t;





struct v3_core_info;

int v3_translate_guest_pt_32(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr, addr_t * paddr);
int v3_translate_guest_pt_32pae(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr, addr_t * paddr);
int v3_translate_guest_pt_64(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr, addr_t * paddr);

int v3_translate_host_pt_32(struct v3_core_info * core, v3_reg_t host_cr3, addr_t vaddr, addr_t * paddr);
int v3_translate_host_pt_32pae(struct v3_core_info * core, v3_reg_t host_cr3, addr_t vaddr, addr_t * paddr);
int v3_translate_host_pt_64(struct v3_core_info * core, v3_reg_t host_cr3, addr_t vaddr, addr_t * paddr);


int v3_find_host_pt_32_page(struct v3_core_info * core, v3_reg_t host_cr3, page_type_t type, addr_t vaddr, 
			    addr_t * page_ptr, addr_t * page_pa);
int v3_find_host_pt_32pae_page(struct v3_core_info * core, v3_reg_t host_cr3, page_type_t type, addr_t vaddr, 
			       addr_t * page_ptr, addr_t * page_pa);
int v3_find_host_pt_64_page(struct v3_core_info * core, v3_reg_t host_cr3, page_type_t type, addr_t vaddr, 
			    addr_t * page_ptr, addr_t * page_pa);
int v3_find_guest_pt_32_page(struct v3_core_info * core, v3_reg_t guest_cr3, 
			     page_type_t type, addr_t vaddr, 
			     addr_t * page_ptr, addr_t * page_pa);
int v3_find_guest_pt_32pae_page(struct v3_core_info * core, v3_reg_t guest_cr3, 
				page_type_t type, addr_t vaddr, 
				addr_t * page_ptr, addr_t * page_pa);
int v3_find_guest_pt_64_page(struct v3_core_info * core, v3_reg_t guest_cr3, 
			     page_type_t type, addr_t vaddr, 
			     addr_t * page_ptr, addr_t * page_pa);



pt_access_status_t inline v3_can_access_pde32(pde32_t * pde, addr_t addr, pf_error_t access_type);
pt_access_status_t inline v3_can_access_pte32(pte32_t * pte, addr_t addr, pf_error_t access_type);

pt_access_status_t inline v3_can_access_pdpe32pae(pdpe32pae_t * pdpe, addr_t addr, pf_error_t access_type);
pt_access_status_t inline v3_can_access_pde32pae(pde32pae_t * pde, addr_t addr, pf_error_t access_type);
pt_access_status_t inline v3_can_access_pte32pae(pte32pae_t * pte, addr_t addr, pf_error_t access_type);

pt_access_status_t inline v3_can_access_pml4e64(pml4e64_t * pmle, addr_t addr, pf_error_t access_type);
pt_access_status_t inline v3_can_access_pdpe64(pdpe64_t * pdpe, addr_t addr, pf_error_t access_type);
pt_access_status_t inline v3_can_access_pde64(pde64_t * pde, addr_t addr, pf_error_t access_type);
pt_access_status_t inline v3_can_access_pte64(pte64_t * pte, addr_t addr, pf_error_t access_type);


int v3_check_host_pt_32(struct v3_core_info * core, v3_reg_t host_cr3, addr_t vaddr, 
			pf_error_t access_type, pt_access_status_t * access_status);
int v3_check_host_pt_32pae(struct v3_core_info * core, v3_reg_t host_cr3, addr_t vaddr, 
			   pf_error_t access_type, pt_access_status_t * access_status);
int v3_check_host_pt_64(struct v3_core_info * core, v3_reg_t host_cr3, addr_t vaddr, 
			pf_error_t access_type, pt_access_status_t * access_status);
int v3_check_guest_pt_32(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr, 
			 pf_error_t access_type, pt_access_status_t * access_status);
int v3_check_guest_pt_32pae(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr, 
			    pf_error_t access_type, pt_access_status_t * access_status);
int v3_check_guest_pt_64(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr, 
			 pf_error_t access_type, pt_access_status_t * access_status);



page_type_t v3_get_guest_data_page_type_32(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr);
page_type_t v3_get_guest_data_page_type_32pae(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr);
page_type_t v3_get_guest_data_page_type_64(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr);
page_type_t v3_get_host_data_page_type_32(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr);
page_type_t v3_get_host_data_page_type_32pae(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr);
page_type_t v3_get_host_data_page_type_64(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr);


int v3_drill_host_pt_32(struct v3_core_info * core, v3_reg_t host_cr3, addr_t vaddr,
			int (*callback)(struct v3_core_info * core, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			void * private_data);
int v3_drill_host_pt_32pae(struct v3_core_info * core, v3_reg_t host_cr3, addr_t vaddr,
			   int (*callback)(struct v3_core_info * core, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			   void * private_data);
int v3_drill_host_pt_64(struct v3_core_info * core, v3_reg_t host_cr3, addr_t vaddr,
			int (*callback)(struct v3_core_info * core, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			void * private_data);

int v3_drill_guest_pt_32(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr,
			 int (*callback)(struct v3_core_info * core, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			 void * private_data);
int v3_drill_guest_pt_32pae(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr,
			    int (*callback)(struct v3_core_info * core, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			    void * private_data);
int v3_drill_guest_pt_64(struct v3_core_info * core, v3_reg_t guest_cr3, addr_t vaddr,
			 int (*callback)(struct v3_core_info * core, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			 void * private_data);




int v3_walk_host_pt_32(struct v3_core_info * core, v3_reg_t host_cr3,
		       int (*callback)(struct v3_core_info * core, page_type_t type, addr_t vaddr, addr_t page_va, addr_t page_pa, void * private_data),
		       void * private_data);

int v3_walk_host_pt_32pae(struct v3_core_info * core, v3_reg_t host_cr3,
			  int (*callback)(struct v3_core_info * core, page_type_t type, addr_t vaddr, addr_t page_va, addr_t page_pa, void * private_data),
			  void * private_data);

int v3_walk_host_pt_64(struct v3_core_info * core, v3_reg_t host_cr3,
		       int (*callback)(struct v3_core_info * core, page_type_t type, addr_t vaddr, addr_t page_va, addr_t page_pa, void * private_data),
		       void * private_data);

int v3_walk_guest_pt_32(struct v3_core_info * core, v3_reg_t guest_cr3,
			int (*callback)(struct v3_core_info * core, page_type_t type, addr_t vaddr, addr_t page_va, addr_t page_pa, void * private_data),
			void * private_data);

int v3_walk_guest_pt_32pae(struct v3_core_info * core, v3_reg_t guest_cr3,
			   int (*callback)(struct v3_core_info * core, page_type_t type, addr_t vaddr, addr_t page_va, addr_t page_pa, void * private_data),
			   void * private_data);

int v3_walk_guest_pt_64(struct v3_core_info * core, v3_reg_t guest_cr3,
			int (*callback)(struct v3_core_info * core, page_type_t type, addr_t vaddr, addr_t page_va, addr_t page_pa, void * private_data),
			void * private_data);
  

pde32_t     * create_passthrough_pts_32(struct v3_core_info * core);
pdpe32pae_t * create_passthrough_pts_32PAE(struct v3_core_info * core);
pml4e64_t   * create_passthrough_pts_64(struct v3_core_info * core);


void v3_delete_pgtables_32(pde32_t * pde);
void v3_delete_pgtables_32pae(pdpe32pae_t * pdpe);
void v3_delete_pgtables_64(pml4e64_t *  pml4);



const char * v3_page_type_to_str(page_type_t type);



void v3_print_pt_entry(struct v3_core_info * core, page_type_t type, addr_t vaddr, void * entry);
void v3_print_host_pgtables(struct v3_core_info * core,  v3_cpu_mode_t cpu_mode, addr_t cr3);
void v3_print_guest_pgtables(struct v3_core_info * core, addr_t cr3);
void v3_print_guest_pg_walk(struct v3_core_info * core, addr_t virtual_addr, addr_t cr3);
void v3_print_host_pg_walk(struct v3_core_info * core, addr_t virtual_addr, addr_t cr3);


#endif // !__V3VEE__



#endif
