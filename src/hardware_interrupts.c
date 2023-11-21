#include <microkit.h>
#include <printf.h>

#include <evbarm/bus_funcs.h>

#include <dev/usb/xhcivar.h>

#include <wrapper.h>
#include <tinyalloc.h>
#include <dma.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/device_impl.h>
#include <sys/intr.h>
// #include <sys/systm.h>
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
#include <evbarm/types.h>
#include <sel4_bus_funcs.h>

#include <dev/fdt/fdtvar.h>

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

#ifndef SEL4_USB_DEBUG
#define print
#else
#define print(...) printf(__VA_ARGS__)
#endif

uint32_t xhci_read_print_4(bus_space_tag_t tag, bus_space_handle_t bsh, bus_size_t size){
    uint32_t busval = bus_space_read_4(tag, bsh, size);
    // printf("xhci: Read4: Handle: %lx, Offset: %lx. Result: %08x\n", bsh, size, busval);
    return busval;
}

void xhci_write_print_4(bus_space_tag_t tag, bus_space_handle_t bsh, bus_size_t size, uint32_t val){
    // printf("xhci: Wrte4: Handle: %lx, Offset: %lx.  Value: %08x\n", bsh, size, val);
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

	/* XHCIHIST_FUNC(); */

	usbsts = xhci_op_read_4(sc, XHCI_USBSTS);
	/* XHCIHIST_CALLARGS("USBSTS 0x%08jx", usbsts, 0, 0, 0); */
	if ((usbsts & (XHCI_STS_HSE | XHCI_STS_EINT | XHCI_STS_PCD |
	    XHCI_STS_HCE)) == 0) {
		printf(16, "ignored intr not for this device",
		    0, 0, 0, 0);

		return 0;
	}

	/*
	 * Clear EINT and other transient flags, to not misenterpret
	 * next shared interrupt. Also, to avoid race, EINT must be cleared
	 * before XHCI_IMAN_INTR_PEND is cleared.
	 */
	xhci_op_write_4(sc, XHCI_USBSTS, usbsts & XHCI_STS_RSVDP0);

#ifdef XHCI_DEBUG
	/* usbsts = xhci_op_read_4(sc, XHCI_USBSTS); */
	/* DPRINTFN(16, "USBSTS 0x%08jx", usbsts, 0, 0, 0); */
#endif

	iman = xhci_rt_read_4(sc, XHCI_IMAN(0));
	/* DPRINTFN(16, "IMAN0 0x%08jx", iman, 0, 0, 0); */
	iman |= XHCI_IMAN_INTR_PEND;
	xhci_rt_write_4(sc, XHCI_IMAN(0), iman);

#ifdef XHCI_DEBUG
	/* iman = xhci_rt_read_4(sc, XHCI_IMAN(0)); */
	/* DPRINTFN(16, "IMAN0 0x%08jx", iman, 0, 0, 0); */
	/* usbsts = xhci_op_read_4(sc, XHCI_USBSTS); */
	/* DPRINTFN(16, "USBSTS 0x%08jx", usbsts, 0, 0, 0); */
#endif

	return 1;
}

xhci_intr(void *v)
{
	struct xhci_softc * const sc = v;
	int ret = 0;

	if (sc == NULL)
		return 0;

	mutex_spin_enter(&sc->sc_intr_lock);

    if (sc->sc_dying)
        goto done;

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
    printf("root_intr_ptr = %p\n", xhci_root_intr_pointer);
    initialise_and_start_timer(timer_base);
    printf("Hardware up and running\n");
}

void
notified(microkit_channel ch) {
    switch (ch) {
        case 6:
            // printf("!!xhci hard interrupt!!\n");
            if (glob_xhci_sc != NULL) {
                xhci_intr(glob_xhci_sc);
            } else {
                printf("FATAL: sc not defined");
            }
            // printf("end of ch\n");
            microkit_irq_ack(ch);
            break;
            while (1) {
                
            }
    }
    // printf("Hanging to prevent io spam\n");
    // while(1) {

    // }
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
            print("sending xhci_root_intr_pointer: %p\n", xhci_root_intr_pointer);
            return seL4_MessageInfo_new((uint64_t) xhci_root_intr_pointer, 1, 0, 0);
        default:
            printf("Unexpected channel hardware int %d\n", ch);
    }
}
