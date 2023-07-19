#
# Copyright 2021, Breakaway Consulting Pty. Ltd.
#
# SPDX-License-Identifier: BSD-2-Clause
#
ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR must be specified)
endif

ifeq ($(strip $(SEL4CP_SDK)),)
$(error SEL4CP_SDK must be specified)
endif

ifeq ($(strip $(SEL4CP_BOARD)),)
$(error SEL4CP_BOARD must be specified)
endif

ifeq ($(strip $(SEL4CP_CONFIG)),)
$(error SEL4CP_CONFIG must be specified)
endif

TOOLCHAIN := aarch64-none-elf

CPU := cortex-a53

CC := $(TOOLCHAIN)-gcc
LD := $(TOOLCHAIN)-ld
AS := $(TOOLCHAIN)-as
SEL4CP_TOOL ?= $(SEL4CP_SDK)/bin/sel4cp

XHCI_STUB_OBJS 		:=  xhci_stub.o dev_verbose.o subr_device.o imx8mq_usbphy.o usbdi_util.o usbdi.o usbroothub.o sel4_bus_funcs.o tinyalloc.o dwc3_fdt.o printf.o dma.o usb.o usb_quirks.o usb_subr.o xhci.o usb_mem.o util.o uhub.o timer.o hid.o uhidev.o ukbd.o hidkbdmap.o shared_ringbuffer.o
PIPE_HANDLE_OBJS 	:=  pipe_handler.o dev_verbose.o subr_device.o imx8mq_usbphy.o usbdi_util.o usbdi.o usbroothub.o sel4_bus_funcs.o tinyalloc.o dwc3_fdt.o printf.o dma.o usb.o usb_quirks.o usb_subr.o xhci.o usb_mem.o util.o uhub.o timer.o hid.o ukbd.o uhidev.o hidkbdmap.o shared_ringbuffer.o
# TIMER_OBJS 		:=  timer.o subr_device.o imx8mq_usbphy.o usbdi_util.o usbdi.o usbroothub.o sel4_bus_funcs.o tinyalloc.o dwc3_fdt.o printf.o dma.o usb.o usb_quirks.o usb_subr.o xhci.o usb_mem.o util.o uhub.o
SOFTWARE_OBJS 		:=  software_interrupts.o dev_verbose.o subr_device.o imx8mq_usbphy.o usbdi_util.o usbdi.o usbroothub.o sel4_bus_funcs.o tinyalloc.o dwc3_fdt.o printf.o dma.o usb.o usb_quirks.o usb_subr.o xhci.o usb_mem.o util.o uhub.o timer.o hid.o ukbd.o uhidev.o hidkbdmap.o shared_ringbuffer.o
HARDWARE_OBJS 		:=  hardware_interrupts.o sel4_bus_funcs.o tinyalloc.o printf.o util.o timer.o
MEM_OBJS			:=  mem_handler.o tinyalloc.o tinyalloc.o printf.o
KBD_LOGGER_OBJS 	:=  kbd_logger.o shared_ringbuffer.o printf.o tinyalloc.o hidkbdmap.o 
SIMULATED_KBD_OBJS	:=  simulated_kbd.o printf.o tinyalloc.o

BOARD_DIR := $(SEL4CP_SDK)/board/$(SEL4CP_BOARD)/$(SEL4CP_CONFIG)

IMAGES := xhci_stub.elf hardware.elf pipe_handler.elf software.elf mem_handler.elf kbd_logger.elf simulated_kbd.elf
INC := $(BOARD_DIR)/include include/tinyalloc include/wrapper include/netbsd_include include/bus include/dma include/printf include/timer
INC_PARAMS=$(foreach d, $(INC), -I$d)
WARNINGS := -Wall -Wno-comment -Wno-unused-function -Wno-return-type -Wno-unused-value
CFLAGS := -mcpu=$(CPU) -mstrict-align -ffreestanding -g3 -O3 $(WARNINGS) $(INC_PARAMS) -I$(BOARD_DIR)/include # -DSEL4_USB_DEBUG
LDFLAGS := -L$(BOARD_DIR)/lib
LIBS := -lsel4cp -Tsel4cp.ld

IMAGE_FILE = $(BUILD_DIR)/loader.img
REPORT_FILE = $(BUILD_DIR)/report.txt

all: $(IMAGE_FILE)

$(BUILD_DIR)/%.o: src/%.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@
	
$(BUILD_DIR)/%.o: %.s Makefile
	$(AS) -g3 -mcpu=$(CPU) $< -o $@

$(BUILD_DIR)/mem_handler.elf: $(addprefix $(BUILD_DIR)/, $(MEM_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/software.elf: $(addprefix $(BUILD_DIR)/, $(SOFTWARE_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/xhci_stub.elf: $(addprefix $(BUILD_DIR)/, $(XHCI_STUB_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/hardware.elf: $(addprefix $(BUILD_DIR)/, $(HARDWARE_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/pipe_handler.elf: $(addprefix $(BUILD_DIR)/, $(PIPE_HANDLE_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/kbd_logger.elf: $(addprefix $(BUILD_DIR)/, $(KBD_LOGGER_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/simulated_kbd.elf: $(addprefix $(BUILD_DIR)/, $(SIMULATED_KBD_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(IMAGE_FILE) $(REPORT_FILE): $(addprefix $(BUILD_DIR)/, $(IMAGES)) xhci_stub.system
	$(SEL4CP_TOOL) xhci_stub.system --search-path $(BUILD_DIR) --board $(SEL4CP_BOARD) --config $(SEL4CP_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

# clean
clean:
	rm -f *.o *.elf .depend*
	find . -name \*.o |xargs --no-run-if-empty rm
