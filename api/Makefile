# Define toolchain
TOOLCHAIN   := aarch64-none-elf
CC_BSD      := $(TOOLCHAIN)-gcc
LD_BSD      := $(TOOLCHAIN)-ld
AS_BSD      := $(TOOLCHAIN)-as

NETBSD_SRC	:= dev_verbose.o subr_device.o subr_autoconf.o usbdi_util.o usbdi.o usbroothub.o sel4_bus_funcs.o dma.o usb.o usb_quirks.o usb_subr.o xhci.o usb_mem.o uhub.o hid.o uhidev.o ukbd.o ums.o uts.o hidms.o hidkbdmap.o ioconf.o tpcalib.o uhid.o umass.o umass_quirks.o umass_scsipi.o scsipi_base.o scsipiconf.o scsiconf.o scsi_base.o scsi_subr.o scsipi_ioctl.o heapsort.o strnvisx.o sd.o dksubr.o subr_disk.o  hexdump.o fdt_openfirm.o fdt_phy.o fdt_subr.o strlist.o ofw_subr.o pmatch.o fdt_reset.o bswap.o
FDT_SRC		:= fdt.o fdt_addresses.o fdt_empty_tree.o fdt_ro.o fdt_rw.o fdt_strerror.o fdt_sw.o fdt_wip.o
UTILS		:= timer.o printf.o tinyalloc.o util.o

XHCI_STUB_OBJS 	:=  xhci_stub.o $(NETBSD_SRC) $(FDT_SRC) imx8mq_usbphy.o dwc3_fdt.o shared_ringbuffer.o $(UTILS)
SOFTWARE_OBJS 	:=  software_interrupts.o $(NETBSD_SRC) $(FDT_SRC) imx8mq_usbphy.o dwc3_fdt.o $(UTILS) shared_ringbuffer.o
HARDWARE_OBJS 	:=  hardware_interrupts.o sel4_bus_funcs.o $(UTILS)
MEM_OBJS		:=  mem_handler.o printf.o tinyalloc.o util.o

INC 		:= $(BOARD_DIR)/include api/include/shared_ringbuffer api/include api/include/tinyalloc api/include/printf api/include/wrapper $(NETBSD_DIR)/sys $(NETBSD_DIR)/sys/external/bsd/libfdt/dist $(NETBSD_DIR)/mach_include api/include/bus api/include/dma api/include/printf api/include/timer api/utils/
INC_PARAMS	=$(foreach d, $(INC), -I$d)
WARNINGS 	:= -Wall -Wno-comment -Wno-return-type -Wno-unused-function -Wno-unused-value -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label -Wno-pointer-sign
CFLAGS 		:= -mcpu=$(CPU) -mstrict-align  -nostdlib -nolibc -ffreestanding -g3 -O3 $(WARNINGS) $(INC_PARAMS)  -DSEL4 # -DSEL4_USB_DEBUG
LD_BSDFLAGS := -L$(BOARD_DIR)/lib 
LIBS_BSD 	:= -lmicrokit -Tmicrokit.ld

API_IMAGES := xhci_stub.elf hardware.elf software.elf mem_handler.elf 

all: machine

# create machine directory
machine:
	@mkdir -p $(NETBSD_DIR)/mach_include/machine
	@ln -fs $(NETBSD_DIR)/sys/arch/evbarm/include/* $(NETBSD_DIR)/mach_include/machine/
	@mkdir -p $(NETBSD_DIR)/mach_include/arm
	@ln -fs $(NETBSD_DIR)/sys/arch/arm/include/* $(NETBSD_DIR)/mach_include/arm/
	@mkdir -p $(NETBSD_DIR)/mach_include/aarch64
	@ln -fs $(NETBSD_DIR)/sys/arch/arm/include/* $(NETBSD_DIR)/mach_include/aarch64/

$(BUILD_DIR)/%.o: $(NETBSD_DIR)/sys/external/bsd/libfdt/dist/%.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SEL4_XHCI_PATH)/api/utils/%.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SEL4_XHCI_PATH)/api/include/printf/%.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SEL4_XHCI_PATH)/api/include/tinyalloc/%.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SEL4_XHCI_PATH)/api/include/shared_ringbuffer/%.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SEL4_XHCI_PATH)/api/src/%.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SEL4_XHCI_PATH)/api/test/%.c Makefile
	$(CC_BSD) -c $(CFLAGS) -DSEL4_USB_DEBUG $< -o $@

$(BUILD_DIR)/usb.o: $(NETBSD_DIR)/sys/dev/usb/usb.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/usbdi.o: $(NETBSD_DIR)/sys/dev/usb/usbdi.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/usbdi_util.o: $(NETBSD_DIR)/sys/dev/usb/usbdi_util.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/usb_mem.o: $(NETBSD_DIR)/sys/dev/usb/usb_mem.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/usb_quirks.o: $(NETBSD_DIR)/sys/dev/usb/usb_quirks.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/usb_subr.o: $(NETBSD_DIR)/sys/dev/usb/usb_subr.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/usbroothub.o: $(NETBSD_DIR)/sys/dev/usb/usbroothub.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/umass.o: $(NETBSD_DIR)/sys/dev/usb/umass.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/umass_scsipi.o: $(NETBSD_DIR)/sys/dev/usb/umass_scsipi.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/umass_quirks.o: $(NETBSD_DIR)/sys/dev/usb/umass_quirks.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/scsiconf.o: $(NETBSD_DIR)/sys/dev/scsipi/scsiconf.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/scsi_subr.o: $(NETBSD_DIR)/sys/dev/scsipi/scsi_subr.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/scsi_base.o: $(NETBSD_DIR)/sys/dev/scsipi/scsi_base.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@
	
$(BUILD_DIR)/scsipi_base.o: $(NETBSD_DIR)/sys/dev/scsipi/scsipi_base.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/scsipiconf.o: $(NETBSD_DIR)/sys/dev/scsipi/scsipiconf.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/scsipi_ioctl.o: $(NETBSD_DIR)/sys/dev/scsipi/scsipi_ioctl.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@
$(BUILD_DIR)/pmatch.o: $(NETBSD_DIR)/sys/lib/libkern/pmatch.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/strlist.o: $(NETBSD_DIR)/sys/lib/libkern/strlist.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@
$(BUILD_DIR)/fdt_subr.o: $(NETBSD_DIR)/sys/dev/fdt/fdt_subr.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@
$(BUILD_DIR)/fdt_phy.o: $(NETBSD_DIR)/sys/dev/fdt/fdt_phy.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@
# $(BUILD_DIR)/fdt_clock.o: $(NETBSD_DIR)/sys/dev/fdt/fdt_clock.c Makefile
# 	$(CC_BSD) -c $(CFLAGS) $< -o $@
$(BUILD_DIR)/fdt_reset.o: $(NETBSD_DIR)/sys/dev/fdt/fdt_reset.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@
$(BUILD_DIR)/fdt_openfirm.o: $(NETBSD_DIR)/sys/dev/fdt/fdt_openfirm.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@
# $(BUILD_DIR)/clk.o: $(NETBSD_DIR)/sys/dev/clk/clk.c Makefile
# 	$(CC_BSD) -c $(CFLAGS) $< -o $@
# open firmware
$(BUILD_DIR)/openfirmio.o: $(NETBSD_DIR)/sys/dev/ofw/openfirmio.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@
$(BUILD_DIR)/ofw_sysctl.o: $(NETBSD_DIR)/sys/dev/ofw/ofw_sysctl.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@
$(BUILD_DIR)/ofw_network_subr.o: $(NETBSD_DIR)/sys/dev/ofw/ofw_network_subr.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@
$(BUILD_DIR)/ofw_spi_subr.o: $(NETBSD_DIR)/sys/dev/ofw/ofw_spi_subr Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@
$(BUILD_DIR)/ofw_subr.o: $(NETBSD_DIR)/sys/dev/ofw/ofw_subr.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/uts.o: $(NETBSD_DIR)/sys/dev/usb/uts.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/bswap.o: $(NETBSD_DIR)/sys/sys/bswap64.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/uhid.o: $(NETBSD_DIR)/sys/dev/usb/uhid.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/uhidev.o: $(NETBSD_DIR)/sys/dev/usb/uhidev.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/uhub.o: $(NETBSD_DIR)/sys/dev/usb/uhub.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/ukbd.o: $(NETBSD_DIR)/sys/dev/usb/ukbd.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/ums.o: $(NETBSD_DIR)/sys/dev/usb/ums.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/xhci.o: $(NETBSD_DIR)/sys/dev/usb/xhci.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/strnvisx.o: $(NETBSD_DIR)/sys/lib/libkern/strnvisx.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/sd.o: $(NETBSD_DIR)/sys/dev/scsipi/sd.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/dksubr.o: $(NETBSD_DIR)/sys/dev/dksubr.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/hexdump.o: $(NETBSD_DIR)/sys/lib/libkern/hexdump.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/heapsort.o: $(NETBSD_DIR)/common/lib/libc/stdlib/heapsort.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/subr_autoconf.o: $(NETBSD_DIR)/sys/kern/subr_autoconf.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/subr_device.o: $(NETBSD_DIR)/sys/kern/subr_device.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/subr_disk.o: $(NETBSD_DIR)/sys/kern/subr_disk.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

# $(BUILD_DIR)/subr_humanize.o: $(NETBSD_DIR)/sys/kern/subr_humanize.c Makefile
# 	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/kern_pmf.o: $(NETBSD_DIR)/sys/kern/kern_pmf.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/hid.o: $(NETBSD_DIR)/sys/dev/hid/hid.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/hidkbdmap.o: $(NETBSD_DIR)/sys/dev/hid/hidkbdmap.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/hidms.o: $(NETBSD_DIR)/sys/dev/hid/hidms.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/dev_verbose.o: $(NETBSD_DIR)/sys/dev/dev_verbose.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/dwc3_fdt.o: $(NETBSD_DIR)/sys/dev/fdt/dwc3_fdt.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/imx8mq_usbphy.o: $(NETBSD_DIR)/sys/arch/arm/nxp/imx8mq_usbphy.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/tpcalib.o: $(NETBSD_DIR)/sys/dev/wscons/tpcalib.c Makefile
	$(CC_BSD) -c $(CFLAGS) $< -o $@
	
$(BUILD_DIR)/%.o: %.s Makefile
	$(AS_BSD) -g3 -mcpu=$(CPU) $< -o $@

$(BUILD_DIR)/mem_handler.elf: $(addprefix $(BUILD_DIR)/, $(MEM_OBJS))
	$(LD_BSD) $(LD_BSDFLAGS) $^ $(LIBS_BSD) -o $@

$(BUILD_DIR)/software.elf: $(addprefix $(BUILD_DIR)/, $(SOFTWARE_OBJS))
	$(LD_BSD) $(LD_BSDFLAGS) $^ $(LIBS_BSD) -o $@

$(BUILD_DIR)/xhci_stub.elf: $(addprefix $(BUILD_DIR)/, $(XHCI_STUB_OBJS))
	$(LD_BSD) $(LD_BSDFLAGS) $^ $(LIBS_BSD) -o $@

$(BUILD_DIR)/hardware.elf: $(addprefix $(BUILD_DIR)/, $(HARDWARE_OBJS))
	$(LD_BSD) $(LD_BSDFLAGS) $^ $(LIBS_BSD) -o $@