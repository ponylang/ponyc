// g++ -Wall -O3 -march=native --std=c++11 n-body.cc -o n-body-cc

/* The Computer Language Benchmarks Game
   http://benchmarksgame.alioth.debian.org/

   contributed by Mark C. Lewis
   modified slightly by Chad Whipkey
   converted from java to c++,added sse support, by Branimir Maksimovic
   converted from c++ to c, by Alexey Medvedchikov
   converted from c to c++11, by Walter Landry
   modified by Dmitri Naumov
*/

#include <algorithm>
#include <stdio.h>
#include <cmath>
#include <stdlib.h>
#include <immintrin.h>
#include <array>

constexpr double PI(3.141592653589793);
constexpr double SOLAR_MASS ( 4 * PI * PI );
constexpr double DAYS_PER_YEAR(365.24);

struct body {
  double x[3], fill, v[3], mass;
  constexpr body(double x0, double x1, double x2, double v0, double v1, double v2,  double Mass):
    x{x0,x1,x2}, fill(0), v{v0,v1,v2}, mass(Mass) {}
};

class N_Body_System
{
  static std::array<body,5> bodies;

  void offset_momentum()
  {
    unsigned int k;
    for(auto &body: bodies)
      for(k = 0; k < 3; ++k)
        bodies[0].v[k] -= body.v[k] * body.mass / SOLAR_MASS;
  }

public:
  N_Body_System()
  {
    offset_momentum();
  }
  void advance(double dt)
  {
    constexpr unsigned int N = ((bodies.size() - 1) * bodies.size()) / 2;

    static double r[N][4];
    static double mag[N];

    unsigned int i, m;
    __m128d dx[3], dsquared, distance, dmag;

    i=0;
    for(auto bi(bodies.begin()); bi!=bodies.end(); ++bi)
      {
        auto bj(bi);
        for(++bj; bj!=bodies.end(); ++bj, ++i)
          for (m=0; m<3; ++m)
            r[i][m] = bi->x[m] - bj->x[m];
      }

    for (i=0; i<N; i+=2)
      {
        for (m=0; m<3; ++m)
          {
            dx[m] = _mm_loadl_pd(dx[m], &r[i][m]);
            dx[m] = _mm_loadh_pd(dx[m], &r[i+1][m]);
          }

        dsquared = dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2];
        distance = _mm_cvtps_pd(_mm_rsqrt_ps(_mm_cvtpd_ps(dsquared)));

        for (m=0; m<2; ++m)
          distance = distance * _mm_set1_pd(1.5)
            - ((_mm_set1_pd(0.5) * dsquared) * distance)
            * (distance * distance);

        dmag = _mm_set1_pd(dt) / (dsquared) * distance;
        _mm_store_pd(&mag[i], dmag);
      }

    i=0;
    for(auto bi(bodies.begin()); bi!=bodies.end(); ++bi)
      {
        auto bj(bi);
        for(++bj; bj!=bodies.end(); ++bj, ++i)
          for(m=0; m<3; ++m)
            {
              const double x = r[i][m] * mag[i];
              bi->v[m] -= x * bj->mass;
              bj->v[m] += x * bi->mass;
            }
      }

    for(auto &body: bodies)
      for(m=0; m<3; ++m)
        body.x[m] += dt * body.v[m];
  }

  double energy()
  {
    double e(0.0);
    for(auto bi(bodies.cbegin()); bi!=bodies.cend(); ++bi)
      {
        e += bi->mass * ( bi->v[0] * bi->v[0]
                          + bi->v[1] * bi->v[1]
                          + bi->v[2] * bi->v[2] ) / 2.;

        auto bj(bi);
        for(++bj; bj!=bodies.end(); ++bj)
          {
            double distance = 0;
            for(auto k=0; k<3; ++k)
            {
              const double dx = bi->x[k] - bj->x[k];
              distance += dx * dx;
            }

            e -= (bi->mass * bj->mass) / std::sqrt(distance);
          }
      }
    return e;
  }
};


std::array<body,5> N_Body_System::bodies{{
    /* sun */
    body(0., 0., 0. ,
         0., 0., 0. ,
         SOLAR_MASS),
    /* jupiter */
    body(4.84143144246472090e+00,
         -1.16032004402742839e+00,
         -1.03622044471123109e-01 ,
         1.66007664274403694e-03 * DAYS_PER_YEAR,
         7.69901118419740425e-03 * DAYS_PER_YEAR,
         -6.90460016972063023e-05 * DAYS_PER_YEAR ,
         9.54791938424326609e-04 * SOLAR_MASS
         ),
    /* saturn */
    body(8.34336671824457987e+00,
         4.12479856412430479e+00,
         -4.03523417114321381e-01 ,
         -2.76742510726862411e-03 * DAYS_PER_YEAR,
         4.99852801234917238e-03 * DAYS_PER_YEAR,
         2.30417297573763929e-05 * DAYS_PER_YEAR ,
         2.85885980666130812e-04 * SOLAR_MASS
         ),
    /* uranus */
    body(1.28943695621391310e+01,
         -1.51111514016986312e+01,
         -2.23307578892655734e-01 ,
         2.96460137564761618e-03 * DAYS_PER_YEAR,
         2.37847173959480950e-03 * DAYS_PER_YEAR,
         -2.96589568540237556e-05 * DAYS_PER_YEAR ,
         4.36624404335156298e-05 * SOLAR_MASS
         ),
    /* neptune */
    body(1.53796971148509165e+01,
         -2.59193146099879641e+01,
         1.79258772950371181e-01 ,
         2.68067772490389322e-03 * DAYS_PER_YEAR,
         1.62824170038242295e-03 * DAYS_PER_YEAR,
         -9.51592254519715870e-05 * DAYS_PER_YEAR ,
         5.15138902046611451e-05 * SOLAR_MASS
         )
  }};

int main(int , char** argv)
{
  int i, n = atoi(argv[1]);
  N_Body_System system;

  printf("%.9f\n", system.energy());
  for (i = 0; i < n; ++i)
    system.advance(0.01);
  printf("%.9f\n", system.energy());

  return 0;
}
