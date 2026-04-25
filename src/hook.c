#include <ps5kld/hook.h>

#include <sys/queue.h>

typedef struct hook_entry
{
    hook_t hook;
    uint8_t used;
    SLIST_ENTRY(hook_entry) entries;
} hook_entry_t;

SLIST_HEAD(hook_head, hook_entry);

static struct hook_head hook_head = SLIST_HEAD_INITIALIZER(hook_head);
static hook_entry_t hook_entries[HW_BREAKPOINT_SLOTS];
static int hook_manager_initialized;
static size_t hook_manager_num_hooks;

static void hook_manager_free_entry(hook_entry_t *entry);

static void hook_clear(hook_t *hook)
{
    hook->target = 0;
    hook->replacement = 0;
    hook->gateway = 0;
    hook->kind = HOOK_NONE;
    hook->type = ALWAYS_ENABLED;
    hook->hwbp_slot = -1;
    hook->installed = 0;
}

static int hook_type_valid(hook_type_t type)
{
    return type == ALWAYS_ENABLED || type == ONESHOT;
}

static void hook_manager_remove_entry(hook_entry_t *entry)
{
    SLIST_REMOVE(&hook_head, entry, hook_entry, entries);
    hook_manager_num_hooks--;
    hook_manager_free_entry(entry);
}

static void hook_manager_reap_oneshots(void)
{
    hook_entry_t *entry = SLIST_FIRST(&hook_head);

    while (entry != 0)
    {
        hook_entry_t *next = SLIST_NEXT(entry, entries);

        if (entry->hook.type == ONESHOT && entry->hook.installed == 0)
            hook_manager_remove_entry(entry);

        entry = next;
    }
}

static hook_entry_t *hook_manager_alloc_entry(void)
{
    for (int i = 0; i < HW_BREAKPOINT_SLOTS; i++)
    {
        if (hook_entries[i].used != 0)
            continue;

        hook_entries[i].used = 1;
        hook_clear(&hook_entries[i].hook);
        return &hook_entries[i];
    }

    return 0;
}

static void hook_manager_free_entry(hook_entry_t *entry)
{
    hook_clear(&entry->hook);
    entry->used = 0;
}

static hook_entry_t *hook_manager_find_entry(void *target)
{
    hook_entry_t *entry;

    hook_manager_init();

    SLIST_FOREACH(entry, &hook_head, entries)
    {
        if (entry->hook.target == target)
            return entry;
    }

    return 0;
}

static int hook_hwbp_callback(int slot, trap_frame_t *frame, void *arg)
{
    hook_t *hook = (hook_t *)arg;
    int reenable;

    if (hook == 0 || hook->installed == 0 || hook->kind != HOOK_HWBP)
        return 0;
    if (frame->rip != (uint64_t)hook->target)
        return 0;

    reenable = hook->type != ONESHOT;
    hwbp_disable_slot_local(slot);

    if (hook->type == ONESHOT)
    {
        hwbp_clear_local(slot);
        hook->installed = 0;
        hook->kind = HOOK_NONE;
        hook->hwbp_slot = -1;
    }

    frame->rip = (uint64_t)hook->replacement;
    if (reenable != 0)
        hwbp_enable_slot_local(slot);
    return 1;
}

int hook_install_hwbp(hook_t *hook, void *target, void *replacement)
{
    int slot;
    int error;

    if (hook == 0 || target == 0 || replacement == 0)
        return -1;

    slot = hwbp_find_free();
    if (slot < 0)
        return -2;

    hook->target = target;
    hook->replacement = replacement;
    hook->gateway = target;
    hook->kind = HOOK_HWBP;
    hook->hwbp_slot = slot;
    hook->installed = 1;

    error = hwbp_set_all(slot, (uint64_t)target, HWBP_EXECUTE,
        HWBP_LEN_1, hook_hwbp_callback, hook);
    if (error != 0)
    {
        hook->installed = 0;
        hook->kind = HOOK_NONE;
        hwbp_clear_all_cpus(slot);
        return error;
    }

    return 0;
}

int hook_install(hook_t *hook, void *target, void *replacement)
{
    if (kernel_fwver < 0x500)
        return -3;

    return hook_install_hwbp(hook, target, replacement);
}

int hook_disable_current_cpu(hook_t *hook)
{
    if (hook == 0 || hook->installed == 0 || hook->kind != HOOK_HWBP)
        return -1;

    hwbp_disable_slot_local(hook->hwbp_slot);
    return 0;
}

int hook_enable_current_cpu(hook_t *hook)
{
    if (hook == 0 || hook->installed == 0 || hook->kind != HOOK_HWBP)
        return -1;

    return hwbp_enable_slot_local(hook->hwbp_slot);
}

int hook_remove(hook_t *hook)
{
    int error;

    if (hook == 0)
        return -1;
    if (hook->installed == 0)
    {
        hook->kind = HOOK_NONE;
        hook->hwbp_slot = -1;
        return 0;
    }
    if (hook->kind != HOOK_HWBP)
        return -1;

    error = hwbp_clear_all_cpus(hook->hwbp_slot);
    hook->installed = 0;
    hook->kind = HOOK_NONE;
    hook->hwbp_slot = -1;
    return error;
}

void hook_manager_init(void)
{
    if (hook_manager_initialized != 0)
        return;

    SLIST_INIT(&hook_head);
    hook_manager_initialized = 1;
}

hook_t *hook_manager_add(void *target, void *replacement, hook_type_t type)
{
    hook_entry_t *entry;
    int error;

    if (hook_type_valid(type) == 0)
        return 0;

    hook_manager_init();
    hook_manager_reap_oneshots();

    entry = hook_manager_find_entry(target);
    if (entry != 0)
        return &entry->hook;

    entry = hook_manager_alloc_entry();
    if (entry == 0)
        return 0;

    entry->hook.type = type;

    SLIST_INSERT_HEAD(&hook_head, entry, entries);
    hook_manager_num_hooks++;
    __sync_synchronize();

    error = hook_install(&entry->hook, target, replacement);
    if (error != 0)
    {
        SLIST_REMOVE(&hook_head, entry, hook_entry, entries);
        hook_manager_num_hooks--;
        hook_manager_free_entry(entry);
        __sync_synchronize();
        return 0;
    }

    return &entry->hook;
}

int hook_manager_remove(void *target)
{
    hook_entry_t *entry;
    int error;

    hook_manager_init();

    entry = hook_manager_find_entry(target);
    if (entry == 0)
        return -1;

    error = hook_remove(&entry->hook);
    if (error != 0)
        return error;

    hook_manager_remove_entry(entry);
    return 0;
}

hook_t *hook_manager_find(void *target)
{
    hook_entry_t *entry = hook_manager_find_entry(target);

    if (entry == 0)
        return 0;

    return &entry->hook;
}

int hook_manager_list(hook_t *hooks, size_t *len)
{
    hook_entry_t *entry;
    size_t copied = 0;
    size_t capacity;

    hook_manager_init();

    if (len == 0)
        return -1;

    if (hooks == 0)
    {
        *len = hook_manager_num_hooks;
        return 0;
    }

    capacity = *len;
    if (capacity < hook_manager_num_hooks)
    {
        *len = hook_manager_num_hooks;
        return -2;
    }

    SLIST_FOREACH(entry, &hook_head, entries)
    {
        hooks[copied++] = entry->hook;
    }

    *len = copied;
    return 0;
}
