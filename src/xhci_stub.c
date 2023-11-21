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
#include <dev/usb/ukbd.h>

#include <timer_xhci.h>
#include <shared_ringbuffer.h>
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

#include <lib/libkern/libkern.h>
#include <dev/fdt/fdtvar.h>
#define BUS_DEBUG 0
#define __AARCH64__

bool int_once = false;
struct xhci_softc *glob_xhci_sc	= NULL;
struct usb_softc *glob_usb_sc 	= NULL;
struct usbd_bus_methods *xhci_bus_methods_ptr;
uintptr_t xhci_root_intr_pointer;
uintptr_t xhci_root_intr_pointer_other;
uintptr_t device_ctrl_pointer;
uintptr_t device_ctrl_pointer_other;
uintptr_t device_intr_pointer;
uintptr_t device_intr_pointer_other;
uintptr_t rx_free;
uintptr_t rx_used;
uintptr_t tx_free;
uintptr_t tx_used;
bool pipe_thread;

// struct usb_softc {
// 	struct usbd_bus *sc_bus;	/* USB controller */
// 	struct usbd_port sc_port;	/* dummy port for root hub */

// 	struct lwp	*sc_event_thread;
// 	struct lwp	*sc_attach_thread;

// 	char		sc_dying;
// 	bool		sc_pmf_registered;
// };

struct imx8mq_usbphy_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_bst;
	bus_space_handle_t	sc_bsh;
	int			sc_phandle;
};

// definitions from .system file
uintptr_t xhci_base;
uintptr_t xhci_phy_base;
uintptr_t heap_base;
uint64_t heap_size = 0x2000000;
int ta_blocks = 256;
int ta_thresh = 16;
int ta_align = 64;
uintptr_t pipe_heap_base;
uintptr_t dma_base;
uintptr_t dma_cp_paddr;
uintptr_t dma_cp_vaddr = 0x54000000;
uintptr_t ta_limit;
uintptr_t timer_base;
uintptr_t software_heap;

/* Pointers to shared_ringbuffers */
ring_handle_t *kbd_buffer_ring;

ring_handle_t *tx_ring = 0;

// TODO: put these in a header file so can change it in a single place for a platform

int phy_setup() {
    struct imx8mq_usbphy_softc *sc_usbphy;
	sc_usbphy = kmem_alloc(sizeof(*sc_usbphy), 0);
    device_t parent_usbphy = NULL;
    device_t self_usbphy = kmem_alloc(sizeof(device_t), 0);
    void *aux_usbphy = kmem_alloc(sizeof(struct fdt_attach_args), 0);
	sc_usbphy->sc_bsh = 0x382f0040;
	sc_usbphy->sc_bst = kmem_alloc(sizeof(bus_space_tag_t), 0);
	self_usbphy->dv_private = sc_usbphy;
	imx8mq_usbphy_attach(parent_usbphy, self_usbphy,aux_usbphy);

    imx8mq_usbphy_enable(self_usbphy, NULL, true); //imx8 doesn't need priv 
    return 0;
}

void
init(void) {
    if (BUS_DEBUG) {
        uint32_t read_offset    = 0xc120;
        // uint32_t write_offset   = 0xc2c0;

        microkit_dbg_puts("Starting read and write tests\n");
        /* read test */
        printf("xhci_base: %p\n", xhci_base);
        uint32_t response;
        response  = bus_space_read_1(0, xhci_base, read_offset);
        printf("Attempted bus_space_read_1: %p\n", response);
        response  = bus_space_read_4(0, xhci_base, 0xc120);
        printf("Attempted bus_space_read_4: %p\n", response);
    }

    pipe_thread = false;
    // init
    xhci_root_intr_pointer = get_root_intr_methods();
    xhci_bus_methods_ptr = get_bus_methods();
    device_ctrl_pointer = get_device_methods();
    microkit_msginfo addr = microkit_ppcall(1, seL4_MessageInfo_new((uint64_t) xhci_root_intr_pointer,1,0,0));
    xhci_root_intr_pointer_other = microkit_msginfo_get_label(addr);
    device_ctrl_pointer = get_device_methods();
    addr = microkit_ppcall(3, seL4_MessageInfo_new((uint64_t) device_ctrl_pointer,1,0,0));
    device_ctrl_pointer_other = microkit_msginfo_get_label(addr);
    device_intr_pointer = get_device_intr_methods();
    addr = microkit_ppcall(4, seL4_MessageInfo_new((uint64_t) device_intr_pointer,1,0,0));
    device_intr_pointer_other = microkit_msginfo_get_label(addr);
    /* memcpy(&xhci_root_intr_pointer, get_root_intr_methods(), sizeof(struct usbd_pipe_methods)); */
    /* printf("xhci_stub received root_intr ptr %p\n", xhci_root_intr_pointer); */

    initialise_and_start_timer(timer_base);

    sel4_dma_init(dma_cp_paddr, dma_cp_vaddr, dma_cp_vaddr + 0x200000);

    device_t parent_xhci = NULL;
    kbd_buffer_ring = kmem_alloc(sizeof(*kbd_buffer_ring), 0);

    phy_setup();
    device_t self_xhci = kmem_alloc(sizeof(device_t), 0);
    void *aux_xhci = kmem_alloc(sizeof(struct fdt_attach_args), 0);

    struct xhci_softc *sc_xhci = kmem_alloc(sizeof(struct xhci_softc), 0);
    glob_xhci_sc = sc_xhci;
    sc_xhci->sc_ioh=0x38200000;
    microkit_ppcall(0, seL4_MessageInfo_new((uint64_t) sc_xhci,1,0,0));
    microkit_ppcall(2, seL4_MessageInfo_new((uint64_t) sc_xhci,1,0,0));
	bus_space_tag_t iot = kmem_alloc(sizeof(bus_space_tag_t), 0);
    sc_xhci->sc_iot=iot;

    self_xhci->dv_private = sc_xhci;

    dwc3_fdt_attach(parent_xhci,self_xhci,aux_xhci);

    struct usb_softc *usb_sc = kmem_alloc(sizeof(struct usb_softc),0);
    struct usbd_bus *sc_bus = kmem_alloc(sizeof(struct usbd_bus),0);
    device_t self = kmem_alloc(sizeof(device_t), 0);
    *sc_bus = glob_xhci_sc->sc_bus;
    sc_bus->ub_methods = glob_xhci_sc->sc_bus.ub_methods;
    self->dv_unit = 1;
    self->dv_private = usb_sc;
    device_t parent = NULL;
    usb_attach(parent, self, sc_bus);
	usb_sc->sc_bus->ub_needsexplore = 1;
    usb_discover(usb_sc);
    printf("Setup finished, type on keyboard to begin\n");
}


void
notified(microkit_channel ch)
{
    switch (ch) {
        default:
            printf("XHCI_STUB: unexpected channel\n");
            break;
    }
}

microkit_msginfo
protected(microkit_channel ch, microkit_msginfo msginfo) {
    switch (ch) {
        case 1:
            // return addr of root_intr_methods
            xhci_root_intr_pointer = (uintptr_t) microkit_msginfo_get_label(msginfo);
            break;
        default:
            printf("XHCI_STUB: received protected unexpected channel\n");
    }
    return seL4_MessageInfo_new(0,0,0,0);
}
