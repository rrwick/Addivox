#pragma once

#include "IControls.h"
#include "theme.h"

namespace plugin_ui {
using namespace iplug;
using namespace igraphics;

namespace layout {
class AboutBuiltWithControl final : public IControl {
public:
  explicit AboutBuiltWithControl(const IRECT& bounds, const char* url) : IControl(bounds), mURL(url ? url : "") { mIgnoreMouse = false; }

  void Draw(IGraphics& g) override {
    UpdateLayout();

    const IText labelText = theme::AboutMetaText().WithAlign(EAlign::Near);
    IText linkText = theme::AboutLinkText().WithAlign(EAlign::Near);
    linkText.mFGColor = mMouseIsOver ? COLOR_WHITE : colour::ui::kAccentPrimary;

    g.DrawText(labelText, kLabelText, mLabelRECT, &mBlend);
    g.DrawText(linkText, kLinkText, mLinkRECT, &mBlend);

    IRECT linkTextBounds;
    g.MeasureText(linkText, kLinkText, linkTextBounds);
    const float underlineY = mLinkRECT.MH() + linkTextBounds.B;
    g.DrawLine(linkText.mFGColor, mLinkRECT.L, underlineY, mLinkRECT.L + linkTextBounds.W(), underlineY, &mBlend);
  }

  bool IsHit(float x, float y) const override {
    UpdateLayout();
    return mLinkHitRECT.Contains(x, y);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override {
    if (!IsHit(x, y) || !GetUI()) return;

    GetUI()->OpenURL(mURL);
    GetUI()->ReleaseMouseCapture();
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override {
    GetUI()->SetMouseCursor(ECursor::HAND);
    IControl::OnMouseOver(x, y, mod);
  }

  void OnMouseOut() override {
    GetUI()->SetMouseCursor();
    IControl::OnMouseOut();
  }

  void OnResize() override { mLayoutValid = false; }

private:
  void UpdateLayout() const {
    if (mLayoutValid || !GetUI()) return;

    const IText rowText = theme::AboutMetaText().WithAlign(EAlign::Near);
    const IText labelText = theme::AboutMetaText().WithAlign(EAlign::Near);
    const IText linkText = theme::AboutLinkText().WithAlign(EAlign::Near);

    IRECT rowBounds;
    IRECT labelBounds;
    IRECT linkBoundsInRow;
    IRECT linkBounds;
    GetUI()->MeasureText(rowText, kFullText, rowBounds);
    GetUI()->MeasureText(labelText, kLabelText, labelBounds);
    GetUI()->MeasureText(rowText, kLinkText, linkBoundsInRow);
    GetUI()->MeasureText(linkText, kLinkText, linkBounds);

    float spaceWidth = rowBounds.W() - labelBounds.W() - linkBoundsInRow.W();
    if (spaceWidth < 0.f) spaceWidth = 0.f;

    const float rowWidth = labelBounds.W() + spaceWidth + linkBounds.W();
    const float left = mRECT.MW() - (rowWidth * 0.5f);

    mLabelRECT = IRECT::MakeXYWH(left, mRECT.T, labelBounds.W(), mRECT.H());
    mLinkRECT = IRECT::MakeXYWH(left + labelBounds.W() + spaceWidth, mRECT.T, linkBounds.W(), mRECT.H());
    mLinkHitRECT = mLinkRECT;
    mLayoutValid = true;
  }

  static constexpr const char* kLabelText = "built with";
  static constexpr const char* kLinkText = "iPlug2";
  static constexpr const char* kFullText = "built with iPlug2";

  const char* mURL = "";
  mutable bool mLayoutValid = false;
  mutable IRECT mLabelRECT;
  mutable IRECT mLinkRECT;
  mutable IRECT mLinkHitRECT;
};
} // namespace layout
} // namespace plugin_ui
