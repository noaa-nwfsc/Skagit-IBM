#ifndef __FISH_UTIL_H
#define __FISH_UTIL_H

float unit_rand();
float unit_normal_rand();

unsigned sample(float *weights, unsigned weightsLen);

int poisson(double lambda);

double normal_pdf(double x, double mu, double sigma);

#endif