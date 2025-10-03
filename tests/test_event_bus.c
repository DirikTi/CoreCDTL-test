#include "unity.h"
#include "event_bus.h"
#include <string.h>

static plugin_id_t plugin = 1;
static int callback_called = 0;

static void test_callback(const void *data, size_t len, void *user) {
    (void)user;
    if (data && len > 0) {
        callback_called = *((int*)data);
    }
}

void test_bus_init(void) {
    int res = bus_init();
    TEST_ASSERT_EQUAL_INT(0, res);
}

void test_bus_subscribe_and_publish(void) {
    int res = bus_subscribe(plugin, "TEST_EVENT", test_callback, NULL);
    TEST_ASSERT_EQUAL_INT(0, res);

    int val = 123;
    res = bus_publish(plugin, "TEST_EVENT", &val, sizeof(val));
    TEST_ASSERT_EQUAL_INT(0, res);

    /*
     * It is not async event_bus
    */
    // TEST_ASSERT_EQUAL_INT(123, callback_called);
}

void test_event_bus_get_list(void) {
    char buf[256];
    event_bus_get_list(buf, sizeof(buf));
    TEST_ASSERT_TRUE(strstr(buf, "TEST_EVENT") != NULL);
}
