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
#include <pipe_methods.h>

uintptr_t xhci_base;
uintptr_t dma_base;
uintptr_t heap_base;
uintptr_t pipe_heap_base;
uintptr_t bus_heap_base;
uintptr_t dma_cp_vaddr = 0x54000000;
uintptr_t dma_cp_paddr;
uintptr_t timer_base;

struct xhci_softc *glob_xhci_sc	= NULL;
uintptr_t xhci_root_intr_pointer;
uintptr_t xhci_root_intr_pointer_other;

uint64_t heap_size = 0x2000000;
int ta_blocks = 256;
int ta_thresh = 16;
int ta_align = 64;

void
init(void) {
    printf("PIPE_HANDLER: dmapaddr = %p\n", dma_cp_paddr);
    /* xhci_root_intr_pointer = get_root_intr_methods(); */
    /* printf("root_intr_ptr = %p\n", xhci_root_intr_pointer); */
    sel4_dma_init(dma_cp_paddr, dma_cp_vaddr, dma_cp_vaddr + 0x200000);
    initialise_and_start_timer(timer_base);
    int ta_limit = bus_heap_base + heap_size;
    bool error = ta_init((void*)bus_heap_base, (void*)ta_limit, ta_blocks, ta_thresh, ta_align);
    printf("bus_handler up and running\n");
}

void
notified(microkit_channel ch) {
    printf("pipe_methods_thread called!\n");
}

microkit_msginfo
protected(microkit_channel ch, microkit_msginfo msginfo) {
    printf("Inside protected (pipe handler)\n");
    struct pipe_method_rpc *pmr;
    struct pipe_method_init *pmi;
    switch (ch) {
        case (BUS_INIT_CHANNEL):
        case (BUS_INIT_CHANNEL + 10):
            pmi = (struct pipe_method_init *) microkit_msginfo_get_label(msginfo);
            pmi->pipe->up_methods = get_up_methods(pmi->method_ptr);
            printf("set init to %p\n", pmi->pipe->up_methods);
            break;
        case (BUS_METHOD_CHANNEL):
        case (BUS_METHOD_CHANNEL+10):
            pmr = (struct pipe_method_rpc *) microkit_msginfo_get_label(msginfo);
            switch (pmr->method_ptr) {
                case (TRANSFER):
                    pmr = (struct pipe_method_rpc *) microkit_msginfo_get_label(msginfo);
                    pmr->pipe->up_methods->upm_transfer(pmr->xfer);
                    break;
                case (START):
                    pmr = (struct pipe_method_rpc *) microkit_msginfo_get_label(msginfo);
                    pmr->pipe->up_methods->upm_start(pmr->xfer);
                    break;
                case (ABORT):
                    pmr = (struct pipe_method_rpc *) microkit_msginfo_get_label(msginfo);
                    pmr->pipe->up_methods->upm_abort(pmr->xfer);
                    break;
                case (CLOSE):
                    pmr = (struct pipe_method_rpc *) microkit_msginfo_get_label(msginfo);
                    pmr->pipe->up_methods->upm_close(pmr->pipe);
                    break;
                case (CLEARTOGGLE):
                    pmr = (struct pipe_method_rpc *) microkit_msginfo_get_label(msginfo);
                    pmr->pipe->up_methods->upm_cleartoggle(pmr->pipe);
                    break;
                case (DONE):
                    pmr = (struct pipe_method_rpc *) microkit_msginfo_get_label(msginfo);
                    pmr->pipe->up_methods->upm_done(pmr->xfer);
                    break;
            }
        default:
            printf("pipe handler proc Unexpected channel %d\n", ch);
    }
    return seL4_MessageInfo_new(0,0,0,0);
}
