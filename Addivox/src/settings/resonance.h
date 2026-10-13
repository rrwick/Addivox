#pragma once

#include "eq.h"

#include <array>
#include <cstddef>
#include <utility>

// The Resonance tab's zero-breath and full-breath curves, which breath morphs between. Both curves have the same number of points, each is sorted by
// frequency on its own, and points pair up by index. Unlike EqCurve, points sharing a frequency are kept, since merging them would break the pairing.
class ResonanceCurves {
public:
  enum class Breath : std::size_t { zero = 0, full = 1 };

  // Leaves the curves unchanged and returns false if the point counts differ.
  bool SetPoints(EqCurve::PointList zeroBreathPoints, EqCurve::PointList fullBreathPoints) {
    if (zeroBreathPoints.size() != fullBreathPoints.size()) return false;

    mCurves = {Curve{std::move(zeroBreathPoints)}, Curve{std::move(fullBreathPoints)}};
    return true;
  }

  const EqCurve::PointList& GetPoints(Breath breath) const { return Get(breath).points; }

  bool Empty() const { return mCurves.front().points.empty(); }

  double EvaluateDb(Breath breath, double frequencyHz) const { return EqCurve::ClampGainDb(Get(breath).spline.Evaluate(EqCurve::LogFrequency(frequencyHz))); }

private:
  struct Curve {
    Curve() = default;
    explicit Curve(EqCurve::PointList unsortedPoints) : points(EqCurve::ClampAndSort(std::move(unsortedPoints))), spline(EqCurve::MakeSpline(points)) {}

    EqCurve::PointList points;
    MonotoneSpline spline;
  };

  const Curve& Get(Breath breath) const { return mCurves[static_cast<std::size_t>(breath)]; }

  std::array<Curve, 2> mCurves{};
};
