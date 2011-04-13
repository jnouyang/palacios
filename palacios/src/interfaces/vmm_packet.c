/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Lei Xia <lxia@cs.northwestern.edu> 
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_types.h>
#include <palacios/vm_guest.h>
#include <interfaces/vmm_packet.h>

static struct v3_packet_hooks * packet_hooks = 0;

int V3_send_raw(const char * pkt, uint32_t len) {
    if(packet_hooks != NULL && packet_hooks->send != NULL){
    	return packet_hooks->send(pkt, len, NULL);
    }

    return -1;
}


int V3_packet_add_recver(const char * mac, struct v3_vm_info * vm){
    if(packet_hooks != NULL && packet_hooks->add_recver != NULL){
        return packet_hooks->add_recver(mac, vm);
    }

    return -1;
}


int V3_packet_del_recver(const char * mac, struct v3_vm_info * vm){
    if(packet_hooks != NULL && packet_hooks->del_recver != NULL){
        return packet_hooks->del_recver(mac, vm);
    }

    return -1;
}

void V3_Init_Packet(struct v3_packet_hooks * hooks) {
    packet_hooks = hooks;
    PrintDebug("V3 raw packet interface inited\n");

    return;
}
