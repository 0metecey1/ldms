#include "unity.h"
#include "se97.h"

void test_se97_in_normal_mode_after_create(void)
{
    se97_t *sensor = se97_create (1, 0);
    TEST_ASSERT_EQUAL_HEX8(33, 42);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_se97_in_normal_mode_after_create);
    return UNITY_END();
}
