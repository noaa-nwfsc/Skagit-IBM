#ifndef __FISH_UTIL_H
#define __FISH_UTIL_H

#include <random>

class GlobalRand {
public:
    static std::default_random_engine generator;
    static std::uniform_real_distribution<float> unit_dist;
    static std::normal_distribution<float> normal_dist;

    static float unit_rand();
    static float unit_normal_rand();

    static constexpr unsigned int USE_RANDOM_SEED = 0;
    static void reseed(unsigned int seed);
    static void reseed_random();
};

float unit_rand();
float unit_normal_rand();

unsigned sample(float *weights, unsigned weightsLen);

int poisson(double lambda);

double normal_pdf(double x, double mu, double sigma);

#endif