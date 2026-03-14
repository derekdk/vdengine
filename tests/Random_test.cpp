/**
 * @file Random_test.cpp
 * @brief Unit tests for Random.h (RandomStream)
 */

#include <vde/api/Random.h>

#include <gtest/gtest.h>

using namespace vde;

TEST(RandomStreamTest, DeterministicWithSameSeed) {
    RandomStream a(42);
    RandomStream b(42);

    for (int i = 0; i < 100; ++i) {
        EXPECT_FLOAT_EQ(a.unit(), b.unit()) << "Diverged at iteration " << i;
    }
}

TEST(RandomStreamTest, DifferentSeedsProduceDifferentResults) {
    RandomStream a(1);
    RandomStream b(2);

    // Very unlikely that 10 samples are all equal with different seeds
    bool anyDifferent = false;
    for (int i = 0; i < 10; ++i) {
        if (a.unit() != b.unit()) {
            anyDifferent = true;
            break;
        }
    }
    EXPECT_TRUE(anyDifferent);
}

TEST(RandomStreamTest, UnitInRange) {
    RandomStream rng(123);
    for (int i = 0; i < 1000; ++i) {
        float v = rng.unit();
        EXPECT_GE(v, 0.0f);
        EXPECT_LT(v, 1.0f);
    }
}

TEST(RandomStreamTest, RangeInBounds) {
    RandomStream rng(456);
    for (int i = 0; i < 1000; ++i) {
        float v = rng.range(-5.0f, 5.0f);
        EXPECT_GE(v, -5.0f);
        EXPECT_LE(v, 5.0f);
    }
}

TEST(RandomStreamTest, RangeIntInBounds) {
    RandomStream rng(789);
    for (int i = 0; i < 1000; ++i) {
        int v = rng.rangeInt(0, 10);
        EXPECT_GE(v, 0);
        EXPECT_LE(v, 10);
    }
}

TEST(RandomStreamTest, ChanceProbabilityZero) {
    RandomStream rng(100);
    for (int i = 0; i < 100; ++i) {
        EXPECT_FALSE(rng.chance(0.0f));
    }
}

TEST(RandomStreamTest, ChanceProbabilityOne) {
    RandomStream rng(100);
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(rng.chance(1.0f));
    }
}

TEST(RandomStreamTest, UnitDirection2DIsUnitLength) {
    RandomStream rng(200);
    for (int i = 0; i < 100; ++i) {
        auto d = rng.unitDirection2D();
        float len = glm::length(d);
        EXPECT_NEAR(len, 1.0f, 0.01f);
    }
}

TEST(RandomStreamTest, InsideBoundsStaysInBounds) {
    RandomStream rng(300);
    WorldBounds2D bounds(Meters(-10.0f), Meters(-5.0f), Meters(10.0f), Meters(5.0f));
    for (int i = 0; i < 1000; ++i) {
        auto pt = rng.inside(bounds);
        EXPECT_GE(pt.x, -10.0f);
        EXPECT_LE(pt.x, 10.0f);
        EXPECT_GE(pt.y, -5.0f);
        EXPECT_LE(pt.y, 5.0f);
    }
}

TEST(RandomStreamTest, ReseedRestoresDeterminism) {
    RandomStream rng(42);

    float first = rng.unit();

    rng.reseed(42);
    float second = rng.unit();

    EXPECT_FLOAT_EQ(first, second);
}

TEST(RandomStreamTest, SeedReturnsConstructedSeed) {
    RandomStream rng(999);
    EXPECT_EQ(rng.seed(), 999u);
}
