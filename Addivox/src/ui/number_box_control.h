#pragma once

#include "IControls.h"

#include <algorithm>

namespace plugin_ui {
using namespace iplug;
using namespace igraphics;

class NumberBoxControl final : public IContainerBase {
public:
  NumberBoxControl(const IRECT& bounds, int paramIdx, const IVStyle& style = DEFAULT_STYLE, double defaultValue = 0.0, double minValue = 0.0,
                   double maxValue = 100.0, const char* fmtStr = "%0.0f")
      : IContainerBase(bounds, kNoParameter), mParamIdx(paramIdx), mStyle(style), mDefaultValue(defaultValue), mMinValue(minValue), mMaxValue(maxValue) {
    mFormatString.Set(fmtStr ? fmtStr : "%0.0f");
  }

  void SetTooltip(const char* tooltip) {
    IControl::SetTooltip(tooltip);
    mTooltip.Set(tooltip ? tooltip : "");

    if (mNumberBox) mNumberBox->SetTooltip(mTooltip.Get());
    if (mIncrementButton) mIncrementButton->SetTooltip(mTooltip.Get());
    if (mDecrementButton) mDecrementButton->SetTooltip(mTooltip.Get());
  }

  void OnAttached() override {
    mNumberBox = new SteppedNumberBoxControl(IRECT(), mParamIdx, nullptr, "", mStyle, false, mDefaultValue, mMinValue, mMaxValue, mFormatString.Get(), false);
    mNumberBox->SetDrawTriangle(false);

    mIncrementButton = new ArrowButtonControl(IRECT(), ArrowButtonControl::Direction::Up, mStyle);
    mIncrementButton->SetAnimationEndActionFunction([this](IControl*) { StepValue(+1.0); });

    mDecrementButton = new ArrowButtonControl(IRECT(), ArrowButtonControl::Direction::Down, mStyle);
    mDecrementButton->SetAnimationEndActionFunction([this](IControl*) { StepValue(-1.0); });

    AddChildControl(mNumberBox);
    AddChildControl(mIncrementButton);
    AddChildControl(mDecrementButton);

    if (mTooltip.GetLength() > 0) SetTooltip(mTooltip.Get());

    OnResize();
  }

  void OnResize() override {
    if (!mNumberBox || !mIncrementButton || !mDecrementButton) return;

    IRECT sections = mRECT;
    const float buttonColumnWidth = std::min(18.f, sections.W() * 0.35f);
    const IRECT buttonColumnBounds = sections.ReduceFromRight(buttonColumnWidth);
    const IRECT numberBounds = sections;

    mNumberBox->SetTargetAndDrawRECTs(numberBounds);
    mIncrementButton->SetTargetAndDrawRECTs(buttonColumnBounds.FracRectVertical(0.5f, true));
    mDecrementButton->SetTargetAndDrawRECTs(buttonColumnBounds.FracRectVertical(0.5f, false));
  }

private:
  class SteppedNumberBoxControl final : public IVNumberBoxControl {
  public:
    using IVNumberBoxControl::IVNumberBoxControl;

    void OnAttached() override {
      IVNumberBoxControl::OnAttached();
      SetText(mStyle.valueText);
      ApplyValueChange(true);
    }

    void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override {
      const double gearing = IsFineControl(mod, true) ? mSmallIncrement : mLargeIncrement;
      mRealValue -= static_cast<double>(dY) * gearing;
      ApplyValueChange();
    }

    void OnTextEntryCompletion(const char* str, int valIdx) override {
      mRealValue = str ? std::atof(str) : 0.0;
      ApplyValueChange();
    }

    void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override {
      const double gearing = IsFineControl(mod, true) ? mSmallIncrement : mLargeIncrement;
      const double increment = (d > 0.f ? 1.0 : -1.0) * gearing;
      mRealValue += increment;
      ApplyValueChange();
    }

    void SetValueFromDelegate(double value, int valIdx = 0) override {
      if (GetParam()) {
        mRealValue = GetParam()->FromNormalized(value);
        ApplyValueChange(true);
      }

      IControl::SetValueFromDelegate(value, valIdx);
    }

    void SetValueFromUserInput(double value, int valIdx = 0) override {
      if (GetParam()) {
        mRealValue = GetParam()->FromNormalized(value);
        ApplyValueChange(true);
      }

      IControl::SetValueFromUserInput(value, valIdx);
    }

    void StepValue(double direction) {
      const double step = GetParam() ? GetParam()->GetStep() : 1.0;
      const double snappedValue = GetParam() ? GetParam()->FromNormalized(GetParam()->ToNormalized(mRealValue)) : mRealValue;
      mRealValue = snappedValue + direction * step;
      ApplyValueChange();
    }

  private:
    void ApplyValueChange(bool preventAction = false) {
      mRealValue = Clip(mRealValue, mMinValue, mMaxValue);

      double displayValue = mRealValue;
      if (GetParam()) displayValue = GetParam()->FromNormalized(GetParam()->ToNormalized(mRealValue));

      if (displayValue == 0.0) displayValue = 0.0;

      if (displayValue == 0.0 && mFmtStr.GetLength() >= 2 && mFmtStr.Get()[0] == '%' && mFmtStr.Get()[1] == '+') {
        WDL_String unsignedZeroFormat;
        unsignedZeroFormat.SetFormatted(32, "%%%s", mFmtStr.Get() + 2);
        mTextReadout->SetStrFmt(32, unsignedZeroFormat.Get(), displayValue);
      } else {
        mTextReadout->SetStrFmt(32, mFmtStr.Get(), displayValue);
      }

      if (!preventAction && GetParam()) SetValue(GetParam()->ToNormalized(mRealValue));

      SetDirty(!preventAction);
    }
  };

  class ArrowButtonControl final : public IVButtonControl {
  public:
    enum class Direction { Up, Down };

    ArrowButtonControl(const IRECT& bounds, Direction direction, const IVStyle& style)
        : IVButtonControl(bounds, SplashClickActionFunc, "", style, false, false), mDirection(direction) {}

    void DrawWidget(IGraphics& g) override {
      IVButtonControl::DrawWidget(g);

      const IRECT arrowBounds = mRECT.GetPadded(-5.f);
      const IColor arrowColor = IsDisabled() ? GetColor(kSH) : GetColor(kX1);
      const float midX = arrowBounds.MW();
      const float padX = arrowBounds.W() * 0.2f;
      const float padY = arrowBounds.H() * 0.2f;

      if (mDirection == Direction::Up) {
        g.FillTriangle(arrowColor, midX, arrowBounds.T + padY, arrowBounds.R - padX, arrowBounds.B - padY, arrowBounds.L + padX, arrowBounds.B - padY);
      } else {
        g.FillTriangle(arrowColor, arrowBounds.L + padX, arrowBounds.T + padY, arrowBounds.R - padX, arrowBounds.T + padY, midX, arrowBounds.B - padY);
      }
    }

  private:
    Direction mDirection;
  };

  void StepValue(double direction) {
    if (!mNumberBox || mNumberBox->IsDisabled()) return;

    const bool hasParamBinding = mNumberBox->GetParam() && mNumberBox->GetDelegate();
    if (hasParamBinding) mNumberBox->GetDelegate()->BeginInformHostOfParamChangeFromUI(mNumberBox->GetParamIdx());

    mNumberBox->StepValue(direction);

    if (hasParamBinding) mNumberBox->GetDelegate()->EndInformHostOfParamChangeFromUI(mNumberBox->GetParamIdx());
  }

  int mParamIdx{kNoParameter};
  IVStyle mStyle;
  double mDefaultValue{0.0};
  double mMinValue{0.0};
  double mMaxValue{100.0};
  WDL_String mFormatString{};
  WDL_String mTooltip{};
  SteppedNumberBoxControl* mNumberBox{nullptr};
  ArrowButtonControl* mIncrementButton{nullptr};
  ArrowButtonControl* mDecrementButton{nullptr};
};

} // namespace plugin_ui
