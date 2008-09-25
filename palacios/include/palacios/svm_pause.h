/* Northwestern University */
/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */

#ifndef __SVM_PAUSE_H
#define __SVM_PAUSE_H

#ifdef __V3VEE__

#include <palacios/vm_guest.h>
#include <palacios/vmcb.h>
#include <palacios/vmm.h>


int handle_svm_pause(struct guest_info * info);


#endif // ! __V3VEE__

#endif
