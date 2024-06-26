/* This work is Crown Copyright NCSC, 2023. */
#include <microkit.h>
#include <pdprint.h>

#include <machine/bus_funcs.h>


#include <wrapper.h>
#include <dma.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/device_impl.h>
#include <sys/intr.h>
#include <sys/kernel.h>
#include <sys/kmem.h>

#include <timer.h>
#include <shared_ringbuffer.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbhist.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/xhcireg.h>
#include <dev/usb/xhcivar.h>
#include <dev/usb/umassvar.h>
#include <sys/device.h>
#include <machine/types.h>
#include <sel4_bus_funcs.h>
#include <libfdt.h>
#include <xhci_api.h>

#include <dev/fdt/fdtvar.h>
#include <stdio.h>

// channels
#define SOFTC_SHARE_HARD 0
#define SOFTC_SHARE_SOFT 1
#define INTR_SHARE 2
#define HOTPLUG 3

// Setup for getting printf functionality working {{{
static int
libc_microkit_putc(char c, FILE *file)
{
    (void) file; /* Not used by us */
    microkit_dbg_putc(c);
    return c;
}

static int
sample_getc(FILE *file)
{
	return -1; /* getc not implemented, return EOF */
}
static FILE __stdio = FDEV_SETUP_STREAM(libc_microkit_putc,
                    sample_getc,
                    NULL,
                    _FDEV_SETUP_WRITE);
FILE *const stdin = &__stdio; __strong_reference(stdin, stdout); __strong_reference(stdin, stderr);
// END OF LIBC
// }}}

#define BUS_DEBUG 0
#define __AARCH64__

//extern variables
bool int_once = false;
struct xhci_softc *glob_xhci_sc	= NULL;
struct usb_softc *glob_usb_sc 	= NULL;
struct usbd_bus_methods *xhci_bus_methods_ptr;

//API shared data
uintptr_t kbd_free;
uintptr_t kbd_used;
uintptr_t mse_free;
uintptr_t mse_used;
uintptr_t uts_free;
uintptr_t uts_used;
uintptr_t umass_resp;
uintptr_t umass_req;
uintptr_t usb_new_device_free;
uintptr_t usb_new_device_used;

// shared memory between client and driver
uintptr_t shared_mem;
uintptr_t shared_heap;

struct intr_ptrs_holder *intr_ptrs;
bool pipe_thread;
int cold = 1;

char *pd_name = "xhci_stub";

// definitions from .system file
uintptr_t xhci_base;
uintptr_t xhci_phy_base;
uintptr_t heap_base;
uintptr_t dma_base;
uintptr_t dma_cp_paddr;
uintptr_t timer_base;

struct usb_softc *usb_sc, *usb_sc2;

/* Pointers to shared_ringbuffers */
ring_handle_t *kbd_buffer_ring;
ring_handle_t *mse_buffer_ring;
ring_handle_t *uts_buffer_ring;
blk_queue_handle_t *umass_buffer_ring;
ring_handle_t *usb_new_device_ring;

// reset number of devices
int num_devices = 0;

struct umass_request *active_xfer;

void
init(void) {
    uintptr_t fdt = microkit_msginfo_get_label(microkit_ppcall(44, seL4_MessageInfo_new(0,0,0,0)));
    if (fdt_magic(fdt) != 0xd00dfeed){
        print_fatal("fdt magic failed\n");
        return;
    }

    fdtbus_init((void*)fdt);

    //initialise autoconf data structures
    config_init(); 
    initialise_and_start_timer(timer_base);
    sel4_dma_init(dma_cp_paddr, dma_base, dma_base + 0x200000);

    pipe_thread = false;
    cold = 0;

    // initialise structures. Communicate with software interrupt pd to get back all required data structures
    xhci_bus_methods_ptr            = (struct usbd_bus_methods *) get_bus_methods();
    intr_ptrs                       = (struct intr_ptrs_holder *) microkit_msginfo_get_label(microkit_ppcall(INTR_SHARE, seL4_MessageInfo_new(0,0,0,0)));

	int offset = 0x6378; // DEBUG: set to -1 to actually traverse tree
    int startoffset = 0;
    int dwc3_phandle;
    bus_size_t addr, size;

    if (offset < 0) {
        print_info("Traversing FDT to find dwc3 node at 0x%x...\n", xhci_base);
        for (offset = fdt_next_node((void*)fdt, startoffset, NULL);
            offset >= 0;
            offset = fdt_next_node((void*)fdt, offset, NULL)) {
            fdtbus_get_reg(fdtbus_offset2phandle(offset), 0, &addr, &size);
            if (addr == xhci_base) {
                dwc3_phandle = fdtbus_offset2phandle(offset);
                print_info("offset: 0x%x\n", offset); // DEBUG: plug this into the offset value to speed up initialisation
                break;
            } else if (offset < 0) {
                if (offset == -FDT_ERR_NOTFOUND) {
                    print_fatal("no dwc3 node at 0x%lx. Exiting.\n", xhci_base);
                    return;
                }
            }
        }
    } else {
        print_warn("using hardcoded dwc3 node\n");
        dwc3_phandle = fdtbus_offset2phandle(offset);
        fdtbus_get_reg(dwc3_phandle, 0, &addr, &size);
    }

    // setup api rings
    kbd_buffer_ring = kmem_alloc(sizeof(*kbd_buffer_ring), 0);
    mse_buffer_ring = kmem_alloc(sizeof(*mse_buffer_ring), 0);
    uts_buffer_ring = kmem_alloc(sizeof(*uts_buffer_ring), 0);

    /* Set up shared memory regions */
    usb_new_device_ring = kmem_alloc(sizeof(*usb_new_device_ring), 0);
    ring_init(usb_new_device_ring, (ring_buffer_t *)usb_new_device_free, (ring_buffer_t *)usb_new_device_used, NULL, 1);


    // setup xhci devices + tell software PD memory locations
    device_t parent_xhci = NULL;

    device_t self_xhci = kmem_alloc(sizeof(device_t), 0);
    struct fdt_attach_args *aux_xhci = kmem_alloc(sizeof(struct fdt_attach_args), 0);

    aux_xhci->faa_phandle = dwc3_phandle;

    struct xhci_softc *sc_xhci = kmem_alloc(sizeof(struct xhci_softc), 0);
    glob_xhci_sc = sc_xhci;
    sc_xhci->sc_ioh=xhci_base;
    microkit_ppcall(SOFTC_SHARE_HARD, seL4_MessageInfo_new((uint64_t) sc_xhci,1,0,0));
    microkit_ppcall(SOFTC_SHARE_SOFT, seL4_MessageInfo_new((uint64_t) sc_xhci,1,0,0));
	bus_space_tag_t iot = kmem_alloc(sizeof(bus_space_tag_t), 0);
    sc_xhci->sc_iot=iot;

    self_xhci->dv_private = sc_xhci;

    dwc3_fdt_attach(parent_xhci,self_xhci,aux_xhci);
    // attach USB3 bus
    usb_sc              = kmem_alloc(sizeof(struct usb_softc),0);
    device_t self       = kmem_alloc(sizeof(device_t), 0);
    self->dv_unit       = 1;
    self->dv_private    = usb_sc;
    usb_sc->sc_bus      = &glob_xhci_sc->sc_bus;
    usb_attach(self_xhci, self, &glob_xhci_sc->sc_bus);

    // attach USB2 bus
    struct usbd_bus *sc_bus2;
    usb_sc2               = kmem_alloc(sizeof(struct usb_softc),0);
    sc_bus2               = kmem_alloc(sizeof(struct usbd_bus),0)
    device_t self2        = kmem_alloc(sizeof(device_t), 0);
    *sc_bus2              = glob_xhci_sc->sc_bus2;
    sc_bus2->ub_methods   = glob_xhci_sc->sc_bus2.ub_methods;
    self2->dv_private     = usb_sc2;
    self2->dv_unit        = 1;
    usb_sc2->sc_bus       = sc_bus2;
    usb_attach(self_xhci, self2, sc_bus2);

    // assert initial explore
	usb_sc->sc_bus->ub_needsexplore    = 1;
	usb_sc2->sc_bus->ub_needsexplore   = 1;

    // setup complete, buses will still need to be explored for devices to function
    usb_discover(usb_sc2);
    usb_discover(usb_sc);
    print_info("Initialised\n");
	microkit_notify(INIT); //notify client xhci is up and running
}

#define HEXDUMP(a, b, c) \
    do { \
		hexdump(printf, a, b, c); \
    } while (/*CONSTCOND*/0)

void create_umass_xfer()
{
    uintptr_t buffer;
    int blkno, id;
    uint16_t nblks;
    unsigned int len = 0;

    int index;
    blk_request_code_t code;

    blk_dequeue_req(umass_buffer_ring, &code, &buffer, &blkno, &nblks, &id);
    if (code == READ_BLOCKS) {
        read_block(0, blkno, nblks, (void*)buffer);
    } else if (code == WRITE_BLOCKS) {
        write_block(0, blkno, nblks, (void*)buffer);
    } else if (code == FLUSH) { //for async IO
        blk_enqueue_resp(umass_buffer_ring, 1, buffer, nblks, 1, id);
        microkit_notify(47);
    } else {
        printf("unrecognised code = %d\n", code);
    }
}

void
notified(microkit_channel ch)
{
    switch (ch) {
        case HOTPLUG: // hotplug
            // do a discover
            if (usb_sc->sc_bus->ub_needsexplore || usb_sc2->sc_bus->ub_needsexplore) {
                printf("Discover on USB3...\n");
                usb_discover(usb_sc);
                printf("USB3 discover finished\n");
            }
            if (usb_sc2->sc_bus->ub_needsexplore) {
                print_debug("Discover on USB2...\n");
                usb_discover(usb_sc2);
                print_debug("USB2 discover finished\n");
            }
            break;
        case UMASS_FLUSH:
            create_umass_xfer();
            break;
        case UMASS_COMPLETE:
            umass_buffer_ring->req_queue->in_progress = false; //release lock
            break;
        default:
            print_warn("xhci_stub received notification unexpected channel %d\n", ch);
    }
}

microkit_msginfo
protected(microkit_channel ch, microkit_msginfo msginfo) {
    switch (ch) {
        default:
            print_warn("xhci_stub received protected unexpected channel\n");
    }
    return seL4_MessageInfo_new(0,0,0,0);
}
