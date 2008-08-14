; Definitions for use in GeekOS boot code
; Copyright (c) 2001, David H. Hovemeyer <daveho@cs.umd.edu>
; $Revision: 1.9 $

; This is free software.  You are permitted to use,
; redistribute, and modify it as specified in the file "COPYING".

; A lot of this code is adapted from Kernel Toolkit 0.2
; and Linux version 2.2.x, so the following copyrights apply:

; Copyright (C) 1991, 1992 Linus Torvalds
; modified by Drew Eckhardt
; modified by Bruce Evans (bde)
; adapted for Kernel Toolkit by Luigi Sgro

%ifndef DEFS_ASM
%define DEFS_ASM

;Zheng 08/01/2008
SYSSEG equ 0x1000
COMMAND_LINE_SIZE equ 1024

SETUPSECTS      equ 4            ; /* default nr of setup-sectors */
SYSSIZE         equ 0x7f00       ; /* system size: # of 16-byte clicks */
ROOT_DEV        equ 0            ; /* ROOT_DEV is now written by "build" */
SWAP_DEV        equ 0            ; /* SWAP_DEV is now written by "build" */

;VM_KERNEL_TARGET	equ 0x100000
	
; BIOS loads the boot sector at offset 0 in this segment
BOOTSEG equ 0x07C0
 
; We'll move the boot sector up to higher memory.
; Note that the "ISA hole" begins at segment 0xA000 == 640K.
INITSEG equ 0x9000

; Put the setup code here, just after the boot sector.
SETUPSEG equ 0x9020

; Load our "Kernel" at this segment, which starts at 64K.
; The number of sectors in the kernel, NUM_KERN_SECTORS,
; will be passed on the command line.
KERNSEG equ 0x1000

;; Load Segment for moving guest kernel to extended memory
;; Note that this segment must not colide with where the VMM kernel
;; is initially loaded.  At this point, we just place it in the last
;; 64K of RAM available before the ISA hole and hope for the best
 VMSEG equ 0x8000

; Size of PFAT boot record.
; Keep up to date with <geekos/pfat.h>.
PFAT_BOOT_RECORD_SIZE equ 28

; Offset of BIOS signature word in boot sector.
BIOS_SIGNATURE_OFFSET equ 510

; Offset of PFAT boot record in boot sector.
PFAT_BOOT_RECORD_OFFSET equ BIOS_SIGNATURE_OFFSET - PFAT_BOOT_RECORD_SIZE

; Video memory segment
VIDSEG equ 0xb800

; The following information is correct for a 1.44M floppy.
; Yes, I'm hard coding this.
;SECTORS_PER_TRACK equ 18	
HEADS equ 2
CYLINDERS equ 80

; 8259A PIC initialization codes.
; Source: Linux bootsect.S, and Intel 8259A datasheet

; The most important reason why we reprogram the PICs is to
; route the hardware interrupts through vectors *above*
; those reserved by Intel.  The BIOS (for historical reasons :-)
; routes them such that they conflict with internal processor-generated
; interrupts.

ICW1 equ 0x11		; ICW1 - ICW4 needed, cascade mode, interval=8,
			;   edge triggered. (I think interval is irrelevant
			;   for x86.)
ICW2_MASTER equ 0x20	; put IRQs 0-7 at 0x20 (above Intel reserved ints)
ICW2_SLAVE equ 0x28	; put IRQs 8-15 at 0x28
ICW3_MASTER equ 0x04	; IR2 connected to slave
ICW3_SLAVE equ 0x02	; slave has id 2
ICW4 equ 0x01		; 8086 mode, no auto-EOI, non-buffered mode,
			;   not special fully nested mode

; Kernel code and data segment selectors.
; Keep these up to date with defs.h.
KERNEL_CS equ 1<<3	; kernel code segment is GDT entry 1
KERNEL_DS equ 2<<3	; kernel data segment is GDT entry 2

; Pages for context object and stack for initial kernel thread -
; the one we construct for Main().  Keep these up to date with defs.h.
; We put them at 1MB, for no particular reason.
	;;  Moved to just after where the VM will go
KERN_THREAD_OBJ equ 4096
KERN_STACK equ KERN_THREAD_OBJ + 4096

%endif
