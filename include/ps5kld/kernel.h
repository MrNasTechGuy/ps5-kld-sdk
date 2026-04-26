#pragma once

#define _KERNEL
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/cpuset.h>
#include <sys/sysent.h>
#include <machine/specialreg.h>
#include <stdint.h>

#include <ps5kld/machine/idt.h>

#define M_NOWAIT 0x0001
#define M_WAITOK 0x0002
#define M_ZERO 0x0100
#define M_NOVM 0x0200
#define M_USE_RESERVE 0x0400
#define M_NODUMP 0x0800

#ifndef PS5KLD_LOG
#define PS5KLD_LOG(...) kprintf(__VA_ARGS__)
#endif

struct flat_pmap {                                                                                                   
    uint64_t mtx_name_ptr;
    uint64_t mtx_flags;
    uint64_t mtx_data;
    uint64_t mtx_lock;
    uint64_t pm_pml4;
    uint64_t pm_cr3;
}; 

typedef struct __kproc_args
{
    uint64_t kdata_base;
    uint32_t fwver;
} kproc_args;

uint64_t get_kernel_base();
int init_kernel(uint32_t fwver);
void copy_gate(idt_64 *dst, const idt_64 *src);


// Kernel functions
extern void(*kprintf)(char* fmt, ...);
extern uint64_t(*kmalloc)(size_t size, uint64_t *type, int flags);
extern struct cdev* (*kmake_dev)(struct cdevsw *cdevsw, int unit, uid_t uid,
    gid_t gid, int perms, const char *fmt, ...);
extern void(*kdestroy_dev)(struct cdev *dev);

// Kernel variables
extern struct flat_pmap* kernel_pmap;
extern uint64_t* apic_ops;
extern uint64_t* KM_TEMP; // malloc_type
extern int* cpu_apic_ids;
extern struct sysentvec* sysentvec;
extern struct sysent* sysent_table;
extern uint32_t kernel_fwver;
