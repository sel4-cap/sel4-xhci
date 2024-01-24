#include <printf.h>

char *pd_name;
// #define PD_DEBUG

#ifdef PD_DEBUG
#define print_debug(...) printf("%s|DEBUG: ", pd_name); printf(__VA_ARGS__);
#else
#define print_debug(...)
#endif

#define print_info(...) printf("%s|INFO: ", pd_name); printf(__VA_ARGS__)
#define print_warn(...) printf("%s|WARN: ", pd_name); printf(__VA_ARGS__)
#define print_fatal(...) printf("%s|FATAL: ", pd_name); printf(__VA_ARGS__)
