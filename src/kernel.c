#include <ps5kld/kernel.h>
#include <ps5kld/offsets/offsets.h>

// Kernel functions
void (*kprintf)(char* fmt, ...) = NULL;
uint64_t (*kmalloc)(size_t size, uint64_t *type, int flags) = NULL;
struct cdev* (*kmake_dev)(struct cdevsw *cdevsw, int unit, uid_t uid,
    gid_t gid, int perms, const char *fmt, ...) = NULL;
void (*kdestroy_dev)(struct cdev *dev) = NULL;

// Kernel variables
struct flat_pmap* kernel_pmap = NULL;
uint64_t kernel_base;
uint64_t Xfast_syscall = 0;
uint64_t* apic_ops = 0;
uint64_t* KM_TEMP = 0;
int* cpu_apic_ids = 0;
struct sysentvec* sysentvec = 0;
struct sysent* sysent_table = 0;
uint32_t kernel_fwver = 0;

#define SET_KERNEL_SYMBOLS(fw) do { \
    Xfast_syscall = Xfast_syscall_##fw; \
    kernel_base   = get_kernel_base(); \
    kprintf       = (void (*)(char *, ...))(kernel_base + kprintf_offset_##fw); \
    apic_ops      = (void *)(kernel_base + apic_ops_offset_##fw); \
    cpu_apic_ids  = (void *)(kernel_base + cpu_apic_ids_offset_##fw); \
    sysentvec     = sysentvec_offset_##fw == 0 ? 0 : \
        (struct sysentvec *)(kernel_base + sysentvec_offset_##fw); \
    sysent_table  = sysentvec == 0 ? 0 : sysentvec->sv_table; \
    kernel_pmap   = (void *)(kernel_base + kernel_pmap_offset_##fw); \
    kmalloc       = (void *)(kernel_base + malloc_offset_##fw); \
    KM_TEMP       = (void *)(kernel_base + malloc_MTEMP_offset_##fw); \
    kmake_dev     = make_dev_offset_##fw == 0 ? 0 : \
        (struct cdev* (*)(struct cdevsw *, int, uid_t, gid_t, int, \
        const char *, ...))(kernel_base + make_dev_offset_##fw); \
    kdestroy_dev  = destroy_dev_offset_##fw == 0 ? 0 : \
        (void (*)(struct cdev *))(kernel_base + destroy_dev_offset_##fw); \
} while (0)


uint64_t get_kernel_base()
{
    return rdmsr(MSR_LSTAR) - Xfast_syscall;
}


int init_kernel(uint32_t fwver)
{
    kernel_fwver = fwver;

    switch (fwver)
    {
        case 0x100:
            SET_KERNEL_SYMBOLS(100);
            break;
        case 0x270:
            SET_KERNEL_SYMBOLS(270);
            break;
        case 0x403:
            SET_KERNEL_SYMBOLS(403);
            break;
        case 0x500:
            SET_KERNEL_SYMBOLS(500);
            break;
        default:
            return 0; // unsupported
    }

    return 1;
}


void copy_gate(idt_64 *dst, const idt_64 *src)
{
    dst->offset_low = src->offset_low;
    dst->selector = src->selector;
    dst->ist_index = src->ist_index;
    dst->reserved_0 = src->reserved_0;
    dst->type = src->type;
    dst->dpl = src->dpl;
    dst->present = src->present;
    dst->offset_middle = src->offset_middle;
    dst->offset_high = src->offset_high;
    dst->reserverd_1 = src->reserverd_1;
}
