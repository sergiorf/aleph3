#pragma once
#include <cmath>

namespace mathix {
    inline double csc(double x)  { return 1.0 / std::sin(x); }
    inline double sec(double x)  { return 1.0 / std::cos(x); }
    inline double coth(double x) { return 1.0 / std::tanh(x); }
    inline double sech(double x) { return 1.0 / std::cosh(x); }
    inline double csch(double x) { return 1.0 / std::sinh(x); }
    inline double cot(double x)  { return 1.0 / std::tan(x); }
}