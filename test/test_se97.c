#include "unity.h"
#include "se97.h"

void test_se97_in_normal_mode_after_create(void)
{
    se97_t *sensor = se97_create (1, 0);
    TEST_ASSERT_EQUAL_HEX8(33, 42);
}

void test_se97_read_temp_max_reg_val_is_max_degrees_celsius(void)
{
    TEST_ASSERT_EQUAL_HEX8(33, 42);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_se97_read_temp_max_reg_val_is_max_degrees_celsius);
    return UNITY_END();
}
