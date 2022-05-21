//
// Created by shiroko on 22-5-16.
//

#ifndef __DEV_H__
#define __DEV_H__

#include <common/types.h>
#include <configs.h>
#include <lib/linklist.h>
#include <vfs.h>

typedef struct __dev_driver_t {
    const char *name;
    uint8_t     major_ver;
    uint8_t     minor_ver;
    uint8_t     dev_id; // reference major in vfs, 0 means no vfs interface
    uint8_t     loading_sequence; // 0xFF means not loading.
    bool        initialized;

    int (*init)(struct __dev_driver_t *driver);
    list_head_t list;

    void *private_data;

    inode_ops_t *dev_inode_ops;
    file_ops_t  *dev_file_ops;
} dev_driver_t;

#define ADD_DEV_DRIVER(driver)                                                 \
    static dev_driver_t *__ptr##driver                                         \
        __attribute__((used, section("DevDrivers"))) = &driver

int          init_driver();
inode_ops_t *get_driver_inode_ops(uint8_t dev_id);
file_ops_t  *get_driver_file_ops(uint8_t dev_id);

#endif // __DEV_H__