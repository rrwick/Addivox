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
  using ResponseLut = std::vector<double>;
  static constexpr std::size_t kResponseLutSize = 512;

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
    mResponseLut.clear();
    RebuildSpline();
  }

  bool Empty() const { return mPoints.empty(); }

  double EvaluateDb(double frequencyHz) const {
    if (!mResponseLut.empty()) return ClampGainDb(EvaluateResponseLut(frequencyHz));

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

  static ResponseLut InterpolateResponseLut(const ResponseLut& lo, const ResponseLut& hi, double t) {
    const double clampedT = std::clamp(t, 0.0, 1.0);
    if (clampedT <= 0.0) return lo;
    if (clampedT >= 1.0) return hi;

    ResponseLut responseLut(kResponseLutSize);
    for (std::size_t i = 0; i < responseLut.size(); ++i) {
      responseLut[i] = GainToDb(Lerp(DbToGain(lo[i]), DbToGain(hi[i]), clampedT));
    }

    return responseLut;
  }

  ResponseLut BuildResponseLut() const {
    if (!mResponseLut.empty()) return mResponseLut;

    ResponseLut responseLut(kResponseLutSize);
    const double denominator = static_cast<double>(responseLut.size() - 1);
    for (std::size_t i = 0; i < responseLut.size(); ++i) {
      const double frequencyHz = FrequencyFromNormalizedLog(static_cast<double>(i) / denominator);
      responseLut[i] = EvaluateDb(frequencyHz);
    }

    return responseLut;
  }

  static EqCurve FromResponseLut(ResponseLut responseLut) {
    EqCurve curve;
    curve.mResponseLut = std::move(responseLut);
    return curve;
  }

private:
  static constexpr double kFrequencyEpsilonHz = 1.0e-6;
  static constexpr double kLogFrequencyEpsilon = 1.0e-12;
  static constexpr double kMuteGainDbEpsilon = 1.0e-9;

  static double Lerp(double lo, double hi, double t) { return lo + ((hi - lo) * t); }

  static double ClampFrequencyHz(double frequencyHz) {
    if (std::isnan(frequencyHz)) return kMinFrequencyHz;
    if (!std::isfinite(frequencyHz)) return frequencyHz < 0.0 ? kMinFrequencyHz : kMaxFrequencyHz;

    return std::clamp(frequencyHz, kMinFrequencyHz, kMaxFrequencyHz);
  }

  static double LogFrequency(double frequencyHz) { return std::log(ClampFrequencyHz(frequencyHz)); }

  static double NormalizedLogFrequency(double frequencyHz) {
    const double logMin = std::log(kMinFrequencyHz);
    const double logMax = std::log(kMaxFrequencyHz);
    return (LogFrequency(frequencyHz) - logMin) / (logMax - logMin);
  }

  static double FrequencyFromNormalizedLog(double normalizedLogFrequency) {
    const double logMin = std::log(kMinFrequencyHz);
    const double logMax = std::log(kMaxFrequencyHz);
    const double clampedNormalizedLogFrequency = std::clamp(normalizedLogFrequency, 0.0, 1.0);
    return std::exp(logMin + ((logMax - logMin) * clampedNormalizedLogFrequency));
  }

  static PointList SanitizePoints(PointList points) {
    if (points.empty()) return {};

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
    std::vector<double> h(numSegments);
    std::vector<double> d(numSegments);
    for (std::size_t i = 0; i < numSegments; ++i) {
      h[i] = mLogFrequencies[i + 1] - mLogFrequencies[i];
      d[i] = (mPoints[i + 1].gainDb - mPoints[i].gainDb) / h[i];
    }

    mTangents.resize(mPoints.size());
    if (mPoints.size() == 2) {
      mTangents[0] = 0.0;
      mTangents[1] = 0.0;
      return;
    }

    // The curve is flat outside the first and last point, so the spline should meet those edges with zero slope rather than forming a corner.
    mTangents.front() = 0.0;
    for (std::size_t i = 1; i + 1 < mPoints.size(); ++i) {
      if ((d[i - 1] * d[i]) <= 0.0) {
        mTangents[i] = 0.0;
        continue;
      }

      const double w1 = (2.0 * h[i]) + h[i - 1];
      const double w2 = h[i] + (2.0 * h[i - 1]);
      mTangents[i] = (w1 + w2) / ((w1 / d[i - 1]) + (w2 / d[i]));
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

  double EvaluateResponseLut(double frequencyHz) const {
    if (mResponseLut.empty()) return 0.0;

    const double position = NormalizedLogFrequency(frequencyHz) * static_cast<double>(mResponseLut.size() - 1);
    const std::size_t lowerIndex = static_cast<std::size_t>(position);
    const std::size_t upperIndex = std::min(lowerIndex + 1, mResponseLut.size() - 1);
    const double fraction = position - static_cast<double>(lowerIndex);
    return ClampGainDb(Lerp(mResponseLut[lowerIndex], mResponseLut[upperIndex], fraction));
  }

  PointList mPoints{};
  std::vector<double> mLogFrequencies{};
  std::vector<double> mTangents{};
  ResponseLut mResponseLut{};
};
