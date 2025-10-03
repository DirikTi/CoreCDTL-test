#include "unity/unity.h"

void test_hk_init(void);
void test_hk_type_register(void);
void test_hk_set_field(void);
void test_hk_get_field(void);
void test_hk_get_list(void);
void test_hk_serialize(void);
void test_hk_deserialize(void);
void test_hk_field_exists(void);

void test_bus_init(void);
void test_bus_subscribe_and_publish(void);
void test_event_bus_get_list(void);

void test_scheduler_init(void);
void test_scheduler_every_ms(void);
void test_scheduler_after_ms(void);
void test_scheduler_cancel(void);
void test_scheduler_get_list(void);

int main(void) {
    UNITY_BEGIN();

    // Heapkit
    RUN_TEST(test_hk_init);
    RUN_TEST(test_hk_type_register);
    RUN_TEST(test_hk_set_field);
    RUN_TEST(test_hk_get_field);
    RUN_TEST(test_hk_get_list);
    RUN_TEST(test_hk_serialize);
    RUN_TEST(test_hk_deserialize);
    RUN_TEST(test_hk_field_exists);

    // Event bus
    RUN_TEST(test_bus_init);
    RUN_TEST(test_bus_subscribe_and_publish);
    RUN_TEST(test_event_bus_get_list);

    // Scheduler
    RUN_TEST(test_scheduler_init);
    RUN_TEST(test_scheduler_every_ms);
    RUN_TEST(test_scheduler_after_ms);
    RUN_TEST(test_scheduler_cancel);
    RUN_TEST(test_scheduler_get_list);

    return UNITY_END();
}
