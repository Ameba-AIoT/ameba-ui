/*
 * Copyright (c) 2021, Armink, <armink.ztl@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../../flashdb/inc/flashdb.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"
#include "vfs.h"
#include "ameba_soc.h"
#include "os_wrapper.h"
#define FDB_LOG_TAG "[main]"

static uint32_t boot_count = 0;
static time_t boot_time[10] = {0, 1, 2, 3};
rtos_sema_t s_lock;
/* default KV nodes */
static struct fdb_default_kv_node default_kv_table[] = {
    {"username", "armink", 0}, /* string KV */
    {"password", "123456", 0}, /* string KV */
    {"boot_count", &boot_count, sizeof(boot_count)}, /* int type KV */
    {"boot_time", &boot_time, sizeof(boot_time)},    /* int array type KV */
};

#ifdef FDB_USING_KVDB
/* KVDB object */
static struct fdb_kvdb kvdb = { 0 };
extern void kvdb_basic_sample(fdb_kvdb_t kvdb);
extern void kvdb_type_string_sample(fdb_kvdb_t kvdb);
extern void kvdb_type_blob_sample(fdb_kvdb_t kvdb);
#endif

#ifdef FDB_USING_TSDB
/* TSDB object */
struct fdb_tsdb tsdb = { 0 };
/* counts for simulated timestamp */
static int counts = 0;
extern void tsdb_sample(fdb_tsdb_t tsdb);
#endif

static void lock(void) {
    rtos_sema_take(s_lock, portMAX_DELAY);
}

static void unlock(void) {
    rtos_sema_give(s_lock);
}

#ifdef FDB_USING_TSDB
static fdb_time_t get_time(void) {
    counts = rtos_time_get_current_system_time_ms();
    return counts;
}
#endif

void flashdb_task(void *param) {
    (void)param;

    fdb_err_t result;
    bool file_mode = true;
    uint32_t sec_size = 4096, db_size = sec_size * 4;

#ifdef FDB_USING_KVDB
    {
        /* KVDB Sample */
        struct fdb_default_kv default_kv;

        default_kv.kvs = default_kv_table;
        default_kv.num = sizeof(default_kv_table) / sizeof(default_kv_table[0]);
        /* set the lock and unlock function if you want */
        rtos_sema_create(&s_lock, 1, 1);
        fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_LOCK, (void *)lock);
        fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_UNLOCK, (void *)unlock);
        /* set the sector and database max size */
        fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_SEC_SIZE, &sec_size);
        fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_MAX_SIZE, &db_size);
        /* enable file mode */
        fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_FILE_MODE, &file_mode);
        /* create database directory */
        mkdir("vfs:fdb_kvdb9", 0777);
        /* Key-Value database initialization
         *
         *       &kvdb: database object
         *       "env": database name
         * "fdb_kvdb1": The flash partition name base on FAL. Please make sure it's in FAL partition table.
         *              Please change to YOUR partition name.
         * &default_kv: The default KV nodes. It will auto add to KVDB when first initialize successfully.
         *  &kv_locker: The locker object.
         */
        result = fdb_kvdb_init(&kvdb, "env", "vfs:fdb_kvdb9", &default_kv, &s_lock);

        if (result != FDB_NO_ERR) {
            printf("fdb_kvdb_init result = -1\r\n");
        }

        /* run basic KV samples */
        kvdb_basic_sample(&kvdb);
        /* run string KV samples */
        kvdb_type_string_sample(&kvdb);
        /* run blob KV samples */
        kvdb_type_blob_sample(&kvdb);
    }
#endif /* FDB_USING_KVDB */

#ifdef FDB_USING_TSDB
    {
        /* TSDB Sample */
        /* set the lock and unlock function if you want */
        fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_SET_LOCK, (void *)lock);
        fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_SET_UNLOCK, (void *)unlock);
        /* set the sector and database max size */
        fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_SET_SEC_SIZE, &sec_size);
        fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_SET_MAX_SIZE, &db_size);
        /* enable file mode */
        fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_SET_FILE_MODE, &file_mode);
        /* create database directory */
        mkdir("vfs:fdb_tsdb4", 0777);
        /* Time series database initialization
         *
         *       &tsdb: database objectflash
         *       "log": database name
         * "fdb_tsdb1": The flash partition name base on FAL. Please make sure it's in FAL partition table.
         *              Please change to YOUR partition name.
         *    get_time: The get current timestamp function.
         *         128: maximum length of each log
         *   ts_locker: The locker object.
         */
        result = fdb_tsdb_init(&tsdb, "log", "vfs:fdb_tsdb4", get_time, 128, &s_lock);
        /* read last saved time for simulated timestamp */
        counts = rtos_time_get_current_system_time_ms();
        fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_GET_LAST_TIME, &counts);

        if (result != FDB_NO_ERR) {
            printf("fdb_tsdb_init result = -1\r\n");
        }

        /* run TSDB sample */
        tsdb_sample(&tsdb);
    }
#endif /* FDB_USING_TSDB */
}

u32 flashdb_test(u16 argc, u8  *argv[]) {
    UNUSED(argc);
    UNUSED(argv);
    if (rtos_task_create(NULL, ((const char *)"flashdb_task"), flashdb_task, NULL, 1024 * 4, 1) != RTK_SUCCESS) {
        printf("\n\r[%s] Create flashdb_task failed", __FUNCTION__);
        return FALSE;
    }
    return TRUE;
}



CMD_TABLE_DATA_SECTION
const COMMAND_TABLE cmd_table_flashdb_test[] = {
    {"flashdb_test", flashdb_test},
};

