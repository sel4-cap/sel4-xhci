# seL4 USB 3.0 (xHCI) driver

This repo contains the code for the xHCI driver developed by capgemini. This is not intended to be standalone and should be used by the [Microkit manifest](https://github.com/sel4-cap/microkit-manifest).

Specifically, we have provided support for these USB devices:
- Keyboard
- Optical Mouse
- Resistive Touch Screen (Capacitative untested but believed to also be supported)
- USB Hub
- Mass Storage Flash Drives

## Directory structure
- `cap/`: microkit files that handle the initialisation of protection domains and interrupts/notifications. Also includes helper functions (such as `sel4_bus_funcs.c`)
- `include/`: contains header files for use by NetBSD fork
- `netbsd/`: Capgemini fork of NetBSD customised to function in the context of microkit. Note that the files are included in [this repository](https://github.com/sel4-cap/netbsd) and will be pulled in by the repo file in the manifest.
- `src/`: Contains external helper files and files that are normally generated by NetBSD at kernel compile time.
- `xhci_stub.system`: microkit system description file.

## Requirements
- This driver expects to have the accompanying NetBSD fork.
  - TEMP NOTE: use branch 'api' for netbsd
  - TEMP NOTE: use branch 'api-empty for xhci_stub
- This driver assumes the existence of a built microkit sdk.
- This driver assumes build microkit libc (found in this [repository](https://github.com/sel4-cap/picolibc/tree/linker_crt))
    - Use branch linker_crt
## Building
To build this driver, use the `build.sh` bash script. The script assumes the driver has been pulled using the microkit manifest and therefore assumes the directory structure will mirror this.

Breakdown of variables set:
- `MICROKIT_DIR`: Base directory of microkit repository
- `MICROKIT_SDK`: Repository of microkit SDK
- `MICROKIT_BOARD`: Board to create image for (default: maaxboard)
- `MICROKIT_CONFIG`: Configuration to build with (default: debug)
- `MICROKIT_TOOL`: Path to microkit tool
- `BOARD_DIR`: Directory used by Microkit to pull includes
- `BUILD_DIR`: (Can be modified) directory to produce image file
- `PYTHONPATH`: Used by python microkit tool
- `PATH`: Path augmented to include path of `aarch64_none_elf-gcc` compiler
- `SEL4_XHCI_PATH`: Path of xhci_stub/ directory
- `NETBSD_DIR`: Path of netbsd directory

By default, the driver will build with an empty example client that is subscribed to all interrupt channels. Data will not be handled. Upon successful initialisation, a list of devices plugged into the board will be printed.

## Setup
The driver will require some memory regions to be mapped into both the driver PD and the client PD. Here is a list of the required memory regions:
- **Free/used buffers** for each communication ring. Suggested size of each of these is around `0x200_000`, but any size can be used. Each of these should be mapped into xhci_stub, software_interrupt, and client PD.
    - `kbd_free/kbd_used`
    - `umass_req_free/umass_req_used`
    - `mse_free/mse_used`
    - `uts_free/uts_used`
    - `usb_new_device_free/usb_new_device_used`
- **Shared heap** to store data structures that would need to be shared between the PDs. This should also be mapped into all three PDs. Suggested size is `0x20000000`, but again, any size would be fine.
- **libc heap**: a heap start and end to be used by picolibc. They must be mapped to `__heap_start` and `__heap_end` in the client PD.
    - Note: for the time being, two distinct memory regions are required, with `__heap_end` simulating the end of the heap, and size should be as small as possible (`0x1000`).

Examples of these can be seen in the `xhci_stub.system` file.

## Using driver
To use the driver, see the sample client (in `sample_client`). The example is *subscribed* to all 4 devices. To subscribe to an interrupt, modify the `.system` file to connect the client to the appropriate channel (see below).

### Init (channel: 40)
This is a software interrupt generated by the driver to notify initial setup is complete. Note, if a USB hub is connected, further setup may occur for a short while afterwards.

Example:
```xml
<!-- Driver initialise complete -->
<channel>
    <end pd="xhci_stub" id="40"/><!--must be mapped to xhci_stub-->
    <end pd="example_pd" id="40"/>
</channel>
```

### New device (channel: 45)
Interrupt generated on new device connection. To allow the API to handle this new device, use `handle_new_device()`. The API will decode the information sent by the driver and populate a list of devices (`device_list`) as they are connected. This list can be accessed, and each entry is of type `struct sel4_usb_device`. This structure is as follows:

- id: device id
- vendor: vendor string
- product: product string
- vendorid: vendor hex code
- productid: product hex code
- class: class of device (if 0, check interface)
- subclass: subclass of device
- ifaceClass: interface class
- ifaceSub: interface sub class
- depth: how many connections from host controller device is
- speed: speed of device
- protocol: USB protocol
- mps: max packet size (CURRENTLY BROKEN)
- len: length of packet
- num_configs: number of configs
- rev: USB revision
- umass_dev: mass storage specific information
    - umass_id: mass storage specific id
    - locked: mutex to ensure only one process can occur at a given time

The hex codes for vendor and product can be used to look up device specific information in the USB-IF database.

```xml
<!-- USB Device Connect Event -->
<channel>
    <end pd="xhci_stub" id="45"/> <!-- must be mapped to xhci_stub -->
    <end pd="example_pd" id="45"/>
</channel>
```

### Keyboard (channel: 46)
The keyboard interrupt signifies a key event generated from the keyboard. The driver will create a buffer in the `kbd_buffer_ring->used_ring` that can be dequeued and processed as desired. To dequeue a buffer, use `dequeue_free(kbd_buffer_ring, buffer, len, NULL)` where the data is written `buffer` which is a pointer of length `len`.

The buffer is an array of hexadecimal codes where the first 2 entries are reserved for special codes, and the others (length depends on keyboard) give the state of the keyboard (keys currently pressed). Note that this channel will fire on both key down and key up events, so it may be useful to store the previous buffer state to calculate which key is unique. If too many keypresses are input for a specific keyboard, all entries in this buffer will be set to `0x1`.

The first item in the buffer (`buffer[0]`) corresponds to:
- `0x01`: Ctrl
- `0x02`: Shft
- `0x04`: Alt
- ...

To get the keycode of a buffer entry, use `buffer[id]&0xFF`. This keycode can then be used as a key to a keyboard decoding table. An example table is included in `wsksymdef.h`.

Example:
```xml
<!-- Keyboard interrupt -->
<channel>
    <end pd="software_interrupt" id="45"/> <!-- must be mapped to software interrupt -->
    <end pd="example_pd" id="45"/>
</channel>
```

### Mouse (channel: 48)
The mouse interrupt is generated on mouse move or click. Similar to keyboard, the driver will create a buffer in `mse_buffer_ring->used_ring`, and can be dequeued by the client progam using `dequeue_free` as before.

The buffer is an array of changes to the direction of the mouse, and can be used as follows:
- `buffer[0]`: x direction
- `buffer[1]`: y direction
- `buffer[2]`: wheel direction
- `buffer[3]`: z direction
- `buffer[4]`: Mouse buttons, a flag indicating buttons down:
    - `0x1`: left
    - `0x2`: middle
    - `0x4`: right

### Touchscreen (channel: 49)
The touchscreen interrupt is generated on a touch event. Similar to mouse, the driver will create a buffer in `uts_buffer_ring->used_ring`, and can be dequeued by the client program using `dequeue_free` as before.

The buffer contains the x and y coordinates, and a z value, which indicates pressure (supported by some hardware). It also has buttons held.
- `buffer[0]`: x coordinate
- `buffer[1]`: y coordinate
- `buffer[2]`: z value
- `buffer[3]`: Buttons, a flag indicating buttons down:
    - `0x1`: left
    - `0x2`: middle
    - `0x4`: right
### Mass storage (channel: 50)
Mass storage works in a different way to the previously mentioned devices, since this device will not generate interrupts without software interaction. This API allows a read/write command to be passed to the driver, which will return with the desired output.

For both commands, `enqueue_umass_request()` is used. The API handles the locking and unlocking of devices and keeps track of the transfer requests automatically. The arguments to this command are as follows:
- `int dev_id`: ID of device to send request to. Must be a mass storage device.
- `bool read`: True for read request, false for write request
- `int blkno`: Block number in device to start operation
- `int nblks`: Number of blocks to perform operation on (NOTE: currently limited to 8)
- `void* val`: Used differently depending on read/write
    - Read: Pointer in shared data to write data
    - Write: Pointer in shared data containing value to write
- `void* cb`: Optional callback function that is called on transfer completion.

## CHANGELOG
### DONE:
- Added maaxboard to the build_sdk.py
- Created a build system
- Pulled in initial bus reads/writes
- Pulled in all netbsd headers required
- Can perform dwc3 bring up
- Implemented printf and malloc
    - malloc receives heap base allocated in `xhci_stub.system`
    - printf receives `microkit_debug_putc()`
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
    - Includes basic prototype: will output basic characters, no use of shift or ctrl (but the presses are registered).
- Can send keypresses to separate PD using notifications and shared memory address
    - Other PD handles shift but not altgr
- Mouse driver now included
    - Buttons and direction
- Touch screen driver now included
    - Our device has no z input but should work for those devices that do
- Autoconf to enable switching between devices (attempts to match and attaches to best match) - as opposed to harcoding for specific devices
- USB Hub support now included
    - Tested with 3 devices, more likely to be supported but untested
- Mass storage support
- API created with sample client

### TODO:
- Condition variables (?)
- More devices:
    - ~~Mass storage~~ DONE
    - ~~Touchscreen~~ DONE
    - ~~Root hub that isn't the pseudo device~~ DONE
- Simultaneous devices. Work but needs more extensive testing to make sure we don't have any race conditions

### Bugs/strange things/important changelog
- ~~`xhci_setup_route` should include path of root hub, but doesn't. No issues because of this yet, but something to look at in the future if breakages happen~~
    - **NEW!** Fixed by correctly attaching to roothub and setting up parents properly.
- ~~`xfer->ux_callback()` commented out and replaced with `uhub_intr()` or `device_ctrl_intr()`. Will likely become a problem in the future, but right now it works.~~
    - ~~A few other hard commented quirks, such as `uhidev_attach` and `ukbd_intr`~~
    - **NEW!** No longer a problem. Autoconf introduced to remove attach hardcodings and new structure in usb.h: `intr_ptrs` included to contain pointers to interrupt function in software interrupt PD.
- Setting MaxBRate of device in `xhci_new_device` returns 0, overwriting correct value returned by `get_initial_ddesc()` if device speed is not ss. Fixed by checking if 0 is returned and if so using the ddesc value.
    - **TODO!**: this always returns 9 which is incorrect usage.
- `bus_methods` replaced with pointer to `xhci_bus_methods` because this is an xhci driver, so assume that it's always going to use an xhci bus.
- `cv_waitsig()` replaced with `usbd_delay_ms(0, 100)` to delay 100ms instead of waiting for cv to change.
- `xfer->ux_status` set to `USBD_IN_PROGRESS` before doing anything else (different to netbsd which schedules a callout).
- Callouts not set, meaning timeout will never happen (netbsd waits 5s)
- Autoconf pulled in manually.
    - Makes use of ioconf.c which is generated on build of netbsd - hardware (board) specific to a degree
    - Devices are uncommented as and when needed to reduce searching overhead
- ~~Hot plugging not implemented (requires a task thread that would have to be a separate PD)~~
    - **NEW!**: Hot plugging implemented (does not work with external hubs), separate PD not necessary.
- ~~RepID issue with touch screen - so prevented from becoming 0 (and crashing)~~
    - **NEW!** fixed. Slight bug in setting parent structure fixed.

### Currently supported autoconf devices

| **Device** | **Name in ioconf.c** | **Notes** |
| --- | --- | --- |
| USB | usb | General usb functions |
| Roothub | uroothub | Enables root hub capabilities |
| USB Hub | uhub    | Physical USB Hub |
| Human interface device | uhidev | General parent for any HIDs (keyboard, mouse, etc) |
| Keyboard | ukbd | Keyboard driver |
| Mouse    | ums  | Mouse driver |
| Touchscreen | uts | Touch screen driver |
| Mass storage | umass | Mass storage |

## External sources
- Tiny alloc: https://github.com/thi-ng/tinyalloc/tree/master
    - License: apache
- printf: https://github.com/mpaland/printf/tree/master
    - License: MIT
- memset/memcpy/strlen etc etc: pulled from linux source, stored in util.c
    - License: BSD-2
