#include "util.h"
#include <random>
#include <cmath>

std::default_random_engine generator;
std::uniform_real_distribution<float> unit_dist;
std::normal_distribution<float> normal_dist;

float unit_rand() {
  return unit_dist(generator);
}

float unit_normal_rand() {
  return normal_dist(generator);
}

unsigned sample(float *weights, unsigned weightsLen) {
  unsigned i = 0;
  float acc = 0;
  float r = unit_rand();
  while (i < weightsLen) {
    acc += weights[i];
    if (acc > r) {
      return i;
    }
    ++i;
  }
  return weightsLen - 1;
}


// from numpy
double logfactorial(int64_t k)
{
  const double halfln2pi = 0.9189385332046728;

  /*
    *  Use the Stirling series, truncated at the 1/k**3 term.
    *  (In a Python implementation of this approximation, the result
    *  was within 2 ULP of the best 64 bit floating point value for
    *  k up to 10000000.)
    */
  return (k + 0.5)*log(k) - k + (halfln2pi + (1.0/k)*(1/12.0 - 1/(360.0*k*k)));
}

// from numpy
#define LS2PI 0.91893853320467267
#define TWELFTH 0.083333333333333333333333
int poisson(double lam) {
  int k;
  double U, V, slam, loglam, a, b, invalpha, vr, us;

  slam = sqrt(lam);
  loglam = log(lam);
  b = 0.931 + 2.53 * slam;
  a = -0.059 + 0.02483 * b;
  invalpha = 1.1239 + 1.1328 / (b - 3.4);
  vr = 0.9277 - 3.6224 / (b - 2);

  while (1) {
    U = unit_rand() - 0.5;
    V = unit_rand();
    us = 0.5 - fabs(U);
    k = (int)floor((2 * a / us + b) * U + lam + 0.43);
    if ((us >= 0.07) && (V <= vr)) {
      return k;
    }
    if ((k < 0) || ((us < 0.013) && (V > us))) {
      continue;
    }
    /* log(V) == log(0.0) ok here */
    /* if U==0.0 so that us==0.0, log is ok since always returns */
    if ((log(V) + log(invalpha) - log(a / (us * us) + b)) <=
        (-lam + k * loglam - logfactorial(k))) {
      return k;
    }
  }
}

double normal_pdf(double x, double mu, double sigma) {
  double xm = x - mu;
  return exp(-(xm*xm)/(2.0*sigma*sigma))/sqrt(2.0*M_PI*sigma*sigma);
}