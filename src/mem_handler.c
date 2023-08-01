#include <sel4cp.h>
#include <printf.h>
#include <tinyalloc.h>

uintptr_t heap_base;
uint64_t heap_size = 0x2000000;
int ta_blocks = 256;
int ta_thresh = 16;
int ta_align = 64;
uintptr_t ta_limit;

void
init(void) {
    ta_limit = heap_base + heap_size;
    printf("stack from %p to %p\n", heap_base, ta_limit);
    bool error = ta_init((void*)heap_base, (void*)ta_limit, ta_blocks, ta_thresh, ta_align);
    printf("Init malloc: %d\n", error);
    printf("mem_allocator up and running\n");
}


void
notified(sel4cp_channel ch) {
}

sel4cp_msginfo
protected(sel4cp_channel ch, sel4cp_msginfo msginfo) {
    switch (ch) {
        case 0:
        case 5:
        case 10:
        case 20:
        case 23:
        case 26:
            return seL4_MessageInfo_new(ta_alloc(sel4cp_msginfo_get_label(msginfo)),1,0,0);
            break;
        case 1:
        case 6:
        case 11:
        case 21:
        case 24:
        case 27:
            return seL4_MessageInfo_new(ta_calloc(sel4cp_msginfo_get_label(msginfo), 1), 1, 0, 0);
            break;
        case 2:
        case 7:
        case 12:
        case 22:
        case 25:
        case 28:
            return seL4_MessageInfo_new(ta_free(sel4cp_msginfo_get_label(msginfo)), 1, 0, 0);
            break;
        default:
            printf("Unexpected channel mem_handler %d\n", ch);
    }
}
