#include "plugin.h"
#include "IPlug_include_in_plug_src.h"
#include "ui_layout.h"

#include <algorithm>
#include <cmath>

namespace
{
struct PseudoLogBase2Shape final : IParam::Shape
{
  Shape* Clone() const override
  {
    return new PseudoLogBase2Shape(*this);
  }

  IParam::EDisplayType GetDisplayType() const override
  {
    return IParam::kDisplayLog;
  }

  double NormalizedToValue(double value, const IParam& param) const override
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

  double ValueToNormalized(double value, const IParam& param) const override
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

const PseudoLogBase2Shape kPseudoLogBase2Shape{};
}

AdditiveWindSynth::AdditiveWindSynth(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamGain)->InitDouble("Gain", 100., 0., 100.0, 0.01, "%");

  const auto initPseudoLogScale = [this](int paramIdx, const char* name, double defaultValue = 1.0) {
    GetParam(paramIdx)->InitDouble(name, defaultValue, 0., 100., 0.01, "x", 0, "", kPseudoLogBase2Shape);
  };
  initPseudoLogScale(kParamGlobalAttackScale, "Global Attack");
  initPseudoLogScale(kParamGlobalReleaseScale, "Global Release");
  GetParam(kParamGlobalPitchShift)->InitDouble("Global Pitch Shift", 0., -50., 50., 0.01, "cents");
  GetParam(kParamGlobalPanShift)->InitDouble("Global Pan Shift", 0., -1., 1., 0.01);
  initPseudoLogScale(kParamGlobalIntensityVariationAmplitudeScale, "Global Intensity Variation Amount");
  initPseudoLogScale(kParamGlobalIntensityVariationRateScale, "Global Intensity Variation Rate");
  initPseudoLogScale(kParamGlobalPitchVariationAmplitudeScale, "Global Pitch Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalPitchVariationRateScale, "Global Pitch Variation Rate");
  initPseudoLogScale(kParamGlobalPanVariationAmplitudeScale, "Global Pan Variation Amount");
  initPseudoLogScale(kParamGlobalPanVariationRateScale, "Global Pan Variation Rate");
    
#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };
  
  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(plugin_ui::colour::ui::kPanelBackground);
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableMultiTouch(true);
    
#ifdef OS_WEB
    pGraphics->AttachPopupMenuControl();
#endif

    // pGraphics->EnableLiveEdit(true);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    const std::array<int, 10> globalModifierParamIdxs{{
      kParamGlobalAttackScale,
      kParamGlobalPitchShift,
      kParamGlobalIntensityVariationAmplitudeScale,
      kParamGlobalPitchVariationAmplitudeScale,
      kParamGlobalPanVariationAmplitudeScale,
      kParamGlobalReleaseScale,
      kParamGlobalPanShift,
      kParamGlobalIntensityVariationRateScale,
      kParamGlobalPitchVariationRateScale,
      kParamGlobalPanVariationRateScale
    }};
    plugin_ui::AttachMainControls(
      pGraphics,
      kParamGain,
      globalModifierParamIdxs,
      kCtrlTagHarmonicVisualizer,
      kCtrlTagKeyboard,
      kCtrlTagBender,
      kCtrlTagBreathMeter,
      kCtrlTagMeter);
    
    pGraphics->SetQwertyMidiKeyHandlerFunc([this, pGraphics](const IMidiMsg& msg) {
      plugin_ui::HandleQwertyMidi(pGraphics, kCtrlTagKeyboard, mLastQwertyMIDINote, msg);
    });
  };
#endif
}

#if IPLUG_DSP
void AdditiveWindSynth::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  (void) inputs;
  mDSP.ProcessBlock(outputs, nFrames);
  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
  mHarmonicVisualizerSender.ProcessBlock(
    nFrames,
    kCtrlTagHarmonicVisualizer,
    [this](VisualizerFrame& frame) { mDSP.GetVisualizerFrame(frame); });
}

void AdditiveWindSynth::OnIdle()
{
  mMeterSender.TransmitData(*this);
  mHarmonicVisualizerSender.TransmitData(*this);
  const double breathLevel = mBreathLevel.load(std::memory_order_relaxed);
  if(breathLevel != mLastSentBreathLevel)
  {
    SendControlValueFromDelegate(kCtrlTagBreathMeter, breathLevel);
    mLastSentBreathLevel = breathLevel;
  }
}

void AdditiveWindSynth::OnReset()
{
  mDSP.Reset(GetSampleRate(), GetBlockSize());
  mMeterSender.Reset(GetSampleRate());
  mHarmonicVisualizerSender.Reset(GetSampleRate(), PLUG_FPS);
  mLastQwertyMIDINote = -1;
  mBreathLevel.store(1.f, std::memory_order_relaxed);
  mLastSentBreathLevel = -1.;
}

void AdditiveWindSynth::ProcessMidiMsg(const IMidiMsg& msg)
{
  if(msg.StatusMsg() == IMidiMsg::kControlChange && msg.ControlChangeIdx() == IMidiMsg::kBreathController)
  {
    mBreathLevel.store(static_cast<float>(msg.ControlChange(IMidiMsg::kBreathController)), std::memory_order_relaxed);
  }

  mDSP.ProcessMidiMsg(msg);
  SendMidiMsg(msg);
}

void AdditiveWindSynth::OnParamChange(int paramIdx)
{
  mDSP.SetParam(paramIdx, GetParam(paramIdx)->Value());
}

bool AdditiveWindSynth::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  if(ctrlTag == kCtrlTagBender && msgTag == IWheelControl::kMessageTagSetPitchBendRange)
  {
    const int bendRange = *static_cast<const int*>(pData);
    mDSP.mSynth.SetPitchBendRange(bendRange);
  }
  
  return false;
}
#endif
