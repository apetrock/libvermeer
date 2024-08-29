#pragma once
#include "common.h"

#include <iostream>

namespace lewitt
{
  namespace color
  {
    GLM_TYPEDEFS;
    enum preset
    {
      grey,
      red,
      green,
      blue,
      sdf,
      rainbow,
    };

    inline vec3 rainbow(float d)
    {
      double r = 0.5 + 0.5 * cos(2.0 * M_PI * (d + 0.000));
      double g = 0.5 + 0.5 * cos(2.0 * M_PI * (d + 0.333));
      double b = 0.5 + 0.5 * cos(2.0 * M_PI * (d + 0.666));
      return vec3(r, g, b);
    }

    inline vec3 sdf(float d)
    {
      vec3 inside(0.0, 1.0, 0.0, 1.0);
      vec3 outside(1.0, 0.0, 0.0, 1.0);
      return d < 0 ? fabs(d) * inside : fabs(d) * outside;
    }

    vec3 _(preset p, float val = 1.0){
      switch(p) {
        case grey: return vec3(0.5, 0.5, 0.5);
        case red: return vec3(1.0, 0.0, 0.0);
        case green: return vec3(0.0, 1.0, 0.0);
        case blue: return vec3(0.0, 0.0, 1.0);
        case rainbow: return rainbow(val);
        case sdf: return sdf(val);
        case default: return vec3(1.0,1.0,1.0);
      }
    }
  }
}