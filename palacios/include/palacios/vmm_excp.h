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

#ifndef __VMM_EXCP_H__
#define __VMM_EXCP_H__

#ifdef __V3VEE__


#include <palacios/vmm_types.h>
#include <palacios/vmm_lock.h>

#define DE_EXCEPTION          0x00  
#define DB_EXCEPTION          0x01
#define NMI_EXCEPTION         0x02
#define BP_EXCEPTION          0x03
#define OF_EXCEPTION          0x04
#define BR_EXCEPTION          0x05
#define UD_EXCEPTION          0x06
#define NM_EXCEPTION          0x07
#define DF_EXCEPTION          0x08
#define TS_EXCEPTION          0x0a
#define NP_EXCEPTION          0x0b
#define SS_EXCEPTION          0x0c
#define GPF_EXCEPTION         0x0d
#define PF_EXCEPTION          0x0e
#define MF_EXCEPTION          0x10
#define AC_EXCEPTION          0x11
#define MC_EXCEPTION          0x12
#define XF_EXCEPTION          0x13
#define SX_EXCEPTION          0x1e


struct v3_core_info;

struct v3_excp_state {

    /* We need to rework the exception state, to handle stacking */
    uint32_t excp_bitmap;          /* 1 = excp pending,     0 = not pending   */
    uint32_t error_bitmap;         /* 1 = error code valid, 0 = no error code */
    uint32_t error_codes[32];
    
    v3_spinlock_t excp_lock;       /* Lock to allow remote cores to raise exceptions */
};


void v3_init_exception_state(struct v3_core_info * core);


int v3_raise_exception(struct v3_core_info * core, uint32_t excp);
int v3_raise_exception_with_error(struct v3_core_info * core, uint32_t excp, uint_t error_code);
int v3_raise_nmi(struct v3_core_info * core);

int v3_excp_pending(struct v3_core_info * core);
int v3_excp_has_error(struct v3_core_info * core, uint32_t excp);

uint32_t v3_get_excp_number(struct v3_core_info * core);
uint32_t v3_get_excp_error(struct v3_core_info * core, uint32_t excp);

int v3_injecting_excp(struct v3_core_info * core, uint32_t excp);


int v3_raise_nmi(struct v3_core_info * core);

#endif 

#endif
