/* Palacios memory manager 
 * (c) Jack Lange, 2010
 */

#include <asm/page_64_types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include <asm/delay.h>

#include <linux/hardirq.h> // provides in_atomic advisory check
//static struct list_head pools;

#include "palacios.h"
#include "mm.h"
#include "buddy.h"
#include "numa.h"


static struct buddy_memzone ** memzones   = NULL;
static uintptr_t             * seed_addrs = NULL;


#define MEM_BLOCK_SIZE_BYTES ((uint64_t)(V3_CONFIG_MEM_BLOCK_SIZE_MB * (1024 * 1024)))


u32 pg_allocs = 0;
u32 pg_frees  = 0;
u32 mallocs   = 0;
u32 frees     = 0;


// alignment is in bytes
uintptr_t 
alloc_palacios_pgs(u64 num_pages, 
		   u32 alignment, 
		   int node_id) 
{
    uintptr_t addr        = 0;
    int       mem_node_id = node_id;

    if (numa_num_nodes() == 1) {
        mem_node_id = 0;
    } else if (node_id == -1) {
	int cpu_id  = get_cpu();
	put_cpu();

	mem_node_id = numa_cpu_to_node(cpu_id);

    } else if (node_id >= numa_num_nodes()) {
	// We are a NUMA aware, and requested an invalid node
	ERROR("Requesting memory from an invalid NUMA node. (Node: %d) (%d nodes on system)\n",
	      node_id, numa_num_nodes());
	return 0;
    }

    v3_lnx_printk("Allocating %llu pages (%llu bytes) order=%d\n", 
		  num_pages, 
		  num_pages * PAGE_SIZE, 
		  get_order(num_pages * PAGE_SIZE) + PAGE_SHIFT);

    addr = buddy_alloc(memzones[mem_node_id], get_order(num_pages * PAGE_SIZE) + PAGE_SHIFT);
    
    if ((node_id == -1) && (!addr)) {

        for (mem_node_id = 0; mem_node_id < numa_num_nodes(); mem_node_id++) {
            addr = buddy_alloc(memzones[mem_node_id], get_order(num_pages * PAGE_SIZE) + PAGE_SHIFT);
   
            if (addr) break;
         }
        
    }

    if (!addr) {
        ERROR("Returning from alloc addr=%p, vaddr=%p\n", (void *)addr, __va(addr));
    }


    pg_allocs += num_pages;

    return addr;
}



void 
free_palacios_pgs(uintptr_t pg_addr,
		  u64       num_pages) 
{
    int node_id = numa_addr_to_node(pg_addr);

    //DEBUG("Freeing Memory page %p\n", (void *)pg_addr);
    
    buddy_free(memzones[node_id], pg_addr, get_order(num_pages * PAGE_SIZE) + PAGE_SHIFT);

    pg_frees += num_pages;
}


int 
add_palacios_memory(uintptr_t base_addr, 
		    u64       num_pages) 
{
    int pool_order = 0;
    int node_id    = 0;

    // This assumes that the memory region does not overlap nodes
    // This is a safe assumption with the standard v3_mem utility
    node_id = numa_addr_to_node(base_addr);

    if (node_id == -1) {
	ERROR("Error locating node for addr %p\n", (void *)base_addr);
	return -1;
    }

    DEBUG("Managing %dMB of memory starting at %llu (%lluMB)\n", 
	  (unsigned int)(num_pages * PAGE_SIZE) / (1024 * 1024), 
	  (unsigned long long)base_addr, 
	  (unsigned long long)(base_addr / (1024 * 1024)));


    //   pool_order = fls(num_pages); 

   pool_order = get_order(num_pages * PAGE_SIZE) + PAGE_SHIFT;

   buddy_add_pool(memzones[node_id], base_addr, pool_order);


   v3_lnx_printk("%p on node %d\n", (void *)base_addr, numa_addr_to_node(base_addr));
   
   return 0;
}



int 
remove_palacios_memory(uintptr_t base_addr) 
{
    int node_id = numa_addr_to_node(base_addr);

    return buddy_remove_pool(memzones[node_id], base_addr, 0);
}



int
palacios_init_mm( void )
{
    int num_nodes = numa_num_nodes();
    int node_id   = 0;

    memzones   = palacios_kmalloc(sizeof(struct buddy_memzone *) * num_nodes, GFP_KERNEL);
    seed_addrs = palacios_kmalloc(sizeof(uintptr_t)              * num_nodes, GFP_KERNEL);

    memset(memzones,   0, sizeof(struct buddy_memzone *) * num_nodes);
    memset(seed_addrs, 0, sizeof(uintptr_t)              * num_nodes);

    for (node_id = 0; node_id < num_nodes; node_id++) {
	struct buddy_memzone * zone = NULL;

	// Seed the allocator with a small set of pages to allow initialization to complete. 
	// For now we will just grab some random pages, but in the future we will need to grab NUMA specific regions
	// See: alloc_pages_node()

	{
	    struct page * pgs   = alloc_pages_node(node_id, GFP_KERNEL, MAX_ORDER - 1);

	    if (!pgs) {
		ERROR("Could not allocate initial memory block for node %d\n", node_id);
		BUG_ON(!pgs);
		return -1;
	    }

	    seed_addrs[node_id] = page_to_pfn(pgs) << PAGE_SHIFT;
	}

 	v3_lnx_printk("Allocated seed region on node %d (addr=%p)\n", node_id, (void *)seed_addrs[node_id]);
	v3_lnx_printk("Initializing Zone %d\n", node_id);

	zone = buddy_init(get_order(MEM_BLOCK_SIZE_BYTES) + PAGE_SHIFT, PAGE_SHIFT, node_id);

	if (zone == NULL) {
	    ERROR("Could not initialization memory management for node %d\n", node_id);
	    return -1;
	}

	v3_lnx_printk("Zone initialized, Adding seed region (order=%d)\n", 
		      (MAX_ORDER - 1) + PAGE_SHIFT);

	if (buddy_add_pool(zone, seed_addrs[node_id], (MAX_ORDER - 1) + PAGE_SHIFT) == -1) {
	    ERROR("Error adding buddy pool\n");
	    return -1;
	}

	memzones[node_id] = zone;
    }

    return 0;
}

int palacios_deinit_mm( void ) {
    int i = 0;
    
    for (i = 0; i < numa_num_nodes(); i++) {

	if (memzones[i]) {
	    buddy_deinit(memzones[i]);
	}

	// note that the memory is not onlined here - offlining and onlining
	// is the resposibility of the caller

	if (seed_addrs[i]) {
	    // free the seed regions
	    free_pages((uintptr_t)__va(seed_addrs[i]), MAX_ORDER - 1);
	}
    }

    palacios_kfree(seed_addrs);
    palacios_kfree(memzones);

    return 0;
}






void * 
palacios_kmalloc(size_t size, 
		 gfp_t  flags) 
{
    void * addr = NULL;

    if ((irqs_disabled() || in_atomic()) && ((flags & GFP_ATOMIC) == 0)) {
	WARNING("Allocating memory with Interrupts disabled!!!\n");
	WARNING("This is probably NOT want you want to do 99%% of the time\n");
	WARNING("If still want to do this, you may dismiss this warning by setting the GFP_ATOMIC flag directly\n");
	dump_stack();

	flags &= ~GFP_KERNEL;
	flags |= GFP_ATOMIC;
    }

    

    addr = kmalloc(size, flags);

    //    printk("V3_MALLOC: [%p]\n", addr);
    //    dump_stack();
    
    //    udelay(500);

    mallocs++;

    return addr;
}


void 
palacios_kfree(void * ptr) 
{
    
    //    printk("V3_FREE: [%p]\n", ptr);

    frees++;
    return kfree(ptr);
}


