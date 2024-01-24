/* This work is Crown Copyright NCSC, 2023. */
#include <microkit.h>
#include <pdprint.h>
#include <tinyalloc.h>
#include <lib/libkern/libkern.h>

uintptr_t heap_base;
uintptr_t fdt;
uint64_t heap_size = 0x2000000;
int ta_blocks = 2048;
int ta_thresh = 16;
int ta_align = 64;
uintptr_t ta_limit;
#define STR2(x) #x
#define STR(x) STR2(x)
 
#define INCBIN_SECTION ".rodata"
char *pd_name = "mem_handler";
 
// this aligns start address to 16 and terminates byte array with explict 0
// which is not really needed, feel free to change it to whatever you want/need
#define INCBIN(name, file) \
    __asm__(".section " INCBIN_SECTION "\n" \
            ".global incbin_" STR(name) "_start\n" \
            ".balign 16\n" \
            "incbin_" STR(name) "_start:\n" \
            ".incbin \"" file "\"\n" \
            \
            ".global incbin_" STR(name) "_end\n" \
            ".balign 1\n" \
            "incbin_" STR(name) "_end:\n" \
            ".byte 0\n" \
    ); \
    extern __attribute__((aligned(16))) const char incbin_ ## name ## _start[]; \
    extern                              const char incbin_ ## name ## _end[]

int test1 = 12;
INCBIN(device_tree, "kernel/kernel.bin");
int test2 = 12;

void
init(void) {
    ta_limit = heap_base + heap_size;
    print_debug("Heap from %p to %p\n", heap_base, ta_limit);
    bool status = ta_init((void*)heap_base, (void*)ta_limit, ta_blocks, ta_thresh, ta_align);
    fdt = (uintptr_t) ta_alloc(incbin_device_tree_end - incbin_device_tree_start);
    memcpy(fdt, incbin_device_tree_start, incbin_device_tree_end - incbin_device_tree_start);
    print_info("Initialised\n");
}


void
notified(microkit_channel ch) {
}

microkit_msginfo
protected(microkit_channel ch, microkit_msginfo msginfo) {
    switch (ch) {
        case 0:
        case 5:
        case 10:
        case 20:
            return seL4_MessageInfo_new((uint64_t) ta_alloc(microkit_msginfo_get_label(msginfo)),1,0,0);
            break;
        case 1:
        case 6:
        case 11:
        case 21:
            return seL4_MessageInfo_new((uint64_t) ta_calloc(microkit_msginfo_get_label(msginfo), 1), 1, 0, 0);
            break;
        case 2:
        case 7:
        case 12:
        case 22:
            return seL4_MessageInfo_new(ta_free((void*)microkit_msginfo_get_label(msginfo)), 1, 0, 0);
            break;
        case 4:
            return seL4_MessageInfo_new(fdt, 1, 0, 0);
            break;
        default:
            print_warn("Unexpected channel mem_handler %d\n", ch);
    }
}
