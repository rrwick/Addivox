#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

struct EqPoint {
  double frequencyHz{0.0};
  double gainDb{0.0};
};

// Monotone cubic Hermite spline, flat beyond its end points. Points sharing an x value form a step.
struct MonotoneSpline {
  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> tangents;

  void BuildTangents() {
    tangents.assign(x.size(), 0.0);
    for (std::size_t i = 1; i + 1 < x.size(); ++i) {
      const double w0 = x[i] - x[i - 1];
      const double w1 = x[i + 1] - x[i];
      if (w0 <= kEpsilon || w1 <= kEpsilon) continue;

      const double s0 = (y[i] - y[i - 1]) / w0;
      const double s1 = (y[i + 1] - y[i]) / w1;
      if ((s0 * s1) <= 0.0) continue;

      const double a = (2.0 * w1) + w0;
      const double b = w1 + (2.0 * w0);
      tangents[i] = (a + b) / ((a / s0) + (b / s1));
    }
  }

  double Evaluate(double atX) const {
    if (x.empty()) return 0.0;
    if (atX <= x.front()) return y.front();
    if (atX >= x.back()) return y.back();

    const std::size_t i1 = static_cast<std::size_t>(std::lower_bound(x.begin(), x.end(), atX) - x.begin());
    const std::size_t i0 = i1 - 1;
    const double h = x[i1] - x[i0];
    if (h <= kEpsilon) return y[i1];

    const double t = (atX - x[i0]) / h;
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double h00 = (2.0 * t3) - (3.0 * t2) + 1.0;
    const double h10 = t3 - (2.0 * t2) + t;
    const double h01 = (-2.0 * t3) + (3.0 * t2);
    const double h11 = t3 - t2;
    return (h00 * y[i0]) + (h10 * h * tangents[i0]) + (h01 * y[i1]) + (h11 * h * tangents[i1]);
  }

private:
  static constexpr double kEpsilon = 1.0e-12;
};

class EqCurve {
public:
  using PointList = std::vector<EqPoint>;

  static constexpr double kMinFrequencyHz = 20.0;
  static constexpr double kMaxFrequencyHz = 22000.0;
  static constexpr double kMuteGainDb = -160.0;
  static constexpr double kMinGainDb = kMuteGainDb;
  static constexpr double kMinFiniteGainDb = -24.0;
  static constexpr double kMaxGainDb = 24.0;

  EqCurve() = default;

  explicit EqCurve(PointList points) { SetPoints(std::move(points)); }

  EqCurve(std::initializer_list<EqPoint> points) { SetPoints(PointList{points}); }

  const PointList& GetPoints() const { return mPoints; }

  void SetPoints(PointList points) {
    mPoints = MergeEqualFrequencies(ClampAndSort(std::move(points)));
    mSpline = MakeSpline(mPoints);
  }

  bool Empty() const { return mPoints.empty(); }

  double EvaluateDb(double frequencyHz) const { return EvaluateSplineDb(mSpline, frequencyHz); }

  // For splines from MakeSpline: x is log-frequency and y is gain in dB.
  static double EvaluateSplineDb(const MonotoneSpline& spline, double frequencyHz) { return ClampGainDb(spline.Evaluate(LogFrequency(frequencyHz))); }

  // Expects points sorted by frequency.
  static MonotoneSpline MakeSpline(const PointList& points) {
    MonotoneSpline spline;
    for (const auto& point : points) {
      spline.x.push_back(LogFrequency(point.frequencyHz));
      spline.y.push_back(point.gainDb);
    }
    spline.BuildTangents();
    return spline;
  }

  static PointList ClampAndSort(PointList points) {
    for (auto& point : points) {
      point.frequencyHz = ClampFrequencyHz(point.frequencyHz);
      point.gainDb = ClampGainDb(point.gainDb);
    }

    // Points are clamped to finite values above before sorting, since NaN would violate strict-weak-ordering here.
    std::stable_sort(points.begin(), points.end(), [](const EqPoint& lhs, const EqPoint& rhs) { return lhs.frequencyHz < rhs.frequencyHz; });
    return points;
  }

  static double LogFrequency(double frequencyHz) { return std::log(ClampFrequencyHz(frequencyHz)); }

  static bool IsMutedGainDb(double gainDb) {
    if (!std::isfinite(gainDb)) return gainDb < 0.0;

    return gainDb <= (kMuteGainDb + kMuteGainDbEpsilon);
  }

  static double ClampGainDb(double gainDb) {
    if (std::isnan(gainDb)) return 0.0;
    if (!std::isfinite(gainDb)) return gainDb < 0.0 ? kMuteGainDb : kMaxGainDb;

    return std::clamp(gainDb, kMinGainDb, kMaxGainDb);
  }

  static double DbToGain(double gainDb) {
    if (IsMutedGainDb(gainDb)) return 0.0;

    return std::pow(10.0, ClampGainDb(gainDb) / 20.0);
  }

  static double GainToDb(double gain) {
    if (!(gain > 0.0) || !std::isfinite(gain)) return kMuteGainDb;

    return ClampGainDb(20.0 * std::log10(gain));
  }

  // The editor's vertical scale (0 = top, 1 = bottom): linear in dB from +24 to -24 dB, then linear in gain through the mute band. All gain interpolation
  // happens in this space, so changes look linear on screen and fade into mute the same way dragging a point does.
  static double VisualYFromDb(double gainDb) {
    const double clampedGainDb = ClampGainDb(gainDb);
    if (clampedGainDb >= kMinFiniteGainDb) return (kMaxGainDb - clampedGainDb) / kTotalVisualDbSpan;

    const double muteBandGain = std::min(DbToGain(clampedGainDb) / kMuteBandTopGain, 1.0);
    return kMuteBandTopVisualY + ((1.0 - muteBandGain) * (1.0 - kMuteBandTopVisualY));
  }

  static double DbFromVisualY(double visualY) {
    const double clampedVisualY = std::clamp(visualY, 0.0, 1.0);
    if (clampedVisualY <= kMuteBandTopVisualY) return kMaxGainDb - (clampedVisualY * kTotalVisualDbSpan);

    const double muteBandProgress = (clampedVisualY - kMuteBandTopVisualY) / (1.0 - kMuteBandTopVisualY);
    return GainToDb(kMuteBandTopGain * (1.0 - muteBandProgress));
  }

  // Scales the curve's distance from 0 dB in visual-y space: 0 flattens it, 1 leaves it unchanged and larger values exaggerate it.
  static double ScaleDb(double gainDb, double scale) {
    if (scale == 1.0) return gainDb;

    return DbFromVisualY(kZeroDbVisualY + (scale * (VisualYFromDb(gainDb) - kZeroDbVisualY)));
  }

  static double InterpolateDb(double lo, double hi, double t) {
    if (t <= 0.0) return lo;
    if (t >= 1.0) return hi;

    return DbFromVisualY(Lerp(VisualYFromDb(lo), VisualYFromDb(hi), t));
  }

private:
  static constexpr double kFrequencyEpsilonHz = 1.0e-6;
  static constexpr double kMuteGainDbEpsilon = 1.0e-9;
  static constexpr double kVisibleDbSpan = kMaxGainDb - kMinFiniteGainDb;
  static constexpr double kMuteBandVisualDbEquivalent = 4.0; // The mute band's height, in dB of the scale above it.
  static constexpr double kTotalVisualDbSpan = kVisibleDbSpan + kMuteBandVisualDbEquivalent;
  static constexpr double kZeroDbVisualY = kMaxGainDb / kTotalVisualDbSpan;
  static constexpr double kMuteBandTopVisualY = kVisibleDbSpan / kTotalVisualDbSpan;
  inline static const double kMuteBandTopGain = std::pow(10.0, kMinFiniteGainDb / 20.0);

  static double Lerp(double lo, double hi, double t) { return lo + ((hi - lo) * t); }

  static double ClampFrequencyHz(double frequencyHz) {
    if (std::isnan(frequencyHz)) return kMinFrequencyHz;
    if (!std::isfinite(frequencyHz)) return frequencyHz < 0.0 ? kMinFrequencyHz : kMaxFrequencyHz;

    return std::clamp(frequencyHz, kMinFrequencyHz, kMaxFrequencyHz);
  }

  static PointList MergeEqualFrequencies(const PointList& points) {
    PointList merged;
    merged.reserve(points.size());
    for (const auto& point : points) {
      if (!merged.empty() && std::abs(merged.back().frequencyHz - point.frequencyHz) <= kFrequencyEpsilonHz) merged.back().gainDb = point.gainDb;
      else
        merged.push_back(point);
    }

    return merged;
  }

  PointList mPoints{};
  MonotoneSpline mSpline{};
};
