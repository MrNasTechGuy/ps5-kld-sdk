#pragma once

#include <stdint.h>

#include <ps5kld/debug.h>

typedef enum hook_kind
{
    HOOK_NONE = 0,
    HOOK_HWBP = 1,
} hook_kind_t;

typedef enum hook_type
{
    ALWAYS_ENABLED = 0,
    ONESHOT = 1,
} hook_type_t;

typedef struct hook
{
    void *target;
    void *replacement;
    void *gateway;
    hook_kind_t kind;
    hook_type_t type;
    int hwbp_slot;
    uint8_t installed;
} hook_t;

int hook_install(hook_t *hook, void *target, void *replacement);
int hook_install_hwbp(hook_t *hook, void *target, void *replacement);
int hook_disable_current_cpu(hook_t *hook);
int hook_enable_current_cpu(hook_t *hook);
int hook_remove(hook_t *hook);

void hook_manager_init(void);
hook_t *hook_manager_add(void *target, void *replacement, hook_type_t type);
int hook_manager_remove(void *target);
hook_t *hook_manager_find(void *target);
int hook_manager_list(hook_t *hooks, size_t *len);
