#pragma once

#define kernel_base_addr 0xFFFFFFFF80210000 

#define OFFSET(x) x - kernel_base_addr

#define Xfast_syscall_403 0x294218
#define kprintf_offset_403 0x28da78
#define apic_ops_offset_403 OFFSET(0xFFFFFFFF81B44AC8)
#define cpu_apic_ids_offset_403 OFFSET(0xFFFFFFFF83BCC870)
#define kernel_pmap_offset_403 OFFSET(0xFFFFFFFF84067A78)
#define malloc_offset_403 OFFSET(0xFFFFFFFF80D66500)
#define malloc_MTEMP_offset_403 OFFSET(0xFFFFFFFF82156080)
#define make_dev_offset_403 OFFSET(0xFFFFFFFF80890C70)
#define destroy_dev_offset_403 OFFSET(0xFFFFFFFF808913F0 )
#define sysentvec_offset_403 OFFSET(0xFFFFFFFF81B21D30)
