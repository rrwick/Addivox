#pragma once

#include "IControls.h"

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

// Knob that keeps a static SVG layer while rotating only the indicator layer.
class LayeredSVGKnobControl : public IKnobControlBase
{
public:
  LayeredSVGKnobControl(const IRECT& bounds,
                        const ISVG& fixedSVG,
                        const ISVG& rotatingSVG,
                        int paramIdx = kNoParameter,
                        float startAngle = -135.f,
                        float endAngle = 135.f,
                        EDirection direction = EDirection::Vertical,
                        double gearing = DEFAULT_GEARING)
  : IKnobControlBase(bounds, paramIdx, direction, gearing)
  , mFixedSVG(fixedSVG)
  , mRotatingSVG(rotatingSVG)
  , mStartAngle(startAngle)
  , mEndAngle(endAngle)
  {
  }

  void Draw(IGraphics& g) override
  {
    g.DrawSVG(mFixedSVG, mRECT, &mBlend);
    g.DrawRotatedSVG(mRotatingSVG, mRECT.MW(), mRECT.MH(), mRECT.W(), mRECT.H(), AngleForValue(), &mBlend);
  }

  void SetFixedSVG(const ISVG& fixedSVG)
  {
    mFixedSVG = fixedSVG;
    SetDirty(false);
  }

  void SetRotatingSVG(const ISVG& rotatingSVG)
  {
    mRotatingSVG = rotatingSVG;
    SetDirty(false);
  }

  void SetAngleRange(float startAngle, float endAngle)
  {
    mStartAngle = startAngle;
    mEndAngle = endAngle;
    SetDirty(false);
  }

private:
  float AngleForValue() const
  {
    return mStartAngle + static_cast<float>(GetValue()) * (mEndAngle - mStartAngle);
  }

  ISVG mFixedSVG;
  ISVG mRotatingSVG;
  float mStartAngle = -135.f;
  float mEndAngle = 135.f;
};
} // namespace plugin_ui
