#pragma once

#include <ps5kld/kernel.h>

int device_available(void);
struct cdev *device_create(struct cdevsw *devsw, int unit, uid_t uid,
    gid_t gid, int perms, const char *name);
void device_destroy(struct cdev *dev);
