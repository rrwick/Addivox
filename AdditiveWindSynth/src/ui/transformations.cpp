#include "transformations.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr double kShapeEpsilon = 1.0e-9;

// https://www.desmos.com/calculator/62op06fbef
// 2*ln(99) = 9.19, puts 1 right in the middle of 0-100
// 2*ln(9) = 4.39, puts 1 right in the middle of 0-10
constexpr double kGlobalPseudoLogShape = 9.19023970026918;
constexpr double kPortamentoPseudoLogShape = 9.19023970026918;
constexpr double kGainPseudoLogShape = 4.394449154672439;
} // namespace

transformations::PseudoLogExpShape::PseudoLogExpShape(double shape)
: mShape(shape)
{
}

double transformations::NormalizedSquareRoot(double normalized)
{
  return std::sqrt(std::clamp(normalized, 0.0, 1.0));
}

double transformations::NormalizedSquareRootInverse(double scaledValue)
{
  const double y = std::clamp(scaledValue, 0.0, 1.0);
  return y * y;
}

double transformations::SignedNormalizedSquareRoot(double normalized)
{
  const double x = std::clamp(normalized, -1.0, 1.0);
  return std::copysign(NormalizedSquareRoot(std::fabs(x)), x);
}

double transformations::SignedNormalizedSquareRootInverse(double scaledValue)
{
  const double y = std::clamp(scaledValue, -1.0, 1.0);
  return std::copysign(NormalizedSquareRootInverse(std::fabs(y)), y);
}

iplug::IParam::Shape* transformations::PseudoLogExpShape::Clone() const
{
  return new PseudoLogExpShape(*this);
}

iplug::IParam::EDisplayType transformations::PseudoLogExpShape::GetDisplayType() const
{
  return iplug::IParam::kDisplayLog;
}

double transformations::PseudoLogExpShape::NormalizedToValue(double value, const iplug::IParam& param) const
{
  const double minValue = param.GetMin();
  const double maxValue = param.GetMax();

  if(maxValue <= minValue)
    return minValue;

  const double shaped = NormalizedExp(value, mShape);
  return minValue + (maxValue - minValue) * shaped;
}

double transformations::PseudoLogExpShape::ValueToNormalized(double value, const iplug::IParam& param) const
{
  const double minValue = param.GetMin();
  const double maxValue = param.GetMax();

  if(maxValue <= minValue)
    return 0.0;

  const double constrained = std::clamp(value, minValue, maxValue);
  const double normalizedValue = (constrained - minValue) / (maxValue - minValue);
  return NormalizedExpInverse(normalizedValue, mShape);
}

double transformations::NormalizedExp(double normalized, double shape)
{
  const double x = std::clamp(normalized, 0.0, 1.0);

  if(std::abs(shape) <= kShapeEpsilon)
    return x;

  const double denominator = std::expm1(shape);
  if(std::abs(denominator) <= kShapeEpsilon)
    return x;

  const double y = std::expm1(shape * x) / denominator;
  return std::clamp(y, 0.0, 1.0);
}

double transformations::NormalizedExpInverse(double scaledValue, double shape)
{
  const double y = std::clamp(scaledValue, 0.0, 1.0);

  if(std::abs(shape) <= kShapeEpsilon)
    return y;

  const double denominator = std::expm1(shape);
  if(std::abs(denominator) <= kShapeEpsilon)
    return y;

  const double safeLogInput = std::max(y * denominator, -1.0 + std::numeric_limits<double>::epsilon());
  const double x = std::log1p(safeLogInput) / shape;
  return std::clamp(x, 0.0, 1.0);
}

double transformations::SignedNormalizedExp(double normalized, double shape)
{
  const double x = std::clamp(normalized, -1.0, 1.0);
  return std::copysign(NormalizedExp(std::fabs(x), shape), x);
}

double transformations::SignedNormalizedExpInverse(double scaledValue, double shape)
{
  const double y = std::clamp(scaledValue, -1.0, 1.0);
  return std::copysign(NormalizedExpInverse(std::fabs(y), shape), y);
}

double transformations::GetGlobalPseudoLogShapeValue()
{
  return kGlobalPseudoLogShape;
}

const iplug::IParam::Shape& transformations::GetGlobalPseudoLogShape()
{
  static const PseudoLogExpShape shape{kGlobalPseudoLogShape};
  return shape;
}

const iplug::IParam::Shape& transformations::GetPortamentoPseudoLogShape()
{
  static const PseudoLogExpShape shape{kPortamentoPseudoLogShape};
  return shape;
}

const iplug::IParam::Shape& transformations::GetGainPseudoLogShape()
{
  static const PseudoLogExpShape shape{kGainPseudoLogShape};
  return shape;
}
