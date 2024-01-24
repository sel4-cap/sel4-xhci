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
#include <dev/usb/usb.h>
#include <dev/usb/usb_quirks.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbhist.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/xhcireg.h>
#include <dev/usb/xhcivar.h>
#include <sys/device.h>
#include <machine/types.h>
#include <sel4_bus_funcs.h>

#include <dev/fdt/fdtvar.h>
#include <config_methods.h>
#include <xhci_api.h>

char* pd_name = "software_interrupts";
int num_devices = 0; //unused

uintptr_t xhci_base;
uintptr_t dma_base;
uintptr_t ring_base;
uintptr_t heap_base;
uintptr_t dma_cp_paddr;
uintptr_t timer_base;
uint64_t heap_size = 0x2000000;

//API shared mem
uintptr_t kbd_free;
uintptr_t kbd_used;
uintptr_t mse_free;
uintptr_t mse_used;
uintptr_t uts_free;
uintptr_t uts_used;
uintptr_t umass_req_free;
uintptr_t umass_req_used;
uintptr_t usb_new_device_free; // unused
uintptr_t usb_new_device_used; // unused

// shared memory between client and driver
uintptr_t shared_mem;

bool pipe_thread;

//extern variables
struct xhci_softc *glob_xhci_sc	= NULL;
struct usbd_bus_methods *xhci_bus_methods_ptr;
struct usbd_pipe_methods *xhci_root_intr_pointer;
struct usbd_pipe_methods *xhci_root_intr_pointer_other;
struct usbd_pipe_methods *device_ctrl_pointer;
struct usbd_pipe_methods *device_ctrl_pointer_other;
struct usbd_pipe_methods *device_intr_pointer;
struct usbd_pipe_methods *device_intr_pointer_other;
struct intr_ptrs_holder *intr_ptrs;
struct umass_wire_methods *umass_bbb_methods_pointer;
struct umass_wire_methods *umass_bbb_methods_pointer_other;
struct usbd_pipe_methods *device_bulk_pointer;
struct usbd_pipe_methods *device_bulk_pointer_other;
int cold = 1;

// shared heap base with client
uintptr_t shared_heap;
uintptr_t shared_soft_heap;

/* Pointers to shared_ringbuffers */
ring_handle_t *kbd_buffer_ring;
ring_handle_t *mse_buffer_ring;
ring_handle_t *uts_buffer_ring;
ring_handle_t *umass_buffer_ring; // unused
ring_handle_t *usb_new_device_ring; // unused

void
init(void) {

    uint64_t shared_heap_size = 0x200000;
    uint64_t ta_limit = shared_soft_heap + shared_heap_size;
    int ta_blocks = 2048;
    int ta_thresh = 16;
    int ta_align = 64;
    bool status = ta_init((void*)shared_soft_heap, (void*)ta_limit, ta_blocks, ta_thresh, ta_align);

    config_init();
    cold = 0;
    xhci_bus_methods_ptr = get_bus_methods();
    xhci_root_intr_pointer = get_root_intr_methods();
    device_ctrl_pointer = get_device_methods();
    device_intr_pointer = get_device_intr_methods();
    umass_bbb_methods_pointer = get_umass_bbb_methods();
    device_bulk_pointer = get_device_bulk_methods();
    pipe_thread = false;
    sel4_dma_init(dma_cp_paddr, dma_base, dma_base + 0x200000);
    initialise_and_start_timer(timer_base);

    kbd_buffer_ring = kmem_alloc(sizeof(*kbd_buffer_ring), 0);
    ring_init(kbd_buffer_ring, (ring_buffer_t *)kbd_free, (ring_buffer_t *)kbd_used, NULL, 0);
    mse_buffer_ring = kmem_alloc(sizeof(*mse_buffer_ring), 0);
    ring_init(mse_buffer_ring, (ring_buffer_t *)mse_free, (ring_buffer_t *)mse_used, NULL, 0);
    uts_buffer_ring = kmem_alloc(sizeof(*uts_buffer_ring), 0);
    ring_init(uts_buffer_ring, (ring_buffer_t *)uts_free, (ring_buffer_t *)uts_used, NULL, 0);
    umass_buffer_ring = kmem_alloc(sizeof(*umass_buffer_ring),0);
    ring_init(umass_buffer_ring, (ring_buffer_t *)umass_req_free, (ring_buffer_t *)umass_req_used, NULL, 0);
    print_info("Initialised\n");
}

void
notified(microkit_channel ch) {
    switch (ch) {
        case 7:
            if (glob_xhci_sc != NULL) {
                xhci_softintr(&glob_xhci_sc->sc_bus);
                microkit_notify(17); //discover call
            } else {
                print_fatal("sc not defined");
            }
            break;
    }
}

microkit_msginfo
protected(microkit_channel ch, microkit_msginfo msginfo) {
    struct set_cfg *cfg;
    switch (ch) {
        case 1:
            xhci_root_intr_pointer_other = (struct usbd_pipe_methods *) microkit_msginfo_get_label(msginfo);
            print_debug("sending xhci_root_intr_pointer: %p\n", xhci_root_intr_pointer);
            return seL4_MessageInfo_new((uint64_t) xhci_root_intr_pointer, 1, 0, 0);
            break;
        case 2:
            glob_xhci_sc = (struct xhci_softc *) microkit_msginfo_get_label(msginfo);
            break;
        case 3:
            device_ctrl_pointer_other = (struct usbd_pipe_methods *) microkit_msginfo_get_label(msginfo);
            print_debug("sending device_ctrl_pointer: %p\n", device_ctrl_pointer);
            return seL4_MessageInfo_new((uint64_t) device_ctrl_pointer, 1, 0, 0);
        case 4:
            device_intr_pointer_other = (struct usbd_pipe_methods *) microkit_msginfo_get_label(msginfo);
            print_debug("sending device_intr_pointer: %p\n", device_intr_pointer);
            return seL4_MessageInfo_new((uint64_t) device_intr_pointer, 1, 0, 0);
        case 5:
            usbd_delay_ms(0, 100);
            print_debug("doing set_config_index in softintr\n");
            cfg = (struct set_cfg*) microkit_msginfo_get_label(msginfo);
            cfg->dev->ud_quirks = get_quirks(); //assume no quirks
            print_debug("config dev = %p\n", cfg->dev);
            usbd_status err = usbd_set_config_index(cfg->dev, cfg->confi, cfg->msg);
            print_debug("reached end of set_conf_index\n");
            return seL4_MessageInfo_new(err,1,0,0);
        case 6:
            umass_bbb_methods_pointer_other = (struct umass_wire_methods *) microkit_msginfo_get_label(msginfo);
            print_debug("sending umass_bbb_methods_pointer: %p\n", umass_bbb_methods_pointer);
            return seL4_MessageInfo_new((uint64_t) umass_bbb_methods_pointer, 1, 0, 0);
        case 8:
            // pass interrupt structures so callback can be used without hardcoding
            intr_ptrs = kmem_alloc(sizeof(struct intr_ptrs_holder), 0);
            intr_ptrs->ukbd             = &ukbd_intr;
            intr_ptrs->ums              = &ums_intr;
            intr_ptrs->uts              = &uts_intr;
            intr_ptrs->uhidev           = &uhidev_intr;
            intr_ptrs->uhub             = &uhub_intr;
            intr_ptrs->uhid             = &uhid_intr;
            intr_ptrs->umass_wire_state = get_umass_wire_state();
            intr_ptrs->umass_scsipi_cb  = get_umass_scsipi_cb();
            intr_ptrs->umass_null_cb    = get_umass_null_cb();
            return seL4_MessageInfo_new((uint64_t) intr_ptrs, 1, 0, 0);
        case 9:
            device_bulk_pointer_other = (struct usbd_pipe_methods *) microkit_msginfo_get_label(msginfo);
            print_debug("sending device_bulk_pointer: %p\n", device_bulk_pointer);
            return seL4_MessageInfo_new((uint64_t) device_bulk_pointer, 1, 0, 0);
        default:
            print_warn("softintr unexpected channel %d\n", ch);
    }
    return seL4_MessageInfo_new(0,0,0,0);
}
