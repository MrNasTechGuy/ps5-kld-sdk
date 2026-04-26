#include <ps5kld/dmap.h>
#include <ps5kld/intrin.h>
#include <ps5kld/ipi.h>
#include <ps5kld/kernel.h>
#include <ps5kld/machine/idt.h>

#include <sys/pcpu.h>

extern void ipi_int_handler(void);

static idt_64 ipi_original_gate;
static volatile uint32_t ipi_installed;
static ipi_callback_t volatile ipi_callback;
static void *volatile ipi_callback_arg;
static volatile uint32_t ipi_eoi_mask;

static void set_gate_handler(idt_64 *gate, void *handler)
{
    uint64_t target = (uint64_t)handler;

    gate->offset_low = target & 0xffff;
    gate->offset_middle = (target >> 16) & 0xffff;
    gate->offset_high = (target >> 32) & 0xffffffff;
}

static idt_64 *idt_entries(void)
{
    uint8_t idt_storage[10];

    __sidt((uint64_t *)idt_storage);
    return (idt_64 *)((IDTR *)idt_storage)->base;
}

static volatile uint32_t *lapic_regs(void)
{
    uint64_t apic_base = __readmsr(MSR_APIC_BASE) & APICBASE_ADDRESS;
    uint64_t dmap = get_dmap();

    if (dmap == 0 || apic_base == 0)
        return 0;

    return (volatile uint32_t *)(dmap + apic_base);
}

static int lapic_x2apic_enabled(void)
{
    return (__readmsr(MSR_APIC_BASE) & APICBASE_X2APIC) != 0;
}

static void lapic_eoi(void)
{
    if (lapic_x2apic_enabled() != 0)
    {
        __writemsr(MSR_X2APIC_EOI, 0);
        return;
    }

    volatile uint32_t *apic = lapic_regs();

    if (apic != 0)
        apic[APIC_EOI_INDEX] = 0;
}

static int cpu_valid(int cpu)
{
    if (cpu < 0 || cpu >= MAX_CPUS || cpu_apic_ids == 0)
        return 0;

    return cpu_apic_ids[cpu] >= 0;
}

static int cpu_from_apic_id(int apic_id)
{
    if (apic_id < 0)
        return -1;

    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (cpu_valid(cpu) && cpu_apic_ids[cpu] == apic_id)
            return cpu;
    }

    return -1;
}

static int lapic_current_apic_id(void)
{
    volatile uint32_t *apic;

    if (lapic_x2apic_enabled() != 0)
        return (int)__readmsr(MSR_X2APIC_ID);

    apic = lapic_regs();
    if (apic == 0)
        return -1;

    return (int)((apic[APIC_ID_INDEX] >> APIC_ID_SHIFT) & 0xff);
}

int cpu_id(void)
{
    int cpu = cpu_from_apic_id(lapic_current_apic_id());

    if (cpu < 0)
        cpu = PCPU_GET(cpuid);

    if (cpu < 0 || cpu >= MAX_CPUS)
        return 0;

    return cpu;
}

int cpu_apic_id(int cpu)
{
    if (!cpu_valid(cpu))
        return -1;

    return cpu_apic_ids[cpu];
}

int ipi_init(void)
{
    idt_64 *entry;

    if (ipi_installed != 0)
        return 0;

    entry = idt_entries();
    if (entry == 0)
        return -1;

    copy_gate(&ipi_original_gate, &entry[IPI_VECTOR]);
    copy_gate(&entry[IPI_VECTOR], &entry[1]);
    set_gate_handler(&entry[IPI_VECTOR], ipi_int_handler);

    ipi_installed = 1;
    return 0;
}

int ipi_send_cpu(int cpu)
{
    volatile uint32_t *apic;
    int apic_id = cpu_apic_id(cpu);
    int x2apic = lapic_x2apic_enabled();
    uint64_t spins;

    if (apic_id < 0)
        return -1;

    if (x2apic != 0)
    {
        uint64_t icr = ((uint64_t)(uint32_t)apic_id << 32) | IPI_ICR;

        __sync_synchronize();
        __writemsr(MSR_X2APIC_ICR, icr);
        return 0;
    }

    apic = lapic_regs();
    if (apic == 0)
        return -1;

    for (spins = 0; spins < 0x100000ULL; spins++)
    {
        if ((apic[APIC_ICR_LOW_INDEX] & APIC_DELIVERY_PENDING) == 0)
            break;
        __asm__ volatile("" ::: "memory");
    }
    if ((apic[APIC_ICR_LOW_INDEX] & APIC_DELIVERY_PENDING) != 0)
        return -2;

    apic[APIC_ICR_HIGH_INDEX] = (uint32_t)apic_id << 24;
    __sync_synchronize();
    apic[APIC_ICR_LOW_INDEX] = IPI_ICR;
    return 0;
}

static int ipi_run_mask(uint32_t mask, int include_self,
    ipi_callback_t callback, void *arg)
{
    int self = cpu_id();
    uint32_t remote_mask = mask;
    int error = 0;

    if (callback == 0)
        return -1;
    if (ipi_init() != 0)
        return -1;

    if (self >= 0 && self < MAX_CPUS)
        remote_mask &= ~(1U << self);

    ipi_callback = callback;
    ipi_callback_arg = arg;
    ipi_eoi_mask = 0;
    __sync_synchronize();

    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        uint32_t cpu_bit;
        uint64_t spins;

        if ((remote_mask & (1U << cpu)) == 0)
            continue;

        __sync_synchronize();
        if (ipi_send_cpu(cpu) != 0)
        {
            error = -3;
            break;
        }

        cpu_bit = 1U << cpu;
        for (spins = 0; spins < 0x100000ULL; spins++)
        {
            if ((ipi_eoi_mask & cpu_bit) != 0)
                break;
            __asm__ volatile("" ::: "memory");
        }
        if ((ipi_eoi_mask & cpu_bit) == 0)
        {
            error = -4;
            break;
        }
    }

    if (error == 0 && include_self != 0 && (mask & (1U << self)) != 0)
        callback(self, arg, 0);

    __sync_synchronize();
    ipi_callback = 0;
    ipi_callback_arg = 0;
    return error;
}

int ipi_run_on_cpu(int cpu, ipi_callback_t callback, void *arg)
{
    if (!cpu_valid(cpu))
        return -1;

    return ipi_run_mask(1U << cpu, 1, callback, arg);
}

int ipi_run_on_all(ipi_callback_t callback, void *arg)
{
    uint32_t mask = 0;

    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (cpu_valid(cpu))
            mask |= 1U << cpu;
    }

    if (mask == 0)
        return -1;

    return ipi_run_mask(mask, 1, callback, arg);
}

int ipi_run_on_others(ipi_callback_t callback, void *arg)
{
    uint32_t mask = 0;
    int self = cpu_id();

    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (cpu != self && cpu_valid(cpu))
            mask |= 1U << cpu;
    }

    if (mask == 0)
        return 0;

    return ipi_run_mask(mask, 0, callback, arg);
}

void ipi_handle_interrupt(trap_frame_t *frame)
{
    int cpu = cpu_id();
    ipi_callback_t callback = ipi_callback;
    uint32_t cpu_bit = 0;

    if (cpu >= 0 && cpu < MAX_CPUS)
        cpu_bit = 1U << cpu;

    if (callback != 0)
        callback(cpu, ipi_callback_arg, frame);

    lapic_eoi();
    if (cpu_bit != 0)
        __sync_fetch_and_or(&ipi_eoi_mask, cpu_bit);
}
