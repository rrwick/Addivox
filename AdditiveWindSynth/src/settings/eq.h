#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

struct EqPoint
{
  double frequencyHz{0.0};
  double gainDb{0.0};
};

class EqCurve
{
public:
  using PointList = std::vector<EqPoint>;

  static constexpr double kMinFrequencyHz = 20.0;
  static constexpr double kMaxFrequencyHz = 20000.0;
  static constexpr double kMinGainDb = -24.0;
  static constexpr double kMaxGainDb = 24.0;

  EqCurve() = default;
  explicit EqCurve(PointList points)
  {
    SetPoints(std::move(points));
  }

  EqCurve(std::initializer_list<EqPoint> points)
  {
    SetPoints(PointList{points});
  }

  const PointList& GetPoints() const
  {
    return mPoints;
  }

  void SetPoints(PointList points)
  {
    mPoints = SanitizePoints(std::move(points));
  }

  bool Empty() const
  {
    return mPoints.empty();
  }

  double EvaluateDb(double frequencyHz) const
  {
    if(mPoints.empty())
      return 0.0;

    const double clampedFrequencyHz = std::clamp(frequencyHz, kMinFrequencyHz, kMaxFrequencyHz);
    if(clampedFrequencyHz <= mPoints.front().frequencyHz)
      return mPoints.front().gainDb;
    if(clampedFrequencyHz >= mPoints.back().frequencyHz)
      return mPoints.back().gainDb;

    const auto upper = std::lower_bound(
      mPoints.begin(),
      mPoints.end(),
      clampedFrequencyHz,
      [](const EqPoint& point, double frequency) {
        return point.frequencyHz < frequency;
      });

    if(upper == mPoints.begin())
      return upper->gainDb;
    if(upper == mPoints.end())
      return mPoints.back().gainDb;
    if(std::abs(upper->frequencyHz - clampedFrequencyHz) <= kFrequencyEpsilonHz)
      return upper->gainDb;

    const auto lower = std::prev(upper);
    const double spanHz = upper->frequencyHz - lower->frequencyHz;
    if(spanHz <= kFrequencyEpsilonHz)
      return upper->gainDb;

    const double t = (clampedFrequencyHz - lower->frequencyHz) / spanHz;
    return lower->gainDb + ((upper->gainDb - lower->gainDb) * t);
  }

  static EqCurve Interpolate(const EqCurve& lo, const EqCurve& hi, double t)
  {
    const double clampedT = std::clamp(t, 0.0, 1.0);
    if(clampedT <= 0.0)
      return lo;
    if(clampedT >= 1.0)
      return hi;

    PointList points;
    points.reserve(lo.mPoints.size() + hi.mPoints.size());
    for(const auto& point : lo.mPoints)
      points.push_back(point);
    for(const auto& point : hi.mPoints)
      points.push_back(point);

    points = SanitizePoints(std::move(points));
    if(points.empty())
      return {};

    PointList interpolatedPoints;
    interpolatedPoints.reserve(points.size());
    for(const auto& point : points)
    {
      const double loDb = lo.EvaluateDb(point.frequencyHz);
      const double hiDb = hi.EvaluateDb(point.frequencyHz);
      interpolatedPoints.push_back({
        point.frequencyHz,
        loDb + ((hiDb - loDb) * clampedT)});
    }

    return EqCurve{std::move(interpolatedPoints)};
  }

  static double DbToGain(double gainDb)
  {
    const double clampedGainDb = std::clamp(gainDb, kMinGainDb, kMaxGainDb);
    return std::pow(10.0, clampedGainDb / 20.0);
  }

private:
  static constexpr double kFrequencyEpsilonHz = 1.0e-6;

  static PointList SanitizePoints(PointList points)
  {
    if(points.empty())
      return {};

    std::stable_sort(
      points.begin(),
      points.end(),
      [](const EqPoint& lhs, const EqPoint& rhs) {
        return lhs.frequencyHz < rhs.frequencyHz;
      });

    PointList sanitized;
    sanitized.reserve(points.size());
    for(auto point : points)
    {
      point.frequencyHz = std::clamp(point.frequencyHz, kMinFrequencyHz, kMaxFrequencyHz);
      point.gainDb = std::clamp(point.gainDb, kMinGainDb, kMaxGainDb);

      if(!sanitized.empty()
         && std::abs(sanitized.back().frequencyHz - point.frequencyHz) <= kFrequencyEpsilonHz)
      {
        sanitized.back().gainDb = point.gainDb;
      }
      else
        sanitized.push_back(point);
    }

    return sanitized;
  }

  PointList mPoints{};
};
