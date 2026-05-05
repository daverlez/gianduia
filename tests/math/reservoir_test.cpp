#include <gtest/gtest.h>
#include <random>
#include "gianduia/math/reservoir.h"

using namespace gnd;

TEST(ReservoirTest, SingleCandidateSelection) {
    Reservoir<int> r;
    bool replaced = r.add(42, 10.0f, 2.0f, 0.5f);

    EXPECT_TRUE(replaced);
    EXPECT_EQ(r.sample, 42);
    EXPECT_EQ(r.M, 1);
    EXPECT_FLOAT_EQ(r.w_sum, 5.0f);
    EXPECT_FLOAT_EQ(r.getFinalWeight(), 0.5f);
}

TEST(ReservoirTest, DeterministicReplacement) {
    Reservoir<int> r;

    r.add(10, 2.0f, 1.0f, 0.9f);
    EXPECT_EQ(r.sample, 10);

    bool replaced1 = r.add(20, 8.0f, 1.0f, 0.9f);
    EXPECT_FALSE(replaced1);
    EXPECT_EQ(r.sample, 10);

    bool replaced2 = r.add(30, 10.0f, 1.0f, 0.1f);
    EXPECT_TRUE(replaced2);
    EXPECT_EQ(r.sample, 30);
}

TEST(ReservoirTest, StatisticalDistribution) {
    int M = 3;
    int counts[3] = {0, 0, 0};
    const int num_trials = 100000;
    
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int i = 0; i < num_trials; ++i) {
        Reservoir<int> r;
        r.add(0, 2.0f, 1.0f, dist(rng));
        r.add(1, 6.0f, 1.0f, dist(rng));
        r.add(2, 2.0f, 1.0f, dist(rng));
        counts[r.sample]++;
    }

    EXPECT_NEAR(counts[0] / static_cast<float>(num_trials), 0.20f, 0.01f);
    EXPECT_NEAR(counts[1] / static_cast<float>(num_trials), 0.60f, 0.01f);
    EXPECT_NEAR(counts[2] / static_cast<float>(num_trials), 0.20f, 0.01f);
}