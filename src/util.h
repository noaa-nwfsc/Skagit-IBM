#ifndef __FISH_UTIL_H
#define __FISH_UTIL_H

#include <random>

class GlobalRand {
public:
    static float unit_rand();
    static float unit_normal_rand();
    static int int_rand(int min, int max);

    static constexpr unsigned int USE_RANDOM_SEED = 0;
    static void reseed(unsigned int seed);
    static void reseed_random();

private:
    static std::default_random_engine generator;
    static std::uniform_real_distribution<float> unit_dist;
    static std::normal_distribution<float> normal_dist;
    static std::uniform_int_distribution<int> int_dist;
};

float unit_rand();
float unit_normal_rand();

unsigned sample(float *weights, unsigned weightsLen);

int poisson(double lambda);

double normal_pdf(double x, double mu, double sigma);

#endif