#include <lib/libkern/libkern.h>
#include <printf.h>

#define aprint_naive(...)   printf(__VA_ARGS__)
#define aprint_normal(...)  printf(__VA_ARGS__)
#ifdef SEL4_USB_DEBUG
#define aprint_verbose(...) printf(__VA_ARGS__)
#define aprint_debug(...)   printf(__VA_ARGS__)
#define aprint_error(...)   printf(__VA_ARGS__)
#else
#define aprint_debug(...)   0
#define aprint_verbose(...) 0
#define aprint_error(...)   0
#endif

#define aprint_normal_dev(dev, ...)     aprint_normal(__VA_ARGS__)
#define aprint_verbose_dev(dev, ...)    aprint_verbose(__VA_ARGS__)
#define aprint_debug_dev(dev, ...)      aprint_debug(__VA_ARGS__)
#define aprint_error_dev(dev, ...)      aprint_error(__VA_ARGS__)

// no-ops
#define panic(s) 0
#define KERNEL_LOCK(count, lwp) 0
#define KERNEL_UNLOCK_ONE(lwp) 0
#define KERNEL_LOCKED_P() 0