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

enum pipe_method {
    INIT,
    TRANSFER,
    START,
    ABORT,
    CLOSE,
    CLEARTOGGLE,
    DONE
};

enum channel_type {
    PIPE_INIT_CHANNEL = 10,
    PIPE_METHOD_CHANNEL = 11
};

enum init_methods {
    ROOTHUB_CTRL,
    XHCI_ROOT_INTR,
    XHCI_DEVICE_CTRL,
    XHCI_DEVICE_ISOC,
    XHCI_DEVICE_BULK,
    XHCI_DEVICE_INTR
};

struct pipe_method_rpc {
    struct usbd_pipe *pipe;
    struct usbd_xfer *xfer;
    int method_ptr;
};

struct pipe_method_init {
    struct usbd_pipe *pipe;
    int method_ptr;
};