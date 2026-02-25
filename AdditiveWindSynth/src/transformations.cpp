#include "transformations.h"

#include <algorithm>
#include <cmath>

namespace
{
class PseudoLogBase2Shape final : public iplug::IParam::Shape
{
public:
  iplug::IParam::Shape* Clone() const override
  {
    return new PseudoLogBase2Shape(*this);
  }

  iplug::IParam::EDisplayType GetDisplayType() const override
  {
    return iplug::IParam::kDisplayLog;
  }

  double NormalizedToValue(double value, const iplug::IParam& param) const override
  {
    const double n = std::clamp(value, 0.0, 1.0);
    const double minValue = param.GetMin();
    const double maxValue = param.GetMax();
    constexpr double centerValue = 1.0;
    constexpr double kCurve = 4.0; // Higher = flatter near center.
    const double curveDenominator = std::exp2(kCurve) - 1.0;

    if(n <= 0.5)
    {
      const double u = n * 2.0;
      const double eased = (std::exp2(kCurve) - std::exp2(kCurve * (1.0 - u))) / curveDenominator;
      return minValue + (centerValue - minValue) * eased;
    }

    const double u = (n - 0.5) * 2.0;
    const double eased = (std::exp2(kCurve * u) - 1.0) / curveDenominator;
    const double upperSpan = std::log2(maxValue / centerValue);
    return centerValue * std::exp2(upperSpan * eased);
  }

  double ValueToNormalized(double value, const iplug::IParam& param) const override
  {
    const double constrainedValue = std::clamp(value, param.GetMin(), param.GetMax());
    const double minValue = param.GetMin();
    const double maxValue = param.GetMax();
    constexpr double centerValue = 1.0;
    constexpr double kCurve = 6.0; // Keep in sync with NormalizedToValue().
    const double curvePow = std::exp2(kCurve);
    const double curveDenominator = curvePow - 1.0;

    if(constrainedValue <= centerValue)
    {
      if(constrainedValue <= minValue)
        return 0.0;

      const double lowerSpan = centerValue - minValue;
      const double eased = (constrainedValue - minValue) / lowerSpan;
      const double inside = std::clamp(curvePow - eased * curveDenominator, 1.0, curvePow);
      const double u = 1.0 - (std::log2(inside) / kCurve);
      return 0.5 * u;
    }

    const double upperSpan = std::log2(maxValue / centerValue);
    const double eased = std::log2(constrainedValue / centerValue) / upperSpan;
    const double inside = std::clamp(1.0 + eased * curveDenominator, 1.0, curvePow);
    const double u = std::log2(inside) / kCurve;
    return 0.5 + 0.5 * u;
  }
};
} // namespace

const iplug::IParam::Shape& transformations::GetPseudoLogBase2Shape()
{
  static const PseudoLogBase2Shape shape{};
  return shape;
}
