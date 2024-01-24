/* This work is Crown Copyright NCSC, 2023. */
#include <microkit.h>
#include <pdprint.h>

#include <machine/bus_funcs.h>

#include <dev/usb/xhcivar.h>
#include <dev/usb/umassvar.h>

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
#include <shared_ringbuffer.h>
#include <dev/scsipi/scsiconf.h>
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
#include <libfdt.h>
#include <xhci_api.h>

#include <lib/libkern/libkern.h>
#include <dev/fdt/fdtvar.h>

#define BUS_DEBUG 0
#define __AARCH64__

//extern variables
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
uintptr_t device_bulk_pointer;
uintptr_t device_bulk_pointer_other;
uintptr_t umass_bbb_methods_pointer;
uintptr_t umass_bbb_methods_pointer_other;

//API shared data
uintptr_t kbd_free;
uintptr_t kbd_used;
uintptr_t mse_free;
uintptr_t mse_used;
uintptr_t uts_free;
uintptr_t uts_used;
uintptr_t umass_req_free;
uintptr_t umass_req_used;
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
ring_handle_t *umass_buffer_ring;
ring_handle_t *usb_new_device_ring;

// reset number of devices
int num_devices = 0;

void
init(void) {
    uintptr_t fdt = microkit_msginfo_get_label(microkit_ppcall(44, seL4_MessageInfo_new(0,0,0,0)));
    if (fdt_magic(fdt) != 0xd00dfeed){
        print_fatal("fdt magic failed\n");
        return;
    }

    uint64_t shared_heap_size = 0x200000;
    uint64_t ta_limit = shared_heap + shared_heap_size;
    int ta_blocks = 2048;
    int ta_thresh = 16;
    int ta_align = 64;
    bool status = ta_init((void*)shared_heap, (void*)ta_limit, ta_blocks, ta_thresh, ta_align);

    // init
    xhci_bus_methods_ptr = (struct usbd_bus_methods *) get_bus_methods();
    xhci_root_intr_pointer = (uintptr_t) get_root_intr_methods();
    device_ctrl_pointer = (uintptr_t) get_device_methods();
    microkit_msginfo mkaddr = microkit_ppcall(1, seL4_MessageInfo_new((uint64_t) xhci_root_intr_pointer,1,0,0));
    xhci_root_intr_pointer_other = microkit_msginfo_get_label(mkaddr);
    mkaddr = microkit_ppcall(3, seL4_MessageInfo_new((uint64_t) device_ctrl_pointer,1,0,0));
    device_ctrl_pointer_other = (uintptr_t) microkit_msginfo_get_label(mkaddr);

    umass_bbb_methods_pointer = (uintptr_t) get_umass_bbb_methods();
    mkaddr = microkit_ppcall(6, seL4_MessageInfo_new((uint64_t) umass_bbb_methods_pointer,1,0,0));
    umass_bbb_methods_pointer_other = microkit_msginfo_get_label(mkaddr);

    device_bulk_pointer = (uintptr_t) get_device_bulk_methods();
    mkaddr = microkit_ppcall(9, seL4_MessageInfo_new((uint64_t) device_bulk_pointer,1,0,0));
    device_bulk_pointer_other = (uintptr_t) microkit_msginfo_get_label(mkaddr);
    
    device_intr_pointer = (uintptr_t) get_device_intr_methods();
    mkaddr = microkit_ppcall(4, seL4_MessageInfo_new((uint64_t) device_intr_pointer,1,0,0));
    device_intr_pointer_other = (uintptr_t) microkit_msginfo_get_label(mkaddr);

    mkaddr = microkit_ppcall(8, seL4_MessageInfo_new(0,0,0,0));
    intr_ptrs = (struct intr_ptrs_holder *) microkit_msginfo_get_label(mkaddr);
    fdtbus_init(fdt);

    //initialise autoconf data structures
    config_init(); 
    initialise_and_start_timer(timer_base);
    sel4_dma_init(dma_cp_paddr, dma_base, dma_base + 0x200000);

    pipe_thread = false;
    cold = 0;

    // initialise structures. Communicate with software interrupt pd to get back all required data structures
    xhci_bus_methods_ptr            = (struct usbd_bus_methods *) get_bus_methods();
    intr_ptrs                       = (struct intr_ptrs_holder *) microkit_msginfo_get_label(microkit_ppcall(8, seL4_MessageInfo_new(0,0,0,0)));
    xhci_root_intr_pointer          = (uintptr_t) get_root_intr_methods();
    device_ctrl_pointer             = (uintptr_t) get_device_methods();
    device_intr_pointer             = (uintptr_t) get_device_intr_methods();
    xhci_root_intr_pointer_other    = (uintptr_t) microkit_msginfo_get_label(microkit_ppcall(1, seL4_MessageInfo_new((uint64_t) xhci_root_intr_pointer,1,0,0)));
    device_ctrl_pointer_other       = (uintptr_t) microkit_msginfo_get_label(microkit_ppcall(3, seL4_MessageInfo_new((uint64_t) device_ctrl_pointer,1,0,0)));
    device_intr_pointer_other       = (uintptr_t) microkit_msginfo_get_label(microkit_ppcall(4, seL4_MessageInfo_new((uint64_t) device_intr_pointer,1,0,0)));

	int offset = 0x6378; // DEBUG: set to -1 to actually traverse tree
    int startoffset = 0;
    int dwc3_phandle;
    bus_size_t addr, size;

    if (offset < 0) {
        print_debug("Traversing FDT to find dwc3 node at 0x%x...\n", xhci_base);
        for (offset = fdt_next_node(fdt, startoffset, NULL);
            offset >= 0;
            offset = fdt_next_node(fdt, offset, NULL)) {
            fdtbus_get_reg(fdtbus_offset2phandle(offset), 0, &addr, &size);
            if (addr == xhci_base) {
                dwc3_phandle = fdtbus_offset2phandle(offset);
                print_debug("offset: 0x%x\n", offset); // DEBUG: plug this into the offset value to speed up initialisation
                break;
            } else if (offset < 0) {
                if (offset == -FDT_ERR_NOTFOUND) {
                    print_fatal("no dwc3 node at 0x%x. Exiting.\n", xhci_base);
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
    microkit_ppcall(0, seL4_MessageInfo_new((uint64_t) sc_xhci,1,0,0));
    microkit_ppcall(2, seL4_MessageInfo_new((uint64_t) sc_xhci,1,0,0));
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

    // setup complete, busses will still need to be explored for devices to function
    print_info("Initialised\n");
    usb_discover(usb_sc2);
    usb_discover(usb_sc);
	microkit_notify(INIT); //notify client xhci is up and running
}

void handle_umass_xfer()
{
    uintptr_t *buffer = 0;
    unsigned int len = 0;
    void *cookie = NULL;

    int index;
    while (!driver_dequeue(umass_buffer_ring->used_ring, (uintptr_t**)&buffer, &len, &cookie)) {
        struct umass_request* xfer = (struct umass_request*)buffer; 

        if (xfer->read) {
            printf("calling read_block: n: %i    s: %i\n", xfer->nblks, xfer->blkno);
            read_block(xfer->umass_id, xfer->blkno, xfer->nblks, xfer->val);
        } else {
            printf("calling write_block: n: %i    s: %i\n", xfer->nblks, xfer->blkno);
            write_block(xfer->umass_id, xfer->blkno, xfer->nblks, xfer->val);
        }
    }
}

void handle_usb_new_device()
{
    // TODO
    printf("!!!!!!! new device");
}


void
notified(microkit_channel ch)
{
    switch (ch) {
        case 17: // hotplug
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
        case 47:
            handle_umass_xfer();
            break;
        case 50:
            handle_usb_new_device();
            break;
        default:
            print_warn("xhci_stub received notification unexpected channel\n");
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
            print_warn("xhci_stub received protected unexpected channel\n");
    }
    return seL4_MessageInfo_new(0,0,0,0);
}
