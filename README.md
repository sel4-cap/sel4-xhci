# XHCI_STUB

## DONE:
- Added maaxboard to the build_sdk.py
- Created a (very average) build system
- Pulled in initial bus reads/writes
- Pulled in all netbsd headers required
- Can perform dwc3 bring up
- "Implemented" printf and malloc
    - malloc receives heap base allocated in `xhci_stub.system`
    - printf receives `sel4cp_debug_putc()`
- Pulled in other C files
- DMA done
    - Incredibly basic algorithm that just has a base pointer for virt and phys addresses and then increments
    - No freeing or reallocation possible yet
- Interrupts enabled
    - Hard and soft
- Keyboard driver now included
    - Is able to set up keyboard to trigger hardware interrupts
    - LEDs not quite working yet (can probably sort this later)
        - TODO: They did work briefly, but now don't again
    - Includes (incredibly) basic prototype: will output basic characters, no use of shift or ctrl (but the presses are actually registered).
- Can send keypresses to separate PD using notifications and shared memory address
    - Other PD handles shift but not altgr
- Mouse driver now included
    - Buttons and direction
- Touch screen driver now included
    - Our device has no z input but should work for those devices that do
- Autoconf to enable switching between devices (attempts to match and attaches to best match) - as opposed to harcoding for specific devices
- USB Hub support in progress (mostly done but requires testing)
- Mass storage support in progress (built but requires testing)

## TODO:
- Neaten up code sooner rather than later
    - Look at CMake now
    - Platform headers for future proofing
    - Headers for things like heap size and heap base
- Setup condition variables
- More devices:
    - Mass storage
    - Touchscreen (Done)
    - Root hub that isn't the pseudo device (in progress)
- Simultaneous devices (scary!)

## Bugs/strange things/important changelog
- `xhci_setup_route` should include path of root hub, but doesn't. No issues because of this yet, but something to look at in the future if breakages happen
- ~~`xfer->ux_callback()` commented out and replaced with `uhub_intr()` or `device_ctrl_intr()`. Will likely become a problem in the future, but right now it works.~~
    - ~~A few other hard commented quirks, such as `uhidev_attach` and `ukbd_intr`~~
    - **NEW!** No longer a problem. Autoconf introduced to remove attach hardcodings and new structure in usb.h: `intr_ptrs` included to contain pointers to interrupt function in software interrupt PD.
- Setting MaxBrate of device in `xhci_new_device` returns 0, overwriting correct value returned by `get_initial_ddesc()` if device speed is not ss. Fixed by checking if 0 is returned and if so using the ddesc value.
- `bus_methods` replaced with pointer to `xhci_bus_methods` because this is an xhci driver, so assume that it's always going to use an xhci bus.
- `cv_waitsig()` replaced with `usbd_delay_ms(0, 100)` to delay 100ms instead of waiting for cv to change.
- `xfer->ux_status` set to `USBD_IN_PROGRESS` before doing anything else (different to netbsd which schedules a callout).
- Callouts not set, meaning timeout will never happen (netbsd waits 5s)
- Autoconf pulled in manually.
    - Makes use of ioconf.c which is generated on build of netbsd - hardware (board) specific to a degree
    - Devices are uncommented as and when needed to reduce searching overhead
- Hot plugging not 
- RepID issue with touch screen - so prevented from becoming 0 (and crashing)

## Currently supported autoconf devices

| **Device** | **Name in ioconf.c** | **Notes** |
| --- | --- | --- |
| USB | usb | General usb functions |
| Roothub | uroothub | Enables root hub capabilities |
| USB Hub | uhub    | Physical USB Hub |
| Human interface device | uhidev | General parent for any HIDs (keyboard, mouse, etc) |
| Keyboard | ukbd | Keyboard driver |
| Mouse    | ums  | Mouse driver |
| Touchscreen | uts | Touch screen driver |
In Progress - umass, umass_scsipi (mass storage, scsi/atapi)

To be updated as and when

## External sources
- Tiny alloc: https://github.com/thi-ng/tinyalloc/tree/master
    - License: apache
- printf: https://github.com/mpaland/printf/tree/master
    - License: MIT
- memset/memcpy/strlen etc etc: pulled from linux source, stored in util.c
    - License: whatever linux has (I think BSD)