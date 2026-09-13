#include <gtest/gtest.h>
#include "drivers/BaseController.h"

// ControllerLib lives in namespace controllerlib. Pulled in here rather than at
// namespace scope in a header, so including a sys-con header does not drag the
// library into the global namespace of everything downstream.
using namespace controllerlib;


TEST(BaseController, test_normalize)
{
    //-32768, 32767
    EXPECT_FLOAT_EQ(BaseController::Normalize(-32768, -32768, 32767), -1.0f);
    EXPECT_FLOAT_EQ(BaseController::Normalize(0, -32768, 32767), 0.0f);
    EXPECT_FLOAT_EQ(BaseController::Normalize(32767, -32768, 32767), 1.0f);
    EXPECT_FLOAT_EQ(BaseController::Normalize(0, -32768, 32767), 0.0f);
    EXPECT_FLOAT_EQ(BaseController::Normalize(0, -32768, 32767), 0.0f);
    EXPECT_NEAR(BaseController::Normalize(-128, -32768, 32767), 0.0f, 0.01f);

    // 0, 255
    EXPECT_NEAR(BaseController::Normalize(128, 0, 255), 0.0f, 0.01f);
    EXPECT_FLOAT_EQ(BaseController::Normalize(0, 0, 255), -1.0f);
    EXPECT_NEAR(BaseController::Normalize(109, 0, 255), -0.14f, 0.01f);
    EXPECT_FLOAT_EQ(BaseController::Normalize(0, 0, 255), -1.0f);
}

TEST(BaseController, test_normalize_center)
{
    EXPECT_FLOAT_EQ(BaseController::Normalize(2000, 400, 3500, 2000), 0.0f);
    EXPECT_FLOAT_EQ(BaseController::Normalize(400, 400, 3500, 2000), -1.0f);
    EXPECT_FLOAT_EQ(BaseController::Normalize(3500, 400, 3500, 2000), 1.0f);
    EXPECT_FLOAT_EQ(BaseController::Normalize(1200, 400, 3500, 2000), -0.5f);
    EXPECT_FLOAT_EQ(BaseController::Normalize(2750, 400, 3500, 2000), 0.5f);
}
