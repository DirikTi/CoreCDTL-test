#include "unity.h"
#include "heapkit.h"
#include <string.h>

static plugin_id_t plugin_id = 1;
static type_info_t test_type;
static field_info_t test_fields[2];

#define TEST_FIELD_NAME "field1"

void test_hk_init(void) {
    int res = hk_init();
    TEST_ASSERT_EQUAL_INT(0, res);
}

void test_hk_type_register(void) {
    test_fields[0].name = TEST_FIELD_NAME;
    test_fields[0].type = TYPE_INT;
    test_fields[0].offset = 0;
    test_fields[0].size = sizeof(int);

    test_fields[1].name = "field2";
    test_fields[1].type = TYPE_FLOAT;
    test_fields[1].offset = sizeof(int);
    test_fields[1].size = sizeof(float);

    test_type.name = "MyType";
    test_type.size = sizeof(int) + sizeof(float);
    test_type.field_count = 2;
    test_type.fields = test_fields;
    test_type.is_simple = false;
    test_type.base_type = TYPE_STRUCT;

    int res = hk_type_register(plugin_id, &test_type);
    TEST_ASSERT_EQUAL_INT(0, res);

    res = hk_type_register(plugin_id, &test_type);
    TEST_ASSERT_EQUAL_INT(1, res);
}

void test_hk_set_field(void) {
    int value = 42;
    int res = hk_set_field(plugin_id, "MyType", TEST_FIELD_NAME, &value);
    TEST_ASSERT_EQUAL_INT(0, res);
}

void test_hk_get_field(void) {
    int value = 0;
    int res = hk_get_field(plugin_id, "MyType", TEST_FIELD_NAME, &value);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_INT(42, value);
}

void test_hk_get_list(void) {
    char buffer[256];
    int res = hk_get_list(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_TRUE(strstr(buffer, "MyType") != NULL);
}

void test_hk_serialize(void) {
    char buffer[256];
    int res = hk_serialize(plugin_id, "MyType", buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_TRUE(strlen(buffer) > 0);
}

void test_hk_deserialize(void) {
    const char* data = "{\"myField\":42}";
    int res = hk_deserialize(plugin_id, "MyType", data);
    TEST_ASSERT_EQUAL_INT(0, res);
}

void test_hk_field_exists(void) {
    bool exists = hk_field_exists(plugin_id, "MyType", TEST_FIELD_NAME);
    TEST_ASSERT_TRUE(exists);
}
