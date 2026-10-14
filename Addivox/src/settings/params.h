#pragma once

enum EParams {
  kParamGlobalLevel = 0,
  kParamGlobalAttackScale,
  kParamGlobalReleaseScale,
  kParamGlobalTuning,
  kParamGlobalPanShift,
  kParamGlobalLevelVariationAmplitudeScale,
  kParamGlobalLevelVariationRateScale,
  kParamGlobalPitchVariationAmplitudeScale,
  kParamGlobalPitchVariationRateScale,
  kParamGlobalPanVariationAmplitudeScale,
  kParamGlobalPanVariationRateScale,
  kParamPortamentoAtCC5Min,
  kParamPortamentoAtCC5Max,
  kParamEffectsReverb,
  kParamEffectsDrive,
  kParamEffectsChorus,
  kParamEffectsTone,
  kParamTranspose,
  kParamFormantsScale, // New parameters go last, so saved host states and automation keep their indices.
  kParamResonanceScale,
  kNumParams
};
