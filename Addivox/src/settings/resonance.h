#pragma once

#include "eq.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

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

  double EvaluateDb(Breath breath, double frequencyHz) const { return EqCurve::EvaluateSplineDb(Get(breath).spline, frequencyHz); }

  // Writes the curve at the given breath (0 = zero-breath curve, 1 = full-breath curve) into spline, for EqCurve::EvaluateSplineDb. Each point pair is
  // interpolated, x linearly in log-frequency and y linearly in visual-y. Reuses spline's storage, so it doesn't allocate once spline has held as many points.
  void Morph(double breath, MonotoneSpline& spline) const {
    const Curve& zero = Get(Breath::zero);
    const Curve& full = Get(Breath::full);
    spline.x.resize(zero.points.size());
    spline.y.resize(zero.points.size());
    for (std::size_t i = 0; i < zero.points.size(); ++i) {
      spline.x[i] = zero.spline.x[i] + (breath * (full.spline.x[i] - zero.spline.x[i]));
      spline.y[i] = EqCurve::DbFromVisualY(zero.visualY[i] + (breath * (full.visualY[i] - zero.visualY[i])));
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

    otherPoints.insert(otherPoints.begin() + static_cast<std::ptrdiff_t>(index), EqPoint{otherHz, EvaluateDb(Other(breath), otherHz)});
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

private:
  struct Curve {
    Curve() = default;
    explicit Curve(PointList unsortedPoints) : points(EqCurve::ClampAndSort(std::move(unsortedPoints))), spline(EqCurve::MakeSpline(points)) {
      for (const auto& point : points) visualY.push_back(EqCurve::VisualYFromDb(point.gainDb));
    }

    PointList points;
    MonotoneSpline spline;
    std::vector<double> visualY; // Cached for morphing
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
