//
// Created by shiroko on 22-5-16.
//
#include <dev.h>
#include <driver/console.h>
#include <environment.h>
#include <lib/string.h>

dev_driver_t *devfs[MAX_DEV_ID];

int init_driver() {
    memset(devfs, 0, sizeof(dev_driver_t *) * MAX_DEV_ID);
    // build ordered list.
    // TODO: use level array to avoid search from head everytime
    list_head_t *head_driver = &env.driver_list_head;

    section_foreach_entry(DevDrivers, dev_driver_t *, drv) {
        int dlevel = (*drv)->loading_sequence;

        list_foreach_entry(head_driver, dev_driver_t, list, added) {
            if (added->loading_sequence >= dlevel) {
                list_add(&((*drv)->list), &(added->list));
                goto out;
            }
        }
        list_add(&((*drv)->list), head_driver);
    out:;
    }
    // init ordered list
    // TODO: non-return fault handle.
    list_foreach_entry(head_driver, dev_driver_t, list, drv) {
        assert(drv->dev_id < MAX_DEV_ID, "Driver's device id exceeded max id.");
        if (drv->dev_id) {
            if (devfs[drv->dev_id]) {
                kprintf("[DRV] Driver %s required an already existed dev id.");
                return 2;
            }
            if (drv->dev_id != 0)
                devfs[drv->dev_id] = drv;
        }
        if (drv->initialized)
            goto finish;
        int ret = 0;
        if ((ret = drv->init(drv)) != 0) {
            kprintf("[DRV] Driver %s cannot initialize.\n", drv->name);
            return 1;
        }
        drv->initialized = true;
    finish:
        kprintf("[DRV] Driver %s initialization complete.\n", drv->name);
    }
    return 0;
}

inode_ops_t *get_driver_inode_ops(uint8_t dev_id) {
    return devfs[dev_id]->dev_inode_ops;
}

file_ops_t *get_driver_file_ops(uint8_t dev_id) {
    return devfs[dev_id]->dev_file_ops;
}
