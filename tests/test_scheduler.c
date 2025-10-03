#include "unity.h"
#include "scheduler.h"
#include <string.h>

static plugin_id_t plugin = 1;
static int cb_called = 0;

// --- Callback örneği ---
static void test_sched_cb(const void *data, size_t len, void *user) {
    (void)data; (void)user; (void)len;
    cb_called = 1;
}

// --- Tests ---

void test_scheduler_init(void) {
    int res = scheduler_init();
    TEST_ASSERT_EQUAL_INT(0, res);
}

void test_scheduler_every_ms(void) {
    int res = scheduler_every_ms(plugin, 10, test_sched_cb, NULL);
    TEST_ASSERT(res >= 0); // id dönmeli
}

void test_scheduler_after_ms(void) {
    int res = scheduler_after_ms(plugin, 20, test_sched_cb, NULL);
    TEST_ASSERT(res >= 0); // id dönmeli
}

void test_scheduler_cancel(void) {
    int id = scheduler_after_ms(plugin, 20, test_sched_cb, NULL);
    TEST_ASSERT(id >= 0);

    int res = scheduler_cancel(plugin, id);
    TEST_ASSERT_EQUAL_INT(0, res);
}

void test_scheduler_get_list(void) {
    char buf[256];
    int res = scheduler_get_list(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(0, res);
}
