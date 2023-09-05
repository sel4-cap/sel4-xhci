# seL4 USB 3.0 (xHCI) driver

This branch is specifically for the integration of the sDDF ethernet driver (https://github.com/lucypa/sDDF) with the NetBSD xHCI driver on MaaXBoard. All code in the `echo_server/` directory is directly from the sDDF repository, modified slightly in some cases.

This example does _**NOT**_ make use of the NetBSD fork utilised by the main branch. Instead, the code is kept in the `src/` directory, where files from NetBSD source have been extracted and modified for the purpose of this example.

## How to use
**NOTE!** this should be used on an Avnet MaaXBoard with an ethernet connection and a usb keyboard plugged into the usb port. 


1. Compile and boot the image as normal (see main branch for more details)
2. Observe and note the IP address displayed by the line
```
DHCP request finished, IP address for netif <DEV> is: <IP-ADDR>
```
3. On the working machine, connect to the MaaXBoard using:
```bash
nc <IP-ADDR> 1236 # 1236 is the port used in this example
``` 
On successful connect, the console should read `100 IPBENCH v1.0`.

4. Once the line `Setup finished, type on keyboard to begin` has appeared, begin typing on the keyboard connected to the board. The characters should show up on the screen (the buffer will only store 7 characters, and the keyboard class driver is not able to process keypresses such as ctrl or alt).

5. To request the buffer, type `KBD<return>` on the working machine. The example will send over and subsequently clear the buffer.

## External sources
- Tiny alloc: https://github.com/thi-ng/tinyalloc/tree/master
    - License: apache
- printf: https://github.com/mpaland/printf/tree/master
    - License: MIT
- memset/memcpy/strlen etc etc: pulled from linux source, stored in util.c
    - License: BSD-2
