
#pragma once

#include <cmath>

// CIELAB to linear RGB conversions

// D65 reference values predivided by 100.
static constexpr double D65RefX = 95.0489 / 100.0;
static constexpr double D65RefY = 100.0 / 100.0;
static constexpr double D65RefZ = 108.884 / 100.0;


inline void LinearRGBtoLAB_D65(double inRed, double inGreen, double inBlue, double& outL, double& outA, double& outB)
{
    double x = (inRed * 0.4124564 + inGreen * 0.3575761 + inBlue * 0.1804375) / D65RefX;
    double y = (inRed * 0.2126729 + inGreen * 0.7151522 + inBlue * 0.0721750) / D65RefY;
    double z = (inRed * 0.0193339 + inGreen * 0.1191920 + inBlue * 0.9503041) / D65RefZ;


    x = (x > 0.008856) ? pow(x, 1.0 / 3.0) : (7.787 * x) + (16.0 / 116.0);
    y = (y > 0.008856) ? pow(y, 1.0 / 3.0) : (7.787 * y) + (16.0 / 116.0);
    z = (z > 0.008856) ? pow(z, 1.0 / 3.0) : (7.787 * z) + (16.0 / 116.0);

    outL = (116.0 * y) - 16.0;
    outA = 500.0 * (x - y);
    outB = 200.0 * (y - z);
}


inline void LABtoLinearRGB_D65(double inL, double inA, double inB, double& outRed, double& outGreen, double& outBlue)
{
    double y = (inL + 16.0) / 116.0;
    double x = inA / 500.0 + y;
    double z = y - inB / 200.0;


    x = ((x > 0.206897) ? pow(x, 3.0) : (x - 16.0 / 116.0) / 7.787);
    y = ((y > 0.206897) ? pow(y, 3.0) : (y - 16.0 / 116.0) / 7.787);
    z = ((z > 0.206897) ? pow(z, 3.0) : (z - 16.0 / 116.0) / 7.787);

    x *= D65RefX;
    y *= D65RefY;
    z *= D65RefZ;

    outRed = x * 3.2404542 + y * -1.5371385 + z * -0.4985314;
    outGreen = x * -0.9692660 + y * 1.8760108 + z * 0.0415560;
    outBlue = x * 0.0556434 + y * -0.2040259 + z * 1.0572252;
}