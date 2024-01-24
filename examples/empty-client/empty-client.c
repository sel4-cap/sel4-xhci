/* This work is Crown Copyright NCSC, 2023. */
#include <microkit.h>
#include <xhci_api.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// includes for keyboard lookup table
#include "wsksymdef.h"
typedef unsigned short keysym_t;

//rings for notifications
uintptr_t kbd_free;
uintptr_t kbd_used;
uintptr_t mse_free;
uintptr_t mse_used;
uintptr_t uts_free;
uintptr_t uts_used;
uintptr_t umass_req_free;
uintptr_t umass_req_used;

/* Pointers to shared_ringbuffers */
ring_handle_t *kbd_buffer_ring;
ring_handle_t *mse_buffer_ring;
ring_handle_t *uts_buffer_ring;
ring_handle_t *umass_buffer_ring;

//keyboard specifics (optional for decoding keypress)
extern const keysym_t hidkbd_keydesc_us[];
extern const keysym_t hidkbd_keydesc_uk[];
#define CODEMASK 0x0ff
#define KC(n)		KS_KEYCODE(n)

// heap(s)
uintptr_t shared_mem;
uintptr_t shared_heap;
uintptr_t shared_soft_heap;

// TEMP: fixes for LIBC DO NOT REMOVE
int __lshrti3(a,b) { return a >> b; }
int __ashlti3(a,b) { return a << b; }

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

void
handle_mouseevent() {
    uintptr_t *buffer = 0;
    unsigned int len = 0;
    void *cookie = NULL;

    while (!dequeue_used(mse_buffer_ring, (uintptr_t*)&buffer, &len, &cookie)) {
        printf("x: %d, y: %d, z:%d, w:%d buttons 0x%x\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
    }
}

static void 
handle_keypress()
{
    /* very simple keyobard handling that dumps the output of the keyboard
        * buffer into debug console */
    uintptr_t *buffer = 0;
    unsigned int len = 0;
    void *cookie = NULL;

    while (!dequeue_used(kbd_buffer_ring, (uintptr_t*)&buffer, &len, &cookie)) {
        printf("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", ((char*)buffer)[0], ((char*)buffer)[1], ((char*)buffer)[2], ((char*)buffer)[3], ((char*)buffer)[4], ((char*)buffer)[5]);
        printf("\r");
        printf("\x1b[s"); /* save current cursor position */
        printf("%*s", 30, ""); /* blank out virual screen */
        printf("\x1b[u"); /* move cursor back to saved position */
    }
}

void
init(void) {
    /* initialise  */
    api_init(&kbd_buffer_ring, &mse_buffer_ring, &uts_buffer_ring, &umass_buffer_ring);
    printf("CLIENT|Data structures initialised\n");
}

void
notified(microkit_channel ch) {
    switch(ch) {
/* ----------------API NOTIFICATIONS---------- */
        case INIT:
            printf("Driver initialised in client\n");
            break;
        case KEYBOARD_EVENT:
            handle_keypress();
            break;
        case MOUSE_EVENT:
            handle_mouseevent();
            break;
        case TOUCHSCREEN_EVENT:
            printf("touchscreen event not implemented\n");
            break;
        case UMASS_COMPLETE:
            handle_xfer_complete();
            break;
        case NEW_DEVICE_EVENT:
            handle_new_device();
            //print out devices here because they haven't been initialised before this point
            print_devs();
            break;
/* ---------------------------------------- */
        default:
            printf("Unexpected channel %d\n", ch);
    }
}
