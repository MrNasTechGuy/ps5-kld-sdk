#include <ps5kld/device.h>

int device_available(void)
{
    return kmake_dev != 0 && kdestroy_dev != 0;
}

struct cdev *device_create(struct cdevsw *devsw, int unit, uid_t uid,
    gid_t gid, int perms, const char *name)
{
    if (kmake_dev == 0 || devsw == 0 || name == 0)
        return 0;

    return kmake_dev(devsw, unit, uid, gid, perms, "%s", name);
}

void device_destroy(struct cdev *dev)
{
    if (kdestroy_dev == 0 || dev == 0)
        return;

    kdestroy_dev(dev);
}
