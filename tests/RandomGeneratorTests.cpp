#include "RandomGenerator.h"

#include <algorithm>
#include <gtest/gtest.h>

TEST( RandomGeneratorTests, SlerpAtStartEqualsStart )
{
    float start   = 1.5f;
    float end     = 5.0f;
    float current = 0.0f;
    EXPECT_NEAR( RandomGenerator::Slerp( current, start, end ), start, 1e-4f );
}

TEST( RandomGeneratorTests, SlerpAtEndEqualsEnd )
{
    float start   = -2.0f;
    float end     = 3.25f;
    float current = 1.0f;
    EXPECT_NEAR( RandomGenerator::Slerp( current, start, end ), end, 1e-4f );
}

TEST( RandomGeneratorTests, SlerpMidpointWithinRange )
{
    float start   = -10.0f;
    float end     = 10.0f;
    float current = 0.5f;
    float value   = RandomGenerator::Slerp( current, start, end );
    // Not strictly linear; just ensure it lies within bounds.
    EXPECT_GE( value, std::min( start, end ) );
    EXPECT_LE( value, std::max( start, end ) );
}
