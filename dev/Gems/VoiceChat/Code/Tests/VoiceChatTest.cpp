#include "VoiceChat_precompiled.h"

#include <AzTest/AzTest.h>

class VoiceChatTest
    : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST_F(VoiceChatTest, ExampleTest)
{
    ASSERT_TRUE(true);
}

AZ_UNIT_TEST_HOOK();
