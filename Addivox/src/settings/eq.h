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
    mPoints = SanitizePoints(std::move(points));
    RebuildSpline();
  }

  bool Empty() const { return mPoints.empty(); }

  double EvaluateDb(double frequencyHz) const {
    if (mPoints.empty()) return 0.0;
    if (mPoints.size() == 1) return mPoints.front().gainDb;

    return ClampGainDb(EvaluateSplineDb(frequencyHz));
  }

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
  static constexpr double kLogFrequencyEpsilon = 1.0e-12;
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

  static double LogFrequency(double frequencyHz) { return std::log(ClampFrequencyHz(frequencyHz)); }

  static PointList SanitizePoints(PointList points) {
    for (auto& point : points) {
      point.frequencyHz = ClampFrequencyHz(point.frequencyHz);
      point.gainDb = ClampGainDb(point.gainDb);
    }

    // Points are clamped to finite values above before sorting, since NaN would violate strict-weak-ordering here.
    std::stable_sort(points.begin(), points.end(), [](const EqPoint& lhs, const EqPoint& rhs) { return lhs.frequencyHz < rhs.frequencyHz; });

    PointList sanitized;
    sanitized.reserve(points.size());
    for (const auto& point : points) {
      if (!sanitized.empty() && std::abs(sanitized.back().frequencyHz - point.frequencyHz) <= kFrequencyEpsilonHz) {
        sanitized.back().gainDb = point.gainDb;
      } else
        sanitized.push_back(point);
    }

    return sanitized;
  }

  void RebuildSpline() {
    mLogFrequencies.clear();
    mTangents.clear();

    if (mPoints.size() <= 1) return;

    mLogFrequencies.resize(mPoints.size());
    for (std::size_t i = 0; i < mPoints.size(); ++i) mLogFrequencies[i] = LogFrequency(mPoints[i].frequencyHz);

    const std::size_t numSegments = mPoints.size() - 1;
    std::vector<double> widths(numSegments);
    std::vector<double> slopes(numSegments);
    for (std::size_t i = 0; i < numSegments; ++i) {
      widths[i] = mLogFrequencies[i + 1] - mLogFrequencies[i];
      slopes[i] = (mPoints[i + 1].gainDb - mPoints[i].gainDb) / widths[i];
    }

    mTangents.resize(mPoints.size());

    // The curve is flat outside the first and last point, so the spline should meet those edges with zero slope rather than forming a corner.
    mTangents.front() = 0.0;
    for (std::size_t i = 1; i + 1 < mPoints.size(); ++i) {
      if ((slopes[i - 1] * slopes[i]) <= 0.0) {
        mTangents[i] = 0.0;
        continue;
      }

      const double w1 = (2.0 * widths[i]) + widths[i - 1];
      const double w2 = widths[i] + (2.0 * widths[i - 1]);
      mTangents[i] = (w1 + w2) / ((w1 / slopes[i - 1]) + (w2 / slopes[i]));
    }
    mTangents.back() = 0.0;
  }

  double EvaluateSplineDb(double frequencyHz) const {
    const double clampedFrequencyHz = ClampFrequencyHz(frequencyHz);
    if (clampedFrequencyHz <= mPoints.front().frequencyHz) return mPoints.front().gainDb;
    if (clampedFrequencyHz >= mPoints.back().frequencyHz) return mPoints.back().gainDb;

    const double logFrequency = LogFrequency(clampedFrequencyHz);
    const auto upper = std::lower_bound(mLogFrequencies.begin(), mLogFrequencies.end(), logFrequency);
    if (upper == mLogFrequencies.begin()) return mPoints.front().gainDb;
    if (upper == mLogFrequencies.end()) return mPoints.back().gainDb;

    const std::size_t upperIndex = static_cast<std::size_t>(upper - mLogFrequencies.begin());
    return EvaluateHermiteSegment(upperIndex - 1, upperIndex, logFrequency);
  }

  double EvaluateHermiteSegment(std::size_t lowerIndex, std::size_t upperIndex, double logFrequency) const {
    const double x0 = mLogFrequencies[lowerIndex];
    const double x1 = mLogFrequencies[upperIndex];
    const double h = x1 - x0;
    if (h <= kLogFrequencyEpsilon) return mPoints[upperIndex].gainDb;

    const double t = (logFrequency - x0) / h;
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double h00 = (2.0 * t3) - (3.0 * t2) + 1.0;
    const double h10 = t3 - (2.0 * t2) + t;
    const double h01 = (-2.0 * t3) + (3.0 * t2);
    const double h11 = t3 - t2;
    return (h00 * mPoints[lowerIndex].gainDb) + (h10 * h * mTangents[lowerIndex]) + (h01 * mPoints[upperIndex].gainDb) + (h11 * h * mTangents[upperIndex]);
  }

  PointList mPoints{};
  std::vector<double> mLogFrequencies{};
  std::vector<double> mTangents{};
};
