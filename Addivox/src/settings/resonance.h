#pragma once

#include "eq.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

// The Resonance tab's zero-breath and full-breath curves, which breath morphs between. Both curves have the same number of points, each is sorted by
// frequency on its own, and points pair up by index. Unlike EqCurve, points sharing a frequency are kept, since merging them would break the pairing.
class ResonanceCurves {
public:
  using PointList = EqCurve::PointList;
  enum class Breath : std::size_t { zero = 0, full = 1 };

  static Breath Other(Breath breath) { return breath == Breath::zero ? Breath::full : Breath::zero; }

  // Leaves the curves unchanged and returns false if the point counts differ.
  bool SetPoints(PointList zeroBreathPoints, PointList fullBreathPoints) {
    if (zeroBreathPoints.size() != fullBreathPoints.size()) return false;

    mCurves = {Curve{std::move(zeroBreathPoints)}, Curve{std::move(fullBreathPoints)}};
    return true;
  }

  const PointList& GetPoints(Breath breath) const { return Get(breath).points; }

  bool Empty() const { return mCurves.front().points.empty(); }

  double EvaluateVisualY(Breath breath, double frequencyHz) const {
    return EqCurve::EvaluateSplineVisualY(Get(breath).spline, EqCurve::LogFrequency(frequencyHz));
  }

  // Writes the curve at the given breath (0 = zero-breath curve, 1 = full-breath curve) into spline, for EqCurve::EvaluateSplineVisualY. Each point pair is
  // interpolated linearly in the splines' (log-frequency, visual-y) space. Reuses spline's storage, so it doesn't allocate once spline has held as many points.
  void Morph(double breath, MonotoneSpline& spline) const {
    const Curve& zero = Get(Breath::zero);
    const Curve& full = Get(Breath::full);
    spline.x.resize(zero.points.size());
    spline.y.resize(zero.points.size());
    for (std::size_t i = 0; i < zero.points.size(); ++i) {
      spline.x[i] = EqCurve::Lerp(zero.spline.x[i], full.spline.x[i], breath);
      spline.y[i] = EqCurve::Lerp(zero.spline.y[i], full.spline.y[i], breath);
    }
    spline.BuildTangents();
  }

  // Adds a point to one curve, plus its pair on the other. The pair goes at the same log-frequency fraction between the corresponding neighbours (axis edges
  // stand in for missing neighbours), at the other curve's current gain, so the other curve keeps its shape.
  void AddPoint(Breath breath, const EqPoint& point) {
    PointList points = GetPoints(breath);
    PointList otherPoints = GetPoints(Other(breath));
    const auto index = static_cast<std::size_t>(UpperBound(points, point) - points.begin());
    const auto neighbourFrequenciesHz = [index](const PointList& list) {
      return std::pair{index > 0 ? list[index - 1].frequencyHz : EqCurve::kMinFrequencyHz,
                       index < list.size() ? list[index].frequencyHz : EqCurve::kMaxFrequencyHz};
    };

    const auto [loHz, hiHz] = neighbourFrequenciesHz(points);
    const auto [otherLoHz, otherHiHz] = neighbourFrequenciesHz(otherPoints);
    const double t = hiHz > loHz ? std::log(point.frequencyHz / loHz) / std::log(hiHz / loHz) : 0.0;
    double otherHz = std::clamp(otherLoHz * std::pow(otherHiHz / otherLoHz, t), otherLoHz, otherHiHz);

    // A first point's pair goes halfway to an axis edge, so zero breath sits left of full breath: an upward sweep with breath, once there are more points.
    if (otherPoints.empty()) otherHz = std::sqrt(point.frequencyHz * (breath == Breath::zero ? EqCurve::kMaxFrequencyHz : EqCurve::kMinFrequencyHz));

    otherPoints.insert(otherPoints.begin() + static_cast<std::ptrdiff_t>(index),
                       EqPoint{otherHz, EqCurve::DbFromVisualY(EvaluateVisualY(Other(breath), otherHz))});
    points.insert(points.begin() + static_cast<std::ptrdiff_t>(index), point);
    Set(breath, std::move(points), std::move(otherPoints));
  }

  // Removes a point and its pair.
  void RemovePoint(std::size_t index) {
    PointList zeroBreathPoints = GetPoints(Breath::zero);
    PointList fullBreathPoints = GetPoints(Breath::full);
    zeroBreathPoints.erase(zeroBreathPoints.begin() + static_cast<std::ptrdiff_t>(index));
    fullBreathPoints.erase(fullBreathPoints.begin() + static_cast<std::ptrdiff_t>(index));
    SetPoints(std::move(zeroBreathPoints), std::move(fullBreathPoints));
  }

  // Moves one curve's point and returns its new index. Moving past a neighbour re-sorts that curve, which changes its pairings.
  std::size_t MovePoint(Breath breath, std::size_t index, const EqPoint& point) {
    PointList points = GetPoints(breath);
    points.erase(points.begin() + static_cast<std::ptrdiff_t>(index));
    const auto newIndex = static_cast<std::size_t>(points.insert(UpperBound(points, point), point) - points.begin());
    Set(breath, std::move(points), PointList{GetPoints(Other(breath))});
    return newIndex;
  }

  // Applies an action to one curve (see EqCurve::ApplyAction). Returns false if there is nothing to act on.
  bool ApplyAction(Breath breath, EqCurve::Action action) {
    PointList points = GetPoints(breath);
    if (!EqCurve::ApplyAction(points, action)) return false;
    Set(breath, std::move(points), PointList{GetPoints(Other(breath))});
    return true;
  }

  void CopyToOther(Breath breath) { mCurves[static_cast<std::size_t>(Other(breath))] = Get(breath); }

  void Swap() { std::swap(mCurves.front(), mCurves.back()); }

private:
  struct Curve {
    Curve() = default;
    explicit Curve(PointList unsortedPoints) : points(EqCurve::ClampAndSort(std::move(unsortedPoints))), spline(EqCurve::MakeSpline(points)) {}

    PointList points;
    MonotoneSpline spline;
  };

  static PointList::iterator UpperBound(PointList& points, const EqPoint& point) {
    return std::upper_bound(points.begin(), points.end(), point, [](const EqPoint& lhs, const EqPoint& rhs) { return lhs.frequencyHz < rhs.frequencyHz; });
  }

  const Curve& Get(Breath breath) const { return mCurves[static_cast<std::size_t>(breath)]; }

  void Set(Breath breath, PointList points, PointList otherPoints) {
    if (breath == Breath::zero) SetPoints(std::move(points), std::move(otherPoints));
    else
      SetPoints(std::move(otherPoints), std::move(points));
  }

  std::array<Curve, 2> mCurves{};
};

// A key note's whole Resonance setup.
struct ResonanceSettings {
  static constexpr double kTrackingPivotMidiNote = 60.0; // Tracking never shifts the curves at middle C.

  ResonanceCurves curves;
  double breathResponse{0.0}; // -1 to 1. Positive moves the morph earlier in the breath, negative later, and 0 is linear.
  double tracking{0.0};       // 0 to 1: how far the curves follow pitch, pivoting around middle C.

  // Shapes breath (0 to 1) into the morph position (see ResonanceCurves::Morph) by scaling its odds, breath / (1 - breath), by 10^breathResponse. The curve
  // is symmetric about y = 1 - x, and opposite responses mirror each other.
  static double ShapeBreath(double breath, double breathResponse) {
    const double oddsScale = std::pow(10.0, breathResponse);
    return (oddsScale * breath) / (1.0 + ((oddsScale - 1.0) * breath));
  }

  void Sanitize() {
    breathResponse = std::isfinite(breathResponse) ? std::clamp(breathResponse, -1.0, 1.0) : 0.0;
    tracking = std::isfinite(tracking) ? std::clamp(tracking, 0.0, 1.0) : 0.0;
  }
};

// The Resonance curve at one breath and pitch, blended between two key notes' setups. Breath Response and Tracking are blended first, so both key notes share
// one breath shaping and one pitch shift, and identical curves stay aligned. Prepare it when breath or pitch changes, then evaluate it per harmonic. Preparing
// reuses the splines' storage, so it doesn't allocate unless a curve has more than kReservedPoints points.
class ResonanceMorph {
public:
  ResonanceMorph() {
    mLower.Reserve(kReservedPoints);
    mUpper.Reserve(kReservedPoints);
  }

  // t blends from lower (0) to upper (1), as in CompoundPatch::ResolvedNoteSpan.
  void Prepare(const ResonanceSettings& lower, const ResonanceSettings& upper, double t, double breath, double midiPitch) {
    mBlendT = &lower == &upper ? 0.0 : t;
    const double tracking = EqCurve::Lerp(lower.tracking, upper.tracking, mBlendT);
    const double shapedBreath = ResonanceSettings::ShapeBreath(breath, EqCurve::Lerp(lower.breathResponse, upper.breathResponse, mBlendT));
    mLogFrequencyShift = tracking * (midiPitch - ResonanceSettings::kTrackingPivotMidiNote) * kLogFrequencyPerSemitone;
    lower.curves.Morph(shapedBreath, mLower);
    if (mBlendT > 0.0) upper.curves.Morph(shapedBreath, mUpper);
  }

  // Tracking translates the curves along log-frequency, which leaves their shape unchanged. Fastest when called in order of increasing frequency.
  double EvaluateVisualY(double logFrequency) {
    const double x = logFrequency - mLogFrequencyShift;
    const double lower = EqCurve::EvaluateSplineVisualY(mLower, x, &mLowerSegment);
    return mBlendT > 0.0 ? EqCurve::Lerp(lower, EqCurve::EvaluateSplineVisualY(mUpper, x, &mUpperSegment), mBlendT) : lower;
  }

private:
  static constexpr std::size_t kReservedPoints = 128;
  static constexpr double kLogFrequencyPerSemitone = 0.6931471805599453 / 12.0; // ln(2) / 12

  MonotoneSpline mLower{};
  MonotoneSpline mUpper{};
  double mBlendT{0.0};
  double mLogFrequencyShift{0.0};
  std::size_t mLowerSegment{0}; // Search hints for EvaluateVisualY
  std::size_t mUpperSegment{0};
};
