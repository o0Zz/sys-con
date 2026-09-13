#include <gtest/gtest.h>
#include "drivers/BaseController.h"

// ControllerLib lives in namespace controllerlib. Pulled in here rather than at
// namespace scope in a header, so including a sys-con header does not drag the
// library into the global namespace of everything downstream.
using namespace controllerlib;


TEST(BaseController, test_deadzone)
{
    // Deadzone 0%
    EXPECT_FLOAT_EQ(BaseController::ApplyDeadzone(0, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(BaseController::ApplyDeadzone(0, 0.2f), 0.2f);
    EXPECT_FLOAT_EQ(BaseController::ApplyDeadzone(0, 0.9f), 0.9f);
    EXPECT_FLOAT_EQ(BaseController::ApplyDeadzone(0, 1.0f), 1.0f);

    // Deadzone 10%
    EXPECT_FLOAT_EQ(BaseController::ApplyDeadzone(10, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(BaseController::ApplyDeadzone(10, 0.05f), 0.0f);
    EXPECT_FLOAT_EQ(BaseController::ApplyDeadzone(10, 0.1f), 0.0f);
    EXPECT_NEAR(BaseController::ApplyDeadzone(10, 0.11f), 0.01f, 0.01f);
    EXPECT_NEAR(BaseController::ApplyDeadzone(10, 0.2f), 0.11f, 0.01f);
    EXPECT_NEAR(BaseController::ApplyDeadzone(10, 0.9f), 0.88f, 0.01f);
    EXPECT_FLOAT_EQ(BaseController::ApplyDeadzone(10, 1.0f), 1.0f);
}
