/* This work is Crown Copyright NCSC, 2023. */
#include <microkit.h>
#include <pdprint.h>

#include <machine/bus_funcs.h>

#include <dev/usb/xhcivar.h>

#include <wrapper.h>
#include <tinyalloc.h>
#include <dma.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/device_impl.h>
#include <sys/intr.h>
#include <sys/kernel.h>
#include <sys/kmem.h>

#include <timer.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbhist.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/xhcireg.h>
#include <dev/usb/xhcivar.h>
#include <sys/device.h>
#include <machine/types.h>
#include <sel4_bus_funcs.h>

#include <dev/fdt/fdtvar.h>

/* #define INTR_DEBUG */

char *pd_name = "hardware_interrupts";

uintptr_t xhci_base;
uintptr_t dma_base;
uintptr_t heap_base;
uintptr_t pipe_heap_base;
uintptr_t dma_cp_vaddr = 0x54000000;
uintptr_t dma_cp_paddr;
uintptr_t timer_base;

struct xhci_softc *glob_xhci_sc	= NULL;
struct usbd_bus_methods *xhci_bus_methods_ptr;
uintptr_t xhci_root_intr_pointer;
uintptr_t xhci_root_intr_pointer_other;
uintptr_t device_ctrl_pointer;
uintptr_t device_ctrl_pointer_other;
bool pipe_thread;

uint32_t xhci_read_print_4(bus_space_tag_t tag, bus_space_handle_t bsh, bus_size_t size){
    uint32_t busval = bus_space_read_4(tag, bsh, size);
#ifdef INTR_DEBUG
    print_debug("xhci: Read4: Handle: %lx, Offset: %lx. Result: %08x\n", bsh, size, busval);
#endif
    return busval;
}

void xhci_write_print_4(bus_space_tag_t tag, bus_space_handle_t bsh, bus_size_t size, uint32_t val){
#ifdef INTR_DEBUG
    print_debug("xhci: Wrte4: Handle: %lx, Offset: %lx.  Value: %08x\n", bsh, size, val);
#endif
    bus_space_write_4(tag, bsh, size, val);
}

static inline uint32_t
xhci_op_read_4(const struct xhci_softc * const sc, bus_size_t offset)
{
	return xhci_read_print_4(sc->sc_iot, sc->sc_obh, offset);
}

static inline void
xhci_op_write_4(const struct xhci_softc * const sc, bus_size_t offset,
    uint32_t value)
{
	xhci_write_print_4(sc->sc_iot, sc->sc_obh, offset, value);
}


static inline uint32_t
xhci_rt_read_4(const struct xhci_softc * const sc, bus_size_t offset)
{
	return xhci_read_print_4(sc->sc_iot, sc->sc_rbh, offset);
}

static inline void
xhci_rt_write_4(const struct xhci_softc * const sc, bus_size_t offset,
    uint32_t value)
{
	xhci_write_print_4(sc->sc_iot, sc->sc_rbh, offset, value);
}

int
xhci_intr1(struct xhci_softc * const sc)
{
	uint32_t usbsts;
	uint32_t iman;


	usbsts = xhci_op_read_4(sc, XHCI_USBSTS);
#ifdef INTR_DEBUG
	print_debug("USBSTS 0x%08x\n", usbsts);
#endif

	if ((usbsts & (XHCI_STS_HSE | XHCI_STS_EINT | XHCI_STS_PCD |
	    XHCI_STS_HCE)) == 0) {
		print_info("ignored intr not for this device\n");

		return 0;
	}

	/*
	 * Clear EINT and other transient flags, to not misenterpret
	 * next shared interrupt. Also, to avoid race, EINT must be cleared
	 * before XHCI_IMAN_INTR_PEND is cleared.
	 */
	xhci_op_write_4(sc, XHCI_USBSTS, usbsts & XHCI_STS_RSVDP0);

#ifdef INTR_DEBUG
	usbsts = xhci_op_read_4(sc, XHCI_USBSTS);
	print_debug("USBSTS 0x%08x\n", usbsts);
#endif

	iman = xhci_rt_read_4(sc, XHCI_IMAN(0));
#ifdef INTR_DEBUG
	print_debug("IMAN0 0x%08x\n", iman);
#endif
	iman |= XHCI_IMAN_INTR_PEND;
	xhci_rt_write_4(sc, XHCI_IMAN(0), iman);

	return 1;
}

int
xhci_intr(void *v)
{
	struct xhci_softc * const sc = v;
	int ret = 0;
	if (sc == NULL)
		return 0;
	mutex_spin_enter(&sc->sc_intr_lock);

    if (sc->sc_dying) {
        goto done;
	}
	ret = xhci_intr1(sc);
	if (ret) {
		microkit_notify(7); 
	}
done:
	mutex_spin_exit(&sc->sc_intr_lock);
	return ret;
}


void
init(void) {
    device_ctrl_pointer = 0;
    device_ctrl_pointer_other = 0;
	initialise_and_start_timer(timer_base);
    print_info("Initialised\n");
}

void
notified(microkit_channel ch) {
    switch (ch) {
        case 6:
            if (glob_xhci_sc != NULL) {
                xhci_intr(glob_xhci_sc);
            } else {
                print_fatal("sc not defined\n");
            }
            microkit_irq_ack(ch);
            break;
            while (1) {
                
            }
    }
}

microkit_msginfo
protected(microkit_channel ch, microkit_msginfo msginfo) {
    switch (ch) {
        case 0:
            glob_xhci_sc = (struct xhci_softc *) microkit_msginfo_get_label(msginfo);
            return seL4_MessageInfo_new(0,0,0,0);
            break;
        case 1:
            xhci_root_intr_pointer_other = microkit_msginfo_get_label(msginfo);
            print_debug("sending xhci_root_intr_pointer: %p\n", xhci_root_intr_pointer);
            return seL4_MessageInfo_new((uint64_t) xhci_root_intr_pointer, 1, 0, 0);
        default:
            print_debug("Unexpected channel hardware int %d\n", ch);
    }
}
