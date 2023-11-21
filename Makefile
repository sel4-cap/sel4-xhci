#
# Copyright 2021, Breakaway Consulting Pty. Ltd.
#
# SPDX-License-Identifier: BSD-2-Clause
#
ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR must be specified)
endif

ifeq ($(strip $(MICROKIT_SDK)),)
$(error MICROKIT_SDK must be specified)
endif

ifeq ($(strip $(MICROKIT_BOARD)),)
$(error MICROKIT_BOARD must be specified)
endif

ifeq ($(strip $(MICROKIT_CONFIG)),)
$(error MICROKIT_CONFIG must be specified)
endif

ifeq ($(strip $(MICROKIT_DIR)),)
$(error MICROKIT_DIR must be specified)
endif

LWIPDIR=lwip/src

TOOLCHAIN := aarch64-none-elf

CPU := cortex-a53

CC := $(TOOLCHAIN)-gcc
LD := $(TOOLCHAIN)-ld
AS := $(TOOLCHAIN)-as
MICROKIT_TOOL ?= $(MICROKIT_SDK)/bin/microkit

# NETIFFILES: Files implementing various generic network interface functions
NETIFFILES=$(LWIPDIR)/netif/ethernet.c

XHCI_STUB_OBJS 		:=  xhci_stub.o dev_verbose.o subr_device.o imx8mq_usbphy.o usbdi_util.o usbdi.o usbroothub.o sel4_bus_funcs.o tinyalloc.o dwc3_fdt.o printf.o dma.o usb.o usb_quirks.o usb_subr.o xhci.o usb_mem.o util.o uhub.o hid.o uhidev.o ukbd.o hidkbdmap.o shared_ringbuffer.o xhci_timer.o
PIPE_HANDLE_OBJS 	:=  pipe_handler.o dev_verbose.o subr_device.o imx8mq_usbphy.o usbdi_util.o usbdi.o usbroothub.o sel4_bus_funcs.o tinyalloc.o dwc3_fdt.o printf.o dma.o usb.o usb_quirks.o usb_subr.o xhci.o usb_mem.o util.o uhub.o hid.o ukbd.o uhidev.o hidkbdmap.o shared_ringbuffer.o xhci_timer.o
# TIMER_OBJS 		:=  timer.o subr_device.o imx8mq_usbphy.o usbdi_util.o usbdi.o usbroothub.o sel4_bus_funcs.o tinyalloc.o dwc3_fdt.o printf.o dma.o usb.o usb_quirks.o usb_subr.o xhci.o usb_mem.o util.o uhub.o
SOFTWARE_OBJS 		:=  software_interrupts.o dev_verbose.o subr_device.o imx8mq_usbphy.o usbdi_util.o usbdi.o usbroothub.o sel4_bus_funcs.o tinyalloc.o dwc3_fdt.o printf.o dma.o usb.o usb_quirks.o usb_subr.o xhci.o usb_mem.o util.o uhub.o hid.o ukbd.o uhidev.o hidkbdmap.o shared_ringbuffer.o xhci_timer.o 
HARDWARE_OBJS 		:=  hardware_interrupts.o sel4_bus_funcs.o tinyalloc.o printf.o util.o xhci_timer.o
MEM_OBJS			:=  mem_handler.o tinyalloc.o printf.o
KBD_LOGGER_OBJS 	:=  kbd_logger.o shared_ringbuffer.o printf.o hidkbdmap.o
SIMULATED_KBD_OBJS	:=  simulated_kbd.o printf.o
ETH_OBJS 			:=  eth.o shared_ringbuffer.o printf.o

# COREFILES, CORE4FILES: The minimum set of files needed for lwIP.
COREFILES=$(LWIPDIR)/core/init.c \
	$(LWIPDIR)/core/def.c \
	$(LWIPDIR)/core/dns.c \
	$(LWIPDIR)/core/inet_chksum.c \
	$(LWIPDIR)/core/ip.c \
	$(LWIPDIR)/core/mem.c \
	$(LWIPDIR)/core/memp.c \
	$(LWIPDIR)/core/netif.c \
	$(LWIPDIR)/core/pbuf.c \
	$(LWIPDIR)/core/raw.c \
	$(LWIPDIR)/core/stats.c \
	$(LWIPDIR)/core/sys.c \
	$(LWIPDIR)/core/altcp.c \
	$(LWIPDIR)/core/altcp_alloc.c \
	$(LWIPDIR)/core/altcp_tcp.c \
	$(LWIPDIR)/core/tcp.c \
	$(LWIPDIR)/core/tcp_in.c \
	$(LWIPDIR)/core/tcp_out.c \
	$(LWIPDIR)/core/timeouts.c \
	$(LWIPDIR)/core/udp.c

CORE4FILES=$(LWIPDIR)/core/ipv4/autoip.c \
	$(LWIPDIR)/core/ipv4/dhcp.c \
	$(LWIPDIR)/core/ipv4/etharp.c \
	$(LWIPDIR)/core/ipv4/icmp.c \
	$(LWIPDIR)/core/ipv4/igmp.c \
	$(LWIPDIR)/core/ipv4/ip4_frag.c \
	$(LWIPDIR)/core/ipv4/ip4.c \
	$(LWIPDIR)/core/ipv4/ip4_addr.c

CFLAGS_ETH += -I$(BOARD_DIR)/include \
	-Iinclude	\
	-Iinclude/arch	\
	-Iinclude/netbsd_include/sys/kmem  \
	-I$(LWIPDIR)/include \
	-I$(LWIPDIR)/include/ipv4 \
	-I$(RINGBUFFERDIR)/include

# LWIPFILES: All the above.
LWIPFILES=lwip.c $(COREFILES) $(CORE4FILES) $(NETIFFILES)
LWIP_OBJS := $(LWIPFILES:.c=.o) lwip.o shared_ringbuffer.o utilization_socket.o udp_echo_socket.o eth_timer.o printf.o 

BOARD_DIR := $(MICROKIT_SDK)/board/$(MICROKIT_BOARD)/$(MICROKIT_CONFIG)

IMAGES := xhci_stub.elf hardware.elf pipe_handler.elf software.elf mem_handler.elf kbd_logger.elf simulated_kbd.elf eth.elf lwip.elf
INC := $(BOARD_DIR)/include include/tinyalloc include/wrapper include/dma include/netbsd_include include/bus include/printf include/timer echo_server/include echo_server/libsharedringbuffer/include echo_server/lwip/src/include
INC_PARAMS=$(foreach d, $(INC), -I$d)
# INC_ETH := $(BOARD_DIR)/include $(MICROKIT_DIR)/libmicrokit/include
INC_ETH := $(BOARD_DIR)/include $(MICROKIT_DIR)/example/maaxboard/xhci_stub/echo_server/include $(MICROKIT_DIR)/example/maaxboard/xhci_stub/echo_server/lwip/src/include include/printf include/netbsd_include/sys/kmem
INC_PARAMS_ETH=$(foreach d, $(INC_ETH), -I$d)
WARNINGS := -Wall -Wno-comment -Wno-unused-function -Wno-return-type -Wno-unused-value
CFLAGS := -mcpu=$(CPU) -mstrict-align -ffreestanding -g3 -O3 $(WARNINGS) $(INC_PARAMS) -I$(BOARD_DIR)/include # -DSEL4_USB_DEBUG
CFLAGS_ETH := -mcpu=$(CPU) -mstrict-align -ffreestanding -g3 -O3 -Wall  -Wno-unused-function $(INC_PARAMS_ETH)
LDFLAGS := -L$(BOARD_DIR)/lib -L.
LIBS := -lmicrokit -Tmicrokit.ld -lc

IMAGE_FILE = $(BUILD_DIR)/loader.img
REPORT_FILE = $(BUILD_DIR)/report.txt

all: directories $(IMAGE_FILE)

$(BUILD_DIR)/%.o: echo_server/%.c Makefile
	$(CC) -c $(CFLAGS_ETH) $< -o $@

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

$(BUILD_DIR)/eth.elf: $(addprefix $(BUILD_DIR)/, $(ETH_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/lwip.elf: $(addprefix $(BUILD_DIR)/, $(LWIP_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(IMAGE_FILE) $(REPORT_FILE): $(addprefix $(BUILD_DIR)/, $(IMAGES)) xhci_stub.system
	$(MICROKIT_TOOL) xhci_stub.system --search-path $(BUILD_DIR) --board $(MICROKIT_BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

# %.o:
# 	$(CC) $(CFLAGS_ETH) $(@:.o=.c) -o $@

#Make the Directories
directories:
	$(info $(shell mkdir -p $(BUILD_DIR)/lwip/src))	\
	$(info $(shell mkdir -p $(BUILD_DIR)/lwip/src/core)) 	\
	$(info $(shell mkdir -p $(BUILD_DIR)/lwip/src/core/ipv4)) \
	$(info $(shell mkdir -p $(BUILD_DIR)/lwip/src/netif))	\
    $(info $(shell mkdir -p $(BUILD_DIR)/libsharedringbuffer))	\
	$(info $(shell mkdir -p $(BUILD_DIR)/benchmark))	\

# clean
clean:
	rm -f *.o *.elf .depend*
	find . -name \*.o |xargs --no-run-if-empty rm
