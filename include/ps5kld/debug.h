#pragma once

#include <stdint.h>

#include <ps5kld/trap.h>

#define HW_BREAKPOINT_SLOTS 4
#define IDT_DB_VECTOR 1
#define EFLAGS_RF (1ULL << 16)

typedef enum hwbp_type
{
    HWBP_EXECUTE = 0,
    HWBP_WRITE = 1,
    HWBP_IO = 2,
    HWBP_READWRITE = 3,
} hwbp_type_t;

typedef enum hwbp_len
{
    HWBP_LEN_1 = 1,
    HWBP_LEN_2 = 2,
    HWBP_LEN_4 = 4,
    HWBP_LEN_8 = 8,
} hwbp_len_t;

typedef int (*hwbp_callback_t)(int slot, trap_frame_t *frame, void *arg);

typedef struct hwbp
{
    uint64_t address;
    hwbp_type_t type;
    hwbp_len_t length;
    hwbp_callback_t callback;
    void *arg;
    uint8_t enabled;
} hwbp_t;

int debug_init(void);
int hwbp_find_free(void);
int hwbp_set_local(int slot, uint64_t address, hwbp_type_t type,
    hwbp_len_t length, hwbp_callback_t callback, void *arg);
int hwbp_set_all(int slot, uint64_t address, hwbp_type_t type,
    hwbp_len_t length, hwbp_callback_t callback, void *arg);
int hwbp_clear_local(int slot);
int hwbp_clear_all_cpus(int slot);
void hwbp_apply_local(void);
void hwbp_disable_slot_local(int slot);
int hwbp_enable_slot_local(int slot);

void debug_handle_db(trap_frame_t *frame);
