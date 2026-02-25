#pragma once

#include "IPlugParameter.h"

namespace transformations
{
class PseudoLogExpShape final : public iplug::IParam::Shape
{
public:
  explicit PseudoLogExpShape(double shape);

  iplug::IParam::Shape* Clone() const override;
  iplug::IParam::EDisplayType GetDisplayType() const override;
  double NormalizedToValue(double value, const iplug::IParam& param) const override;
  double ValueToNormalized(double value, const iplug::IParam& param) const override;

private:
  static double ApplyNormalizedExp(double normalized, double shape);
  static double ApplyNormalizedExpInverse(double scaledValue, double shape);

  double mShape = 0.0;
};

const iplug::IParam::Shape& GetGlobalPseudoLogShape();
const iplug::IParam::Shape& GetPortamentoPseudoLogShape();
} // namespace transformations
