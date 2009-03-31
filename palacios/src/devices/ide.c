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
#include <devices/ide.h>
#include <devices/pci.h>
#include "ide-types.h"
#include "atapi-types.h"

#define PRI_DEFAULT_IRQ 14
#define SEC_DEFAULT_IRQ 15


#define PRI_DATA_PORT         0x1f0
#define PRI_FEATURES_PORT     0x1f1
#define PRI_SECT_CNT_PORT     0x1f2
#define PRI_SECT_NUM_PORT     0x1f3
#define PRI_CYL_LOW_PORT      0x1f4
#define PRI_CYL_HIGH_PORT     0x1f5
#define PRI_DRV_SEL_PORT      0x1f6
#define PRI_CMD_PORT          0x1f7
#define PRI_CTRL_PORT         0x3f6
#define PRI_ADDR_REG_PORT     0x3f7

#define SEC_DATA_PORT         0x170
#define SEC_FEATURES_PORT     0x171
#define SEC_SECT_CNT_PORT     0x172
#define SEC_SECT_NUM_PORT     0x173
#define SEC_CYL_LOW_PORT      0x174
#define SEC_CYL_HIGH_PORT     0x175
#define SEC_DRV_SEL_PORT      0x176
#define SEC_CMD_PORT          0x177
#define SEC_CTRL_PORT         0x376
#define SEC_ADDR_REG_PORT     0x377


#define PRI_DMA_CMD_PORT      0xc000
#define PRI_DMA_STATUS_PORT   0xc002
#define PRI_DMA_PRD_PORT0     0xc004
#define PRI_DMA_PRD_PORT1     0xc005
#define PRI_DMA_PRD_PORT2     0xc006
#define PRI_DMA_PRD_PORT3     0xc007

#define SEC_DMA_CMD_PORT      0xc008
#define SEC_DMA_STATUS_PORT   0xc00a
#define SEC_DMA_PRD_PORT0     0xc00c
#define SEC_DMA_PRD_PORT1     0xc00d
#define SEC_DMA_PRD_PORT2     0xc00e
#define SEC_DMA_PRD_PORT3     0xc00f

#define DATA_BUFFER_SIZE 2048

static const char * ide_pri_port_strs[] = {"PRI_DATA", "PRI_FEATURES", "PRI_SECT_CNT", "PRI_SECT_NUM", 
					  "PRI_CYL_LOW", "PRI_CYL_HIGH", "PRI_DRV_SEL", "PRI_CMD",
					   "PRI_CTRL", "PRI_ADDR_REG"};


static const char * ide_sec_port_strs[] = {"SEC_DATA", "SEC_FEATURES", "SEC_SECT_CNT", "SEC_SECT_NUM", 
					  "SEC_CYL_LOW", "SEC_CYL_HIGH", "SEC_DRV_SEL", "SEC_CMD",
					   "SEC_CTRL", "SEC_ADDR_REG"};

static const char * ide_dma_port_strs[] = {"PRI_DMA_CMD", NULL,
					   "PRI_DMA_STATUS", NULL,
					   "PRI_DMA_PRD0", "PRI_DMA_PRD1", 
					   "PRI_DMA_PRD2", "PRI_DMA_PRD3",
					   "SEC_DMA_CMD", NULL,
					   "SEC_DMA_STATUS", NULL,
					   "SEC_DMA_PRD0","SEC_DMA_PRD1",
					   "SEC_DMA_PRD2","SEC_DMA_PRD3"};



static inline const char * io_port_to_str(uint16_t port) {
    if ((port >= PRI_DATA_PORT) && (port <= PRI_CMD_PORT)) {
	return ide_pri_port_strs[port - PRI_DATA_PORT];
    } else if ((port >= SEC_DATA_PORT) && (port <= SEC_CMD_PORT)) {
	return ide_sec_port_strs[port - SEC_DATA_PORT];
    } else if ((port == PRI_CTRL_PORT) || (port == PRI_ADDR_REG_PORT)) {
	return ide_pri_port_strs[port - PRI_CTRL_PORT + 8];
    } else if ((port == SEC_CTRL_PORT) || (port == SEC_ADDR_REG_PORT)) {
	return ide_sec_port_strs[port - SEC_CTRL_PORT + 8];
    } else if ((port >= PRI_DMA_CMD_PORT) && (port <= SEC_DMA_PRD_PORT3)) {
	return ide_dma_port_strs[port - PRI_DMA_CMD_PORT];
    } 
    return NULL;
}



static const char * ide_dev_type_strs[] = {"HARDDISK", "CDROM", "NONE"};


static inline const char * device_type_to_str(v3_ide_dev_type_t type) {
    if (type > 2) {
	return NULL;
    }

    return ide_dev_type_strs[type];
}



struct ide_cd_state {
    struct atapi_sense_data sense;
    uint_t current_lba;
    uint8_t atapi_cmd;
    struct atapi_error_recovery err_recovery;
};

struct ide_hd_state {

};

struct ide_drive {
    // Command Registers

    v3_ide_dev_type_t drive_type;

    union {
	struct v3_ide_cd_ops * cd_ops;
	struct v3_ide_hd_ops * hd_ops;
    };


    union {
	struct ide_cd_state cd_state;
	struct ide_hd_state hd_state;
    };

    char model[41];

    // Where we are in the data transfer
    uint_t transfer_index;

    // the length of a transfer
    // calculated for easy access
    uint_t transfer_length;


    // We have a local data buffer that we use for IO port accesses
    uint8_t data_buf[DATA_BUFFER_SIZE];


    void * private_data;
    
    union {
	uint8_t sector_count;             // 0x1f2,0x172
	struct atapi_irq_flags irq_flags;
    } __attribute__((packed));

    union {
	uint8_t sector_num;               // 0x1f3,0x173
	uint8_t lba0;
    };

    union {
	uint16_t cylinder;
	uint16_t lba12;


	struct {
	    uint8_t cylinder_low;       // 0x1f4,0x174
	    uint8_t cylinder_high;      // 0x1f5,0x175
	} __attribute__((packed));

	struct {
	    uint8_t lba1;
	    uint8_t lba2;
	} __attribute__((packed));


	// The transfer length requested by the CPU 
	uint16_t req_len;
    } __attribute__((packed));

    struct ide_dma_cmd_reg dma_cmd;
    struct ide_dma_status_reg dma_status;
    uint32_t dma_prd_addr;

};



struct ide_channel {
    struct ide_drive drives[2];

    // Command Registers
    struct ide_error_reg error_reg;     // [read] 0x1f1,0x171

    struct ide_features_reg features;

    struct ide_drive_head_reg drive_head; // 0x1f6,0x176

    struct ide_status_reg status;       // [read] 0x1f7,0x177
    uint8_t cmd_reg;                // [write] 0x1f7,0x177

    int irq; // this is temporary until we add PCI support

    struct pci_device * pci_dev;

    // Control Registers
    struct ide_ctrl_reg ctrl_reg; // [write] 0x3f6,0x376
};



struct ide_internal {
    struct ide_channel channels[2];
    struct vm_device * pci;
    struct pci_device * busmaster_pci;
};



static inline uint16_t be_to_le_16(const uint16_t val) {
    uint8_t * buf = (uint8_t *)&val;
    return (buf[0] << 8) | (buf[1]) ;
}

static inline uint16_t le_to_be_16(const uint16_t val) {
    return be_to_le_16(val);
}


static inline uint32_t be_to_le_32(const uint32_t val) {
    uint8_t * buf = (uint8_t *)&val;
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

static inline uint32_t le_to_be_32(const uint32_t val) {
    return be_to_le_32(val);
}


static inline int get_channel_index(ushort_t port) {
    if (((port & 0xfff8) == 0x1f0) ||
	((port & 0xfffe) == 0x3f6) || 
	((port & 0xfff8) == 0xc000)) {
	return 0;
    } else if (((port & 0xfff8) == 0x170) ||
	       ((port & 0xfffe) == 0x376) ||
	       ((port & 0xfff8) == 0xc008)) {
	return 1;
    }

    return -1;
}

static inline struct ide_channel * get_selected_channel(struct ide_internal * ide, ushort_t port) {
    int channel_idx = get_channel_index(port);    
    return &(ide->channels[channel_idx]);
}

static inline struct ide_drive * get_selected_drive(struct ide_channel * channel) {
    return &(channel->drives[channel->drive_head.drive_sel]);
}


static inline int is_lba_enabled(struct ide_channel * channel) {
    return channel->drive_head.lba_mode;
}


static void ide_raise_irq(struct vm_device * dev, struct ide_channel * channel) {
    if (channel->ctrl_reg.irq_disable == 0) {
	v3_raise_irq(dev->vm, channel->irq);
    }
}


static void drive_reset(struct ide_drive * drive) {
    drive->sector_count = 0x01;
    drive->sector_num = 0x01;
    
    if (drive->drive_type == IDE_CDROM) {
	drive->cylinder = 0xeb14;
    } else {
	drive->cylinder = 0x0000;
    }


    memset(drive->data_buf, 0, sizeof(drive->data_buf));
    drive->transfer_index = 0;

    // Send the reset signal to the connected device callbacks
    //     channel->drives[0].reset();
    //    channel->drives[1].reset();
}

static void channel_reset(struct ide_channel * channel) {
    
    // set busy and seek complete flags
    channel->status.val = 0x90;

    // Clear errors
    channel->error_reg.val = 0x01;

    // clear commands
    channel->cmd_reg = 0x00;

    channel->ctrl_reg.irq_disable = 0;
}

static void channel_reset_complete(struct ide_channel * channel) {
    channel->status.busy = 0;
    channel->status.ready = 1;

    channel->drive_head.head_num = 0;    
    
    drive_reset(&(channel->drives[0]));
    drive_reset(&(channel->drives[1]));
}


static void ide_abort_command(struct vm_device * dev, struct ide_channel * channel) {
    channel->status.val = 0x41; // Error + ready
    channel->error_reg.val = 0x04; // No idea...

    ide_raise_irq(dev, channel);
}


// Include the ATAPI interface handlers
#include "atapi.h"


static int write_dma_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct ide_internal * ide = (struct ide_internal *)(dev->private_data);
    struct ide_channel * channel = get_selected_channel(ide, port);
    struct ide_drive * drive = get_selected_drive(channel);

    if (length != 1) {
	PrintError("IDE: Invalid Write length on IDE port %x\n", port);
	return -1;
    }

    PrintDebug("IDE: Writing DMA Port %x (%s) (val=%x)\n", port, io_port_to_str(port), *(uint8_t *)src);

    switch (port) {
	case PRI_DMA_CMD_PORT:
	case SEC_DMA_CMD_PORT:
	    drive->dma_cmd.val = *(uint8_t *)src;
	    break;

	case PRI_DMA_STATUS_PORT:
	case SEC_DMA_STATUS_PORT:
	    drive->dma_status.val = *(uint8_t *)src;
	    break;

	case PRI_DMA_PRD_PORT0:
	case PRI_DMA_PRD_PORT1:
	case PRI_DMA_PRD_PORT2:
	case PRI_DMA_PRD_PORT3:
	case SEC_DMA_PRD_PORT0:
	case SEC_DMA_PRD_PORT1:
	case SEC_DMA_PRD_PORT2:
	case SEC_DMA_PRD_PORT3: {
	    uint_t addr_index = port & 0x3;
	    uint8_t * addr_buf = (uint8_t *)&(drive->dma_prd_addr);

	    addr_buf[addr_index] = *(uint8_t *)src;
	    break;
	}
	default:
	    PrintError("IDE: Invalid DMA Port (%x)\n", port);
	    return -1;
    }

    return length;
}


static int read_dma_port(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
    struct ide_internal * ide = (struct ide_internal *)(dev->private_data);
    struct ide_channel * channel = get_selected_channel(ide, port);
    struct ide_drive * drive = get_selected_drive(channel);

    if (length != 1) {
	PrintError("IDE: Invalid Write length on IDE port %x\n", port);
	return -1;
    }



    switch (port) {
	case PRI_DMA_CMD_PORT:
	case SEC_DMA_CMD_PORT:
	    *(uint8_t *)dst = drive->dma_cmd.val;
	    break;

	case PRI_DMA_STATUS_PORT:
	case SEC_DMA_STATUS_PORT:
	    *(uint8_t *)dst = drive->dma_status.val;
	    break;

	case PRI_DMA_PRD_PORT0:
	case PRI_DMA_PRD_PORT1:
	case PRI_DMA_PRD_PORT2:
	case PRI_DMA_PRD_PORT3:
	case SEC_DMA_PRD_PORT0:
	case SEC_DMA_PRD_PORT1:
	case SEC_DMA_PRD_PORT2:
	case SEC_DMA_PRD_PORT3: {
 	    uint_t addr_index = port & 0x3;
	    uint8_t * addr_buf = (uint8_t *)&(drive->dma_prd_addr);

	    *(uint8_t *)dst = addr_buf[addr_index];
	    break;
	}
	default:
	    PrintError("IDE: Invalid DMA Port (%x)\n", port);
	    return -1;
    }

    PrintDebug("IDE: Reading DMA Port %x (%s) (val=%x)\n", port, io_port_to_str(port), *(uint8_t *)dst);

    return length;
}



static int write_cmd_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct ide_internal * ide = (struct ide_internal *)(dev->private_data);
    struct ide_channel * channel = get_selected_channel(ide, port);
    struct ide_drive * drive = get_selected_drive(channel);

    if (length != 1) {
	PrintError("Invalid Write Length on IDE command Port %x\n", port);
	return -1;
    }

    PrintDebug("IDE: Writing Command Port %x (%s) (val=%x)\n", port, io_port_to_str(port), *(uint8_t *)src);
    
    channel->cmd_reg = *(uint8_t *)src;
    
    switch (channel->cmd_reg) {
	
	case 0xa0: // ATAPI Command Packet
	    if (drive->drive_type != IDE_CDROM) {
		ide_abort_command(dev, channel);
	    }
	    
	    drive->sector_count = 1;

	    channel->status.busy = 0;
	    channel->status.write_fault = 0;
	    channel->status.data_req = 1;
	    channel->status.error = 0;

	    // reset the data buffer...
	    drive->transfer_length = ATAPI_PACKET_SIZE;
	    drive->transfer_index = 0;

	    break;
	case 0xa1: // ATAPI Identify Device Packet
	    atapi_identify_device(drive);

	    channel->error_reg.val = 0;
	    channel->status.val = 0x58; // ready, data_req, seek_complete
	    
	    ide_raise_irq(dev, channel);
	    break;
	case 0xec: // Identify Device
	    if (drive->drive_type != IDE_DISK) {
		drive_reset(drive);

		// JRL: Should we abort here?
		ide_abort_command(dev, channel);
	    } else {
		PrintError("IDE Disks currently not implemented\n");
		return -1;
	    }
	    break;
	default:
	    PrintError("Unimplemented IDE command (%x)\n", channel->cmd_reg);
	    return -1;
    }

    return length;
}


static int write_data_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct ide_internal * ide = (struct ide_internal *)(dev->private_data);
    struct ide_channel * channel = get_selected_channel(ide, port);
    struct ide_drive * drive = get_selected_drive(channel);

    //    PrintDebug("IDE: Writing Data Port %x (val=%x, len=%d)\n", 
    //	       port, *(uint32_t *)src, length);
    
    memcpy(drive->data_buf + drive->transfer_index, src, length);    
    drive->transfer_index += length;

    // Transfer is complete, dispatch the command
    if (drive->transfer_index >= drive->transfer_length) {
	switch (channel->cmd_reg) {
	    case 0x30: // Write Sectors
		PrintError("Writing Data not yet implemented\n");
		return -1;
		
	    case 0xa0: // ATAPI packet command
		if (atapi_handle_packet(dev, channel) == -1) {
		    PrintError("Error handling ATAPI packet\n");
		    return -1;
		}
		break;
	    default:
		PrintError("Unhandld IDE Command %x\n", channel->cmd_reg);
		return -1;
	}
    }

    return length;
}


static int read_hd_data(uint8_t * dst, uint_t length, struct vm_device * dev, struct ide_channel * channel) {
    PrintError("Harddrive data port read not implemented\n");
    return -1;
}



static int read_cd_data(uint8_t * dst, uint_t length, struct vm_device * dev, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    int data_offset = drive->transfer_index % DATA_BUFFER_SIZE;
    int req_offset = drive->transfer_index % drive->req_len;
    
    if (drive->cd_state.atapi_cmd != 0x28) {
        PrintDebug("IDE: Reading CD Data (len=%d) (req_len=%d)\n", length, drive->req_len);
    }

    if (drive->transfer_index >= drive->transfer_length) {
	PrintError("Buffer Overrun... (xfer_len=%d) (cur_idx=%d) (post_idx=%d)\n", 
		   drive->transfer_length, drive->transfer_index, 
		   drive->transfer_index + length);
	return -1;
    }



    if ((data_offset == 0) && (drive->transfer_index > 0)) {
	
	if (drive->drive_type == IDE_CDROM) {
	    if (atapi_update_data_buf(dev, channel) == -1) {
		PrintError("Could not update CDROM data buffer\n");
		return -1;
	    } 
	} else {
	    PrintError("IDE Harddrives not implemented\n");
	    return -1;
	}
    }

    memcpy(dst, drive->data_buf + data_offset, length);
    
    drive->transfer_index += length;

    if ((req_offset == 0) && (drive->transfer_index > 0)) {
	if (drive->transfer_index < drive->transfer_length) {
	    // An increment is complete, but there is still more data to be transferred...
	    
	    channel->status.data_req = 1;

	    drive->irq_flags.c_d = 0;

	    // Update the request length in the cylinder regs
	    if (atapi_update_req_len(dev, channel, drive->transfer_length - drive->transfer_index) == -1) {
		PrintError("Could not update request length after completed increment\n");
		return -1;
	    }
	} else {
	    // This was the final read of the request
	    channel->status.data_req = 0;
	    channel->status.ready = 1;
	    
	    drive->irq_flags.c_d = 1;
	    drive->irq_flags.rel = 0;
	}

	drive->irq_flags.io_dir = 1;
	channel->status.busy = 0;

	ide_raise_irq(dev, channel);
    }

    return length;
}


static int read_drive_id(uint8_t * dst, uint_t length, struct vm_device * dev, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);

    channel->status.busy = 0;
    channel->status.ready = 1;
    channel->status.write_fault = 0;
    channel->status.seek_complete = 1;
    channel->status.corrected = 0;
    channel->status.error = 0;
		
    
    memcpy(dst, drive->data_buf + drive->transfer_index, length);
    drive->transfer_index += length;
    
    if (drive->transfer_index >= drive->transfer_length) {
	channel->status.data_req = 0;
    }
    
    return length;
}


static int ide_read_data_port(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
    struct ide_internal * ide = (struct ide_internal *)(dev->private_data);
    struct ide_channel * channel = get_selected_channel(ide, port);
    struct ide_drive * drive = get_selected_drive(channel);

    //    PrintDebug("IDE: Reading Data Port %x (len=%d)\n", port, length);

    if ((channel->cmd_reg == 0xec) ||
	(channel->cmd_reg == 0xa1)) {
	return read_drive_id((uint8_t *)dst, length, dev, channel);
    }

    if (drive->drive_type == IDE_CDROM) {
	if (read_cd_data((uint8_t *)dst, length, dev, channel) == -1) {
	    PrintError("IDE: Could not read CD Data\n");
	    return -1;
	}
    } else if (drive->drive_type == IDE_DISK) {
	if (read_hd_data((uint8_t *)dst, length, dev, channel) == -1) {
	    PrintError("IDE: Could not read HD Data\n");
	    return -1;
	}
    } else {
	memset((uint8_t *)dst, 0, length);
    }

    return length;
}

static int write_port_std(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct ide_internal * ide = (struct ide_internal *)(dev->private_data);
    struct ide_channel * channel = get_selected_channel(ide, port);
    struct ide_drive * drive = get_selected_drive(channel);
	    
    if (length != 1) {
	PrintError("Invalid Write length on IDE port %x\n", port);
	return -1;
    }

    PrintDebug("IDE: Writing Standard Port %x (%s) (val=%x)\n", port, io_port_to_str(port), *(uint8_t *)src);


    switch (port) {
	// reset and interrupt enable
	case PRI_CTRL_PORT:
	case SEC_CTRL_PORT: {
	    struct ide_ctrl_reg * tmp_ctrl = (struct ide_ctrl_reg *)src;

	    // only reset channel on a 0->1 reset bit transition
	    if ((!channel->ctrl_reg.soft_reset) && (tmp_ctrl->soft_reset)) {
		channel_reset(channel);
	    } else if ((channel->ctrl_reg.soft_reset) && (!tmp_ctrl->soft_reset)) {
		channel_reset_complete(channel);
	    }

	    channel->ctrl_reg.val = tmp_ctrl->val;	    
	    break;
	}
	case PRI_FEATURES_PORT:
	case SEC_FEATURES_PORT:
	    channel->features.val = *(uint8_t *)src;
	    break;

	case PRI_SECT_CNT_PORT:
	case SEC_SECT_CNT_PORT:
	    drive->sector_count = *(uint8_t *)src;
	    break;

	case PRI_SECT_NUM_PORT:
	case SEC_SECT_NUM_PORT:
	    drive->sector_num = *(uint8_t *)src;

	case PRI_CYL_LOW_PORT:
	case SEC_CYL_LOW_PORT:
	    drive->cylinder_low = *(uint8_t *)src;
	    break;

	case PRI_CYL_HIGH_PORT:
	case SEC_CYL_HIGH_PORT:
	    drive->cylinder_high = *(uint8_t *)src;
	    break;

	case PRI_DRV_SEL_PORT:
	case SEC_DRV_SEL_PORT: {
	    channel->drive_head.val = *(uint8_t *)src;
	    
	    // make sure the reserved bits are ok..
	    // JRL TODO: check with new ramdisk to make sure this is right...
	    channel->drive_head.val |= 0xa0;

	    drive = get_selected_drive(channel);

	    // Selecting a non-present device is a no-no
	    if (drive->drive_type == IDE_NONE) {
		PrintDebug("Attempting to select a non-present drive\n");
		channel->error_reg.abort = 1;
		channel->status.error = 1;
	    }

	    break;
	}
	default:
	    PrintError("IDE: Write to unknown Port %x\n", port);
	    return -1;
    }
    return length;
}


static int read_port_std(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
    struct ide_internal * ide = (struct ide_internal *)(dev->private_data);
    struct ide_channel * channel = get_selected_channel(ide, port);
    struct ide_drive * drive = get_selected_drive(channel);
    
    if (length != 1) {
	PrintError("Invalid Read length on IDE port %x\n", port);
	return -1;
    }

    PrintDebug("IDE: Reading Standard Port %x (%s)\n", port, io_port_to_str(port));


    if ((port == PRI_ADDR_REG_PORT) ||
	(port == SEC_ADDR_REG_PORT)) {
	// unused, return 0xff
	*(uint8_t *)dst = 0xff;
	return length;
    }


    // if no drive is present just return 0 + reserved bits
    if (drive->drive_type == IDE_NONE) {
	if ((port == PRI_DRV_SEL_PORT) ||
	    (port == SEC_DRV_SEL_PORT)) {
	    *(uint8_t *)dst = 0xa0;
	} else {
	    *(uint8_t *)dst = 0;
	}

	return length;
    }

    switch (port) {

	// This is really the error register.
	case PRI_FEATURES_PORT:
	case SEC_FEATURES_PORT:
	    *(uint8_t *)dst = channel->error_reg.val;
	    break;
	    
	case PRI_SECT_CNT_PORT:
	case SEC_SECT_CNT_PORT:
	    *(uint8_t *)dst = drive->sector_count;
	    break;

	case PRI_SECT_NUM_PORT:
	case SEC_SECT_NUM_PORT:
	    *(uint8_t *)dst = drive->sector_num;
	    break;

	case PRI_CYL_LOW_PORT:
	case SEC_CYL_LOW_PORT:
	    *(uint8_t *)dst = drive->cylinder_low;
	    break;


	case PRI_CYL_HIGH_PORT:
	case SEC_CYL_HIGH_PORT:
	    *(uint8_t *)dst = drive->cylinder_high;
	    break;

	case PRI_DRV_SEL_PORT:
	case SEC_DRV_SEL_PORT:  // hard disk drive and head register 0x1f6
	    *(uint8_t *)dst = channel->drive_head.val;
	    break;

	case PRI_CTRL_PORT:
	case SEC_CTRL_PORT:
	case PRI_CMD_PORT:
	case SEC_CMD_PORT:
	    // Something about lowering interrupts here....
	    *(uint8_t *)dst = channel->status.val;
	    break;

	default:
	    PrintError("Invalid Port: %x\n", port);
	    return -1;
    }

    PrintDebug("\tVal=%x\n", *(uint8_t *)dst);

    return length;
}



static void init_drive(struct ide_drive * drive) {

    drive->sector_count = 0x01;
    drive->sector_num = 0x01;
    drive->cylinder = 0x0000;

    drive->drive_type = IDE_NONE;

    memset(drive->model, 0, sizeof(drive->model));

    drive->transfer_index = 0;
    drive->transfer_length = 0;
    memset(drive->data_buf, 0, sizeof(drive->data_buf));

    drive->dma_cmd.val = 0;
    drive->dma_status.val = 0;
    drive->dma_prd_addr = 0;

    drive->private_data = NULL;
    drive->cd_ops = NULL;
}

static void init_channel(struct ide_channel * channel) {
    int i = 0;

    channel->error_reg.val = 0x01;
    channel->drive_head.val = 0x00;
    channel->status.val = 0x00;
    channel->cmd_reg = 0x00;
    channel->ctrl_reg.val = 0x08;


    for (i = 0; i < 2; i++) {
	init_drive(&(channel->drives[i]));
    }

}

/*
static int pci_config_update(struct pci_device * pci_dev, uint_t reg_num, int length) {
    PrintError("IDE does not handle PCI config updates\n");
    return -1;
}
*/

static int pci_bar_update(struct pci_device * pci_dev, uint_t bar) {
    PrintError("IDE does not support bar updates\n");
    return -1;
}


static int init_ide_state(struct vm_device * dev) {
    struct ide_internal * ide = (struct ide_internal *)(dev->private_data);
    struct v3_pci_bar bars[6];
    struct pci_device * pci_dev = NULL;
    int i, j;

    for (i = 0; i < 2; i++) {
	init_channel(&(ide->channels[i]));

	// JRL: this is a terrible hack...
	ide->channels[i].irq = PRI_DEFAULT_IRQ + i;

	for (j = 0; j < 6; j++) {
	    bars[j].type = PCI_BAR_NONE;
	    bars[j].mem_hook = 0;
	    bars[j].num_pages = 0;
	    bars[j].bar_update = NULL;
	}


	bars[4].type = PCI_BAR_IO;
	bars[4].bar_update = pci_bar_update;

	pci_dev = v3_pci_register_device(ide->pci, PCI_STD_DEVICE, 0, "V3_IDE", -1, bars,
					 NULL, NULL, NULL, dev);

	if (pci_dev == NULL) {
	    PrintError("Failed to register IDE BUS %d with PCI\n", i); 
	    return -1;
	}

	ide->channels[i].pci_dev = pci_dev;

	pci_dev->config_header.vendor_id = 0x1095;
	pci_dev->config_header.device_id = 0x0646;
	pci_dev->config_header.revision = 0x8f07;
	pci_dev->config_header.subclass = 0x01;
	pci_dev->config_header.class = 0x01;
    }



    /* Register PIIX3 Busmaster PCI device */
    for (j = 0; j < 6; j++) {
	bars[j].type = PCI_BAR_NONE;
	bars[j].mem_hook = 0;
	bars[j].num_pages = 0;
	bars[j].bar_update = NULL;
    }

    pci_dev = v3_pci_register_device(ide->pci, PCI_STD_DEVICE, 0, "PIIX3 IDE", -1, bars,
				     NULL, NULL, NULL, dev);
    
    
    ide->busmaster_pci = pci_dev;

    pci_dev->config_header.vendor_id = 0x8086;
    pci_dev->config_header.device_id = 0x7010;
    pci_dev->config_header.revision = 0x80;
    pci_dev->config_header.subclass = 0x01;
    pci_dev->config_header.class = 0x01;


    return 0;
}



static int init_ide(struct vm_device * dev) {
    //struct ide_internal * ide = (struct ide_internal *)(dev->private_data);

    PrintDebug("IDE: Initializing IDE\n");

    if (init_ide_state(dev) == -1) {
	PrintError("Failed to initialize IDE state\n");
	return -1;
    }




    v3_dev_hook_io(dev, PRI_DATA_PORT, 
		   &ide_read_data_port, &write_data_port);
    v3_dev_hook_io(dev, PRI_FEATURES_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, PRI_SECT_CNT_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, PRI_SECT_NUM_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, PRI_CYL_LOW_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, PRI_CYL_HIGH_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, PRI_DRV_SEL_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, PRI_CMD_PORT, 
		   &read_port_std, &write_cmd_port);

    v3_dev_hook_io(dev, SEC_DATA_PORT, 
		   &ide_read_data_port, &write_data_port);
    v3_dev_hook_io(dev, SEC_FEATURES_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, SEC_SECT_CNT_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, SEC_SECT_NUM_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, SEC_CYL_LOW_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, SEC_CYL_HIGH_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, SEC_DRV_SEL_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, SEC_CMD_PORT, 
		   &read_port_std, &write_cmd_port);
  

    v3_dev_hook_io(dev, PRI_CTRL_PORT, 
		   &read_port_std, &write_port_std);

    v3_dev_hook_io(dev, SEC_CTRL_PORT, 
		   &read_port_std, &write_port_std);
  

    v3_dev_hook_io(dev, SEC_ADDR_REG_PORT, 
		   &read_port_std, &write_port_std);

    v3_dev_hook_io(dev, PRI_ADDR_REG_PORT, 
		   &read_port_std, &write_port_std);



    v3_dev_hook_io(dev, PRI_DMA_CMD_PORT, 
		   &read_dma_port, &write_dma_port);
    v3_dev_hook_io(dev, PRI_DMA_STATUS_PORT, 
		   &read_dma_port, &write_dma_port);
    v3_dev_hook_io(dev, PRI_DMA_PRD_PORT0, 
		   &read_dma_port, &write_dma_port);
    v3_dev_hook_io(dev, PRI_DMA_PRD_PORT0, 
		   &read_dma_port, &write_dma_port);
    v3_dev_hook_io(dev, PRI_DMA_PRD_PORT0, 
		   &read_dma_port, &write_dma_port);
    v3_dev_hook_io(dev, PRI_DMA_PRD_PORT0, 
		   &read_dma_port, &write_dma_port);


    v3_dev_hook_io(dev, SEC_DMA_CMD_PORT, 
		   &read_dma_port, &write_dma_port);
    v3_dev_hook_io(dev, SEC_DMA_STATUS_PORT, 
		   &read_dma_port, &write_dma_port);
    v3_dev_hook_io(dev, SEC_DMA_PRD_PORT0, 
		   &read_dma_port, &write_dma_port);
    v3_dev_hook_io(dev, SEC_DMA_PRD_PORT0, 
		   &read_dma_port, &write_dma_port);
    v3_dev_hook_io(dev, SEC_DMA_PRD_PORT0, 
		   &read_dma_port, &write_dma_port);
    v3_dev_hook_io(dev, SEC_DMA_PRD_PORT0, 
		   &read_dma_port, &write_dma_port);

    return 0;
}


static int deinit_ide(struct vm_device * dev) {
    // unhook io ports....
    // deregister from PCI?
    return 0;
}


static struct vm_device_ops dev_ops = {
    .init = init_ide,
    .deinit = deinit_ide,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};


struct vm_device *  v3_create_ide(struct vm_device * pci) {
    struct ide_internal * ide  = (struct ide_internal *)V3_Malloc(sizeof(struct ide_internal));  
    struct vm_device * device = v3_create_device("IDE", &dev_ops, ide);

    ide->pci = pci;

    PrintDebug("IDE: Creating IDE bus x 2\n");

    return device;
}





int v3_ide_register_cdrom(struct vm_device * ide_dev, 
			  uint_t bus_num, 
			  uint_t drive_num,
			  char * dev_name, 
			  struct v3_ide_cd_ops * ops, 
			  void * private_data) {

    struct ide_internal * ide  = (struct ide_internal *)(ide_dev->private_data);  
    struct ide_channel * channel = NULL;
    struct ide_drive * drive = NULL;

    V3_ASSERT((bus_num >= 0) && (bus_num < 2));
    V3_ASSERT((drive_num >= 0) && (drive_num < 2));

    channel = &(ide->channels[bus_num]);
    drive = &(channel->drives[drive_num]);
    
    if (drive->drive_type != IDE_NONE) {
	PrintError("Device slot (bus=%d, drive=%d) already occupied\n", bus_num, drive_num);
	return -1;
    }

    strncpy(drive->model, dev_name, sizeof(drive->model) - 1);

    while (strlen((char *)(drive->model)) < 40) {
	strcat((char*)(drive->model), " ");
    }


    drive->drive_type = IDE_CDROM;

    drive->cd_ops = ops;

    drive->private_data = private_data;

    return 0;
}


int v3_ide_register_harddisk(struct vm_device * ide_dev, 
			     uint_t bus_num, 
			     uint_t drive_num, 
			     char * dev_name, 
			     struct v3_ide_hd_ops * ops, 
			     void * private_data) {

    struct ide_internal * ide  = (struct ide_internal *)(ide_dev->private_data);  
    struct ide_channel * channel = NULL;
    struct ide_drive * drive = NULL;

    V3_ASSERT((bus_num >= 0) && (bus_num < 2));
    V3_ASSERT((drive_num >= 0) && (drive_num < 2));

    channel = &(ide->channels[bus_num]);
    drive = &(channel->drives[drive_num]);
    
    if (drive->drive_type != IDE_NONE) {
	PrintError("Device slot (bus=%d, drive=%d) already occupied\n", bus_num, drive_num);
	return -1;
    }

    strncpy(drive->model, dev_name, sizeof(drive->model) - 1);

    drive->drive_type = IDE_DISK;

    drive->hd_ops = ops;

    drive->private_data = private_data;

    return 0;
}