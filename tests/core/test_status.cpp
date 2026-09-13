/*
    Status is the one error type ControllerLib returns. These tests pin the two properties
    that matter: every value has a name (failures used to be logged as bare integers), and
    the numeric values are unchanged from the CONTROLLER_STATUS_* enum they replaced.
*/
#include <gtest/gtest.h>

#include "Status.h"

#include <cstring>
#include <set>
#include <string>

// ControllerLib lives in namespace controllerlib. Pulled in here rather than at
// namespace scope in a header, so including a sys-con header does not drag the
// library into the global namespace of everything downstream.
using namespace controllerlib;

TEST(Status, test_succeeded_and_failed_are_opposites)
{
    EXPECT_TRUE(Succeeded(Status::Success));
    EXPECT_FALSE(Failed(Status::Success));

    for (Status s : {Status::InvalidEndpoint, Status::Timeout, Status::NothingTodo, Status::UnknownError})
    {
        EXPECT_FALSE(Succeeded(s));
        EXPECT_TRUE(Failed(s));
    }
}

TEST(Status, test_every_value_has_a_distinct_name)
{
    const Status all[] = {
        Status::Success, Status::InvalidEndpoint, Status::BufferEmpty, Status::NothingTodo,
        Status::NotImplemented, Status::UnexpectedData, Status::InvalidArgument,
        Status::InvalidReportDescriptor, Status::HidIsNotJoystick, Status::NoInterfaces,
        Status::NoDataAvailable, Status::OutOfMemory, Status::UsbInterfaceAcquire,
        Status::OpenFailed, Status::WriteFailed, Status::ReadFailed, Status::Timeout,
        Status::UsbEndpointOpen, Status::InvalidIndex, Status::UnknownError};

    std::set<std::string> names;
    for (Status s : all)
    {
        const char *name = ToString(s);
        ASSERT_NE(name, nullptr);
        // A value missing from the switch falls through to the placeholder.
        EXPECT_STRNE(name, "Status(?)") << "unnamed Status value " << static_cast<int>(s);
        names.insert(name);
    }

    EXPECT_EQ(names.size(), sizeof(all) / sizeof(all[0])) << "two Status values share a name";
}

TEST(Status, test_numeric_values_are_unchanged)
{
    // These were the CONTROLLER_STATUS_* values. Nothing persists them, but keeping them
    // identical is what makes the rename a pure refactor.
    EXPECT_EQ(static_cast<int>(Status::Success), 0);
    EXPECT_EQ(static_cast<int>(Status::InvalidEndpoint), 100);
    EXPECT_EQ(static_cast<int>(Status::NothingTodo), 102);
    EXPECT_EQ(static_cast<int>(Status::Timeout), 115);
    EXPECT_EQ(static_cast<int>(Status::InvalidIndex), 117);
    EXPECT_EQ(static_cast<int>(Status::UnknownError), 255);
}
