/*
 * Physical memory allocation
 * Copyright (c) 2001,2003,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 * $Revision: 1.1 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#ifndef GEEKOS_MEM_H
#define GEEKOS_MEM_H

#include <geekos/ktypes.h>
#include <geekos/defs.h>
#include <geekos/list.h>
#include <geekos/paging.h>

struct Boot_Info;

/*
 * Page flags
 */
#define PAGE_AVAIL     0x0000	 /* page is on the freelist */
#define PAGE_KERN      0x0001	 /* page used by kernel code or data */
#define PAGE_HW        0x0002	 /* page used by hardware (e.g., ISA hole) */
#define PAGE_ALLOCATED 0x0004	 /* page is allocated */
#define PAGE_UNUSED    0x0008	 /* page is unused */
#define PAGE_HEAP      0x0010	 /* page is in kernel heap */
#define PAGE_PAGEABLE  0x0020	 /* page can be paged out */
#define PAGE_LOCKED    0x0040    /* page is taken should not be freed */
#define PAGE_VM        0x0080    /* page is used by the VM */


struct Page;

/*
 * List datatype for doubly-linked list of Pages.
 */
DEFINE_LIST(Page_List, Page);

/*
 * Each page of physical memory has one of these structures
 * associated with it, to do allocation and bookkeeping.
 */
struct Page {
    unsigned flags;			 /* Flags indicating state of page */
    DEFINE_LINK(Page_List, Page);	 /* Link fields for Page_List */
    int clock;
    ulong_t vaddr;			 /* User virtual address where page is mapped */
    pte_t *entry;			 /* Page table entry referring to the page */
};

IMPLEMENT_LIST(Page_List, Page);

void Init_Mem(struct Boot_Info* bootInfo);
void Init_BSS(void);
void* Alloc_Page(void);
void* Alloc_Pageable_Page(pte_t *entry, ulong_t vaddr);
void Free_Page(void* pageAddr);

/*
 * Determine if given address is a multiple of the page size.
 */
static __inline__ bool Is_Page_Multiple(ulong_t addr)
{
    return addr == (addr & ~(PAGE_MASK));
}

/*
 * Round given address up to a multiple of the page size
 */
static __inline__ ulong_t Round_Up_To_Page(ulong_t addr)
{
    if ((addr & PAGE_MASK) != 0) {
	addr &= ~(PAGE_MASK);
	addr += PAGE_SIZE;
    }
    return addr;
}

/*
 * Round given address down to a multiple of the page size
 */
static __inline__ ulong_t Round_Down_To_Page(ulong_t addr)
{
    return addr & (~PAGE_MASK);
}

/*
 * Get the index of the page in memory.
 */
static __inline__ int Page_Index(ulong_t addr)
{
    return (int) (addr >> PAGE_POWER);
}


/*
 * Get the Page struct associated with given address.
 */
static __inline__ struct Page *Get_Page(ulong_t addr)
{
    extern struct Page* g_pageList;
      return &g_pageList[Page_Index(addr)];
      //return g_pageList + (sizeof(struct Page) * (int)(addr >> PAGE_POWER));
}

/*
 * Get the physical address of the memory represented by given Page object.
 */
static __inline__ ulong_t Get_Page_Address(struct Page *page)
{
    extern struct Page* g_pageList;
    ulong_t index = page - g_pageList;
    return index << PAGE_POWER;
}

#endif  /* GEEKOS_MEM_H */
