#pragma once

#include <stdint.h>

#include <ps5kld/trap.h>

#define MAX_CPUS 16
#ifndef IPI_VECTOR
#define IPI_VECTOR 0xfe
#endif
#define APIC_ICR_LOW_INDEX (0x300 / sizeof(uint32_t))
#define APIC_ICR_HIGH_INDEX (0x310 / sizeof(uint32_t))
#define APIC_EOI_INDEX (0xb0 / sizeof(uint32_t))
#define APIC_ID_INDEX (0x20 / sizeof(uint32_t))
#define APIC_ID_SHIFT 24
#define APIC_DELIVERY_PENDING 0x1000
#define IPI_ICR (0x4000 | IPI_VECTOR)
#define MSR_APIC_BASE 0x1b
#define MSR_X2APIC_ID 0x802
#define MSR_X2APIC_EOI 0x80b
#define MSR_X2APIC_ICR 0x830
#ifndef APICBASE_X2APIC
#define APICBASE_X2APIC (1ULL << 10)
#endif
#ifndef APICBASE_ADDRESS
#define APICBASE_ADDRESS 0xfffff000ULL
#endif

typedef void (*ipi_callback_t)(int cpu, void *arg, trap_frame_t *frame);

int cpu_id(void);
int cpu_apic_id(int cpu);
int ipi_init(void);
int ipi_send_cpu(int cpu);
int ipi_run_on_cpu(int cpu, ipi_callback_t callback, void *arg);
int ipi_run_on_all(ipi_callback_t callback, void *arg);
int ipi_run_on_others(ipi_callback_t callback, void *arg);

void ipi_handle_interrupt(trap_frame_t *frame);
