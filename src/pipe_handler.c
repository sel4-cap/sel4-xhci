#include <sel4cp.h>
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

#include <dev/fdt/fdtvar.h>
#include <pipe_methods.h>

uintptr_t xhci_base;
uintptr_t dma_base;
uintptr_t heap_base;
uintptr_t pipe_heap_base;
uintptr_t dma_cp_vaddr = 0x54000000;
uintptr_t dma_cp_paddr;
uintptr_t timer_base;
uintptr_t rx_free;
uintptr_t rx_used;
uintptr_t tx_free;
uintptr_t tx_used;

struct xhci_softc *glob_xhci_sc	= NULL;
uintptr_t xhci_root_intr_pointer;
struct usbd_bus_methods *xhci_bus_methods_ptr;
uintptr_t xhci_root_intr_pointer_other;
uintptr_t device_ctrl_pointer;
uintptr_t device_ctrl_pointer_other;

uint64_t heap_size = 0x2000000;
int ta_blocks = 256;
int ta_thresh = 16;
int ta_align = 64;
bool pipe_thread;

uintptr_t xhci_root_intr_pointer;
uintptr_t xhci_root_intr_pointer_other;
uintptr_t device_ctrl_pointer;
uintptr_t device_ctrl_pointer_other;
uintptr_t device_intr_pointer;
uintptr_t device_intr_pointer_other;

/* Pointers to shared_ringbuffers */
ring_handle_t *kbd_buffer_ring;
ring_handle_t *tx_ring = 0;

void
init(void) {
    printf("PIPE_HANDLER: dmapaddr = %p\n", dma_cp_paddr);
    /* xhci_root_intr_pointer = get_root_intr_methods(); */
    /* printf("root_intr_ptr = %p\n", xhci_root_intr_pointer); */
    xhci_root_intr_pointer = xhci_root_intr_pointer_other = device_ctrl_pointer = device_ctrl_pointer_other = device_intr_pointer = device_intr_pointer_other = 0;
    xhci_bus_methods_ptr = get_bus_methods();
    sel4_dma_init(dma_cp_paddr, dma_cp_vaddr, dma_cp_vaddr + 0x200000);
    initialise_and_start_timer(timer_base);
    device_ctrl_pointer = 0;
    device_ctrl_pointer_other = 0;
    pipe_thread = true;
    int ta_limit = pipe_heap_base + heap_size;
    printf("Pipe_handler up and running\n");
}

void
notified(sel4cp_channel ch) {
    printf("pipe_methods_thread called (bad)!\n");
}

sel4cp_msginfo
protected(sel4cp_channel ch, sel4cp_msginfo msginfo) {
    printf("Inside protected (pipe handler)\n");
    struct pipe_method_rpc *pmr;
    struct pipe_method_init *pmi;
    seL4_MessageInfo_t return_msg = seL4_MessageInfo_new(0,0,0,0);
    switch (ch) {
        case (PIPE_INIT_CHANNEL):
        case (PIPE_INIT_CHANNEL + 10):
            pmi = (struct pipe_method_init *) sel4cp_msginfo_get_label(msginfo);
            pmi->pipe->up_methods = get_up_methods(pmi->method_ptr);
            printf("set init to %p\n", pmi->pipe->up_methods);
            break;
        case (PIPE_METHOD_CHANNEL):
        case (PIPE_METHOD_CHANNEL+10):
            pmr = (struct pipe_method_rpc *) sel4cp_msginfo_get_label(msginfo);
            usbd_status err = 0;
            printf("pipe handler xfer = %p\n", pmr->xfer);
            switch (pmr->method_ptr) {
                case (TRANSFER):
                    err = pmr->pipe->up_methods->upm_transfer(pmr->xfer);
                    return_msg = seL4_MessageInfo_new(err, 1, 0, 0);
                    break;
                case (START):
                    err = pmr->pipe->up_methods->upm_start(pmr->xfer);
                    return_msg = seL4_MessageInfo_new(err, 1, 0, 0);
                    break;
                case (ABORT):
                    pmr->pipe->up_methods->upm_abort(pmr->xfer);
                    break;
                case (CLOSE):
                    pmr->pipe->up_methods->upm_close(pmr->pipe);
                    break;
                case (CLEARTOGGLE):
                    pmr->pipe->up_methods->upm_cleartoggle(pmr->pipe);
                    break;
                case (DONE):
                    pmr->pipe->up_methods->upm_done(pmr->xfer);
                    break;
                default:
                    printf("unexpected method %p\n", pmr->method_ptr);
            }
            break;
        default:
            printf("pipe handler proc Unexpected channel %d\n", ch);
    }
    printf("pipe handler ends\n");
    return return_msg;
}
