#include <ps5kld/debug.h>
#include <ps5kld/intrin.h>
#include <ps5kld/ipi.h>
#include <ps5kld/kernel.h>
#include <ps5kld/machine/idt.h>

extern void db_int_handler(void);

static idt_64 db_original_gate;
static volatile uint32_t db_installed;
static hwbp_t hwbp_slots[HW_BREAKPOINT_SLOTS];

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

static int hwbp_len_encode(hwbp_len_t length)
{
    switch (length)
    {
        case HWBP_LEN_1:
            return 0;
        case HWBP_LEN_2:
            return 1;
        case HWBP_LEN_4:
            return 3;
        case HWBP_LEN_8:
            return 2;
        default:
            return -1;
    }
}

static int hwbp_valid(int slot, hwbp_type_t type,
    hwbp_len_t length)
{
    if (slot < 0 || slot >= HW_BREAKPOINT_SLOTS)
        return 0;
    if (type == HWBP_EXECUTE && length != HWBP_LEN_1)
        return 0;
    if (hwbp_len_encode(length) < 0)
        return 0;

    return 1;
}

static void hwbp_configure_slot(int slot, uint64_t address,
    hwbp_type_t type, hwbp_len_t length, hwbp_callback_t callback, void *arg)
{
    hwbp_slots[slot].address = address;
    hwbp_slots[slot].type = type;
    hwbp_slots[slot].length = length;
    hwbp_slots[slot].callback = callback;
    hwbp_slots[slot].arg = arg;
    hwbp_slots[slot].enabled = 1;
    __sync_synchronize();
}

static void writedr_slot(int slot, uint64_t value)
{
    switch (slot)
    {
        case 0:
            __writedr0(value);
            break;
        case 1:
            __writedr1(value);
            break;
        case 2:
            __writedr2(value);
            break;
        case 3:
            __writedr3(value);
            break;
    }
}

void hwbp_disable_slot_local(int slot)
{
    uint64_t dr7;
    uint64_t control_shift;
    uint64_t enable_shift;

    if (slot < 0 || slot >= HW_BREAKPOINT_SLOTS)
        return;

    dr7 = __readdr7();
    control_shift = 16 + (slot * 4);
    enable_shift = slot * 2;

    dr7 &= ~(3ULL << enable_shift);
    dr7 &= ~(0xfULL << control_shift);
    writedr_slot(slot, 0);
    __writedr6(0);
    __writedr7(dr7);
}

int hwbp_enable_slot_local(int slot)
{
    hwbp_t *bp;
    uint64_t dr7;
    uint64_t control_shift;
    uint64_t enable_shift;
    int len;

    if (slot < 0 || slot >= HW_BREAKPOINT_SLOTS)
        return -1;

    bp = &hwbp_slots[slot];
    if (bp->enabled == 0)
        return -1;

    len = hwbp_len_encode(bp->length);
    if (len < 0)
        return -1;

    dr7 = __readdr7();
    control_shift = 16 + (slot * 4);
    enable_shift = slot * 2;

    dr7 &= ~(3ULL << enable_shift);
    dr7 &= ~(0xfULL << control_shift);
    writedr_slot(slot, bp->address);
    dr7 |= 1ULL << enable_shift;
    dr7 |= ((uint64_t)bp->type & 0x3ULL) << control_shift;
    dr7 |= ((uint64_t)len & 0x3ULL) << (control_shift + 2);

    __writedr6(0);
    __writedr7(dr7);
    return 0;
}

int debug_init(void)
{
    idt_64 *entry;

    if (db_installed != 0)
        return 0;

    entry = idt_entries();
    if (entry == 0)
        return -1;

    copy_gate(&db_original_gate, &entry[IDT_DB_VECTOR]);
    set_gate_handler(&entry[IDT_DB_VECTOR], db_int_handler);

    db_installed = 1;
    return 0;
}

int hwbp_find_free(void)
{
    for (int slot = 0; slot < HW_BREAKPOINT_SLOTS; slot++)
    {
        if (hwbp_slots[slot].enabled == 0)
            return slot;
    }

    return -1;
}

void hwbp_apply_local(void)
{
    uint64_t dr7 = __readdr7();

    for (int slot = 0; slot < HW_BREAKPOINT_SLOTS; slot++)
    {
        hwbp_t *bp = &hwbp_slots[slot];
        uint64_t control_shift = 16 + (slot * 4);
        uint64_t enable_shift = slot * 2;
        int len;

        dr7 &= ~(3ULL << enable_shift);
        dr7 &= ~(0xfULL << control_shift);

        if (bp->enabled == 0)
        {
            writedr_slot(slot, 0);
            continue;
        }

        len = hwbp_len_encode(bp->length);
        if (len < 0)
            continue;

        writedr_slot(slot, bp->address);
        dr7 |= 1ULL << enable_shift;
        dr7 |= ((uint64_t)bp->type & 0x3ULL) << control_shift;
        dr7 |= ((uint64_t)len & 0x3ULL) << (control_shift + 2);
    }

    __writedr6(0);
    __writedr7(dr7);
}

static void hwbp_apply_ipi(int cpu __unused, void *arg __unused,
    trap_frame_t *frame __unused)
{
    hwbp_apply_local();
}

int hwbp_set_local(int slot, uint64_t address, hwbp_type_t type,
    hwbp_len_t length, hwbp_callback_t callback, void *arg)
{
    if (hwbp_valid(slot, type, length) == 0 || address == 0)
        return -1;
    if (debug_init() != 0)
        return -1;

    hwbp_configure_slot(slot, address, type, length, callback, arg);
    hwbp_apply_local();
    return 0;
}

int hwbp_set_all(int slot, uint64_t address, hwbp_type_t type,
    hwbp_len_t length, hwbp_callback_t callback, void *arg)
{
    int error;

    if (hwbp_valid(slot, type, length) == 0 || address == 0)
        return -1;
    if (debug_init() != 0)
        return -1;

    hwbp_configure_slot(slot, address, type, length, callback, arg);

    error = ipi_run_on_others(hwbp_apply_ipi, 0);
    if (error != 0)
    {
        hwbp_slots[slot].enabled = 0;
        __sync_synchronize();
        return error;
    }

    hwbp_apply_local();
    return error;
}

int hwbp_clear_local(int slot)
{
    if (slot < 0 || slot >= HW_BREAKPOINT_SLOTS)
        return -1;

    hwbp_slots[slot].enabled = 0;
    hwbp_slots[slot].address = 0;
    hwbp_slots[slot].callback = 0;
    hwbp_slots[slot].arg = 0;
    __sync_synchronize();

    hwbp_apply_local();
    return 0;
}

int hwbp_clear_all_cpus(int slot)
{
    int error = hwbp_clear_local(slot);

    if (error != 0)
        return error;

    return ipi_run_on_all(hwbp_apply_ipi, 0);
}

void debug_handle_db(trap_frame_t *frame)
{
    uint64_t dr6 = __readdr6();
    int handled = 0;

    for (int slot = 0; slot < HW_BREAKPOINT_SLOTS; slot++)
    {
        hwbp_t *bp = &hwbp_slots[slot];

        if ((dr6 & (1ULL << slot)) == 0)
            continue;
        if (bp->enabled == 0)
        {
            hwbp_disable_slot_local(slot);
            handled = 1;
            continue;
        }
        if (bp->type == HWBP_EXECUTE && frame->rip != bp->address)
        {
            hwbp_disable_slot_local(slot);
            handled = 1;
            continue;
        }

        handled = 1;
        if (bp->callback != 0 && bp->callback(slot, frame, bp->arg) == 0)
            hwbp_disable_slot_local(slot);
    }

    if (handled != 0)
        frame->eflags |= EFLAGS_RF;

    __writedr6(0);
}
