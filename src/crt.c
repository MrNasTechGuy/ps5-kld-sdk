#include <crt.h>

extern uint8_t __bss_start[];
extern uint8_t __bss_end[];

// static void zero_bss(void)
// {
//     for (uint8_t *cursor = __bss_start; cursor < __bss_end; cursor++)
//         *cursor = 0;
// }

void _start(kproc_args* args)
{
    // zero_bss();

    if (init_kernel(args->fwver))
        module_start(args);
}
