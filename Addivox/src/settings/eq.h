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

  void Reserve(std::size_t numPoints) {
    for (auto* values : {&x, &y, &tangents}) values->reserve(numPoints);
  }

  // Sets the points to a point-by-point blend of two splines with the same number of points. Doesn't allocate once this spline has held as many points.
  void Blend(const MonotoneSpline& from, const MonotoneSpline& to, double t) {
    x.resize(from.x.size());
    y.resize(from.y.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
      x[i] = from.x[i] + ((to.x[i] - from.x[i]) * t);
      y[i] = from.y[i] + ((to.y[i] - from.y[i]) * t);
    }
    BuildTangents();
  }

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

  // A segment hint speeds up evaluating at x values that mostly increase between calls (e.g. harmonics in order): the search for atX's segment steps from
  // the previous call's segment instead of starting a binary search.
  double Evaluate(double atX, std::size_t* segmentHint = nullptr) const {
    if (x.empty()) return 0.0;
    if (atX <= x.front()) return y.front();
    if (atX >= x.back()) return y.back();

    const std::size_t i1 = segmentHint ? StepToSegment(*segmentHint, atX) : static_cast<std::size_t>(std::lower_bound(x.begin(), x.end(), atX) - x.begin());
    if (segmentHint) *segmentHint = i1;
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

  // Same result as the binary search: the first point at or after atX, which lies strictly between the end points.
  std::size_t StepToSegment(std::size_t i1, double atX) const {
    i1 = std::clamp<std::size_t>(i1, 1, x.size() - 1);
    while (x[i1] < atX) ++i1;
    while (x[i1 - 1] >= atX) --i1;
    return i1;
  }
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

  // Fitted in visual-y (see VisualYFromDb and MakeSpline), so the curve sounds the way it looks in the editor.
  const MonotoneSpline& Spline() const { return mSpline; }

  // For a new key note between two others: blends their curves point by point (see CurveBlend), or copies the nearer one if their point counts differ.
  static EqCurve Interpolate(const EqCurve& lower, const EqCurve& upper, double t) {
    if (&lower == &upper || lower.mPoints.size() != upper.mPoints.size()) return t <= 0.5 ? lower : upper;
    return EqCurve{InterpolatePoints(lower.mPoints, upper.mPoints, t)};
  }

  // Blends two point lists of equal length point by point, in the same space as their splines.
  static PointList InterpolatePoints(const PointList& lower, const PointList& upper, double t) {
    PointList points(lower.size());
    for (std::size_t i = 0; i < points.size(); ++i)
      points[i] = {std::exp(Lerp(LogFrequency(lower[i].frequencyHz), LogFrequency(upper[i].frequencyHz), t)),
                   DbFromVisualY(Lerp(VisualYFromDb(lower[i].gainDb), VisualYFromDb(upper[i].gainDb), t))};
    return points;
  }

  // For splines from MakeSpline: x is log-frequency (see LogFrequency) and y is visual-y. A spline with no points is flat at 0 dB.
  static double EvaluateSplineVisualY(const MonotoneSpline& spline, double logFrequency, std::size_t* segmentHint = nullptr) {
    return spline.x.empty() ? kZeroDbVisualY : spline.Evaluate(logFrequency, segmentHint);
  }

  // Expects points sorted by frequency.
  static MonotoneSpline MakeSpline(const PointList& points) {
    MonotoneSpline spline;
    for (const auto& point : points) {
      spline.x.push_back(LogFrequency(point.frequencyHz));
      spline.y.push_back(VisualYFromDb(point.gainDb));
    }
    spline.BuildTangents();
    return spline;
  }

  enum class Action { scaleUp, scaleDown, shiftUp, shiftDown, shiftRight, shiftLeft, normalize, invert };

  // Applies an editor action to a point list, leaving muted points' gains alone. Keeps the point count and order, so Resonance curves stay paired. Shifting
  // left or right scales every frequency by the same ratio, stopping when the outermost point reaches the axis edge, so the curve keeps its shape. Returns
  // false if there is nothing to act on.
  static bool ApplyAction(PointList& points, Action action) {
    const auto transformGains = [&points](auto&& transform) {
      for (auto& point : points)
        if (!IsMutedGainDb(point.gainDb)) point.gainDb = ClampGainDb(transform(point.gainDb));
    };
    const auto scaleFrequencies = [&points](double ratio) {
      for (auto& point : points) point.frequencyHz *= ratio;
    };
    const auto byFrequency = [](const EqPoint& lhs, const EqPoint& rhs) { return lhs.frequencyHz < rhs.frequencyHz; };
    constexpr double kShiftRatio = 1.044273782427414; // Sixteenth-octave in log-frequency space.

    if (points.empty()) return false;
    switch (action) {
    case Action::scaleUp:    transformGains([](double gainDb) { return gainDb * 1.111111111111111; }); break;
    case Action::scaleDown:  transformGains([](double gainDb) { return gainDb * 0.9; });               break;
    case Action::shiftUp:    transformGains([](double gainDb) { return gainDb + 1.0; });               break;
    case Action::shiftDown:  transformGains([](double gainDb) { return gainDb - 1.0; });               break;
    case Action::invert:     transformGains([](double gainDb) { return -gainDb; });                    break;
    case Action::shiftRight: {
      const double ratio = std::min(kShiftRatio, kMaxFrequencyHz / std::max_element(points.begin(), points.end(), byFrequency)->frequencyHz);
      if (ratio <= 1.0) return false;
      scaleFrequencies(ratio);
      break;
    }
    case Action::shiftLeft: {
      const double ratio = std::max(1.0 / kShiftRatio, kMinFrequencyHz / std::min_element(points.begin(), points.end(), byFrequency)->frequencyHz);
      if (ratio >= 1.0) return false;
      scaleFrequencies(ratio);
      break;
    }
    case Action::normalize: {
      double meanDb = 0.0;
      std::size_t activePointCount = 0;
      for (const auto& point : points) {
        if (IsMutedGainDb(point.gainDb)) continue;
        meanDb += point.gainDb;
        ++activePointCount;
      }
      if (activePointCount == 0) return false;

      meanDb /= static_cast<double>(activePointCount);
      transformGains([meanDb](double gainDb) { return gainDb - meanDb; });
      break;
    }
    }
    return true;
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

  // The editor's vertical scale (0 = top, 1 = bottom): linear in dB from +24 to -24 dB, then linear in gain through the mute band. Curves, key-note
  // blending, breath morphing and the amount knobs all work in this space, so changes look linear on screen and fade into mute the same way dragging a
  // point does.
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

  static double GainFromVisualY(double visualY) {
    if (visualY <= kMuteBandTopVisualY) return std::pow(10.0, (kMaxGainDb - (std::max(visualY, 0.0) * kTotalVisualDbSpan)) / 20.0);

    const double muteBandProgress = (visualY - kMuteBandTopVisualY) / (1.0 - kMuteBandTopVisualY);
    return kMuteBandTopGain * std::max(1.0 - muteBandProgress, 0.0);
  }

  // Scales a curve's distance from 0 dB: 0 flattens it, 1 leaves it unchanged and larger values exaggerate it.
  static double ScaleVisualY(double visualY, double scale) { return std::clamp(kZeroDbVisualY + (scale * (visualY - kZeroDbVisualY)), 0.0, 1.0); }

  static double Lerp(double lo, double hi, double t) { return lo + ((hi - lo) * t); }

private:
  static constexpr double kFrequencyEpsilonHz = 1.0e-6;
  static constexpr double kMuteGainDbEpsilon = 1.0e-9;
  static constexpr double kVisibleDbSpan = kMaxGainDb - kMinFiniteGainDb;
  static constexpr double kMuteBandVisualDbEquivalent = 4.0; // The mute band's height, in dB of the scale above it.
  static constexpr double kTotalVisualDbSpan = kVisibleDbSpan + kMuteBandVisualDbEquivalent;
  static constexpr double kZeroDbVisualY = kMaxGainDb / kTotalVisualDbSpan;
  static constexpr double kMuteBandTopVisualY = kVisibleDbSpan / kTotalVisualDbSpan;
  inline static const double kMuteBandTopGain = std::pow(10.0, kMinFiniteGainDb / 20.0);

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

// Two key notes' curves blended by t (0 = lower, 1 = upper). If their point counts match, each point glides between its positions on the two key notes, so a
// peak moves rather than fading out in one place and in at another. Otherwise, one curve fades into the other. Prepare it whenever t or the curves change.
class CurveBlend {
public:
  static constexpr std::size_t kReservedPoints = 128; // Preparing doesn't allocate for curves up to this size

  CurveBlend() { mBlended.Reserve(kReservedPoints); }

  // Keeps pointers to lower and upper, which must outlive evaluation.
  void Prepare(const MonotoneSpline& lower, const MonotoneSpline& upper, double t) {
    mLower = &lower;
    mUpper = &upper;
    mT = &lower == &upper ? 0.0 : t;
    if (mT > 0.0 && lower.x.size() == upper.x.size()) {
      mBlended.Blend(lower, upper, mT);
      mLower = &mBlended;
      mT = 0.0;
    }
  }

  // Fastest when called in order of increasing frequency.
  double EvaluateVisualY(double logFrequency) {
    const double lower = EqCurve::EvaluateSplineVisualY(*mLower, logFrequency, &mLowerSegment);
    return mT > 0.0 ? EqCurve::Lerp(lower, EqCurve::EvaluateSplineVisualY(*mUpper, logFrequency, &mUpperSegment), mT) : lower;
  }

private:
  const MonotoneSpline* mLower{nullptr};
  const MonotoneSpline* mUpper{nullptr};
  MonotoneSpline mBlended{};
  double mT{0.0};
  std::size_t mLowerSegment{0}; // Search hints for EvaluateVisualY
  std::size_t mUpperSegment{0};
};
