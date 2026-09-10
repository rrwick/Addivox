#pragma once

#include "../../settings/oscillator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// The numerics every tab's macro fit needs, with nothing in here that knows what any tab's knobs mean. A tab
// supplies a scoring function -- a search point in, a residual out -- and that function is where its generator,
// its definition of residual and its own best-candidate bookkeeping live. This file supplies only the search
// that calls it, so a tab's parameterisation can be rewritten without the search moving.
//
// Nothing here includes a UI header, which is deliberate: the offline formula harness can include this file
// whole rather than extracting it by line range, and a stale extraction has already cost one bad constant sweep.
namespace plugin_ui {
namespace editor {
using OscillatorParameterValues = CompoundPatch::OscillatorParameterValues;

inline constexpr double kMacroEpsilon = 1.0e-12;

// Bends a knob's 0..1 travel end to end. An exponent of 1 leaves it alone; above 1 gives the bottom of the knob
// more room, below 1 the top. Bending cannot change which curves a tab can draw, only which part of a rotation
// draws them, so these answer to feel alone.
inline double BendMacroTravel(double knobValue, double exponent) {
  const double clamped = std::clamp(knobValue, 0.0, 1.0);
  return (exponent == 1.0) ? clamped : std::pow(clamped, exponent);
}

// The same, bent about the centre rather than an end, so that half travel stays half travel -- which is what a
// knob whose centre means something in particular (no lift, a straight fall) needs.
inline double BendMacroTravelAboutCentre(double knobValue, double exponent) {
  const double fromCentre = (2.0 * std::clamp(knobValue, 0.0, 1.0)) - 1.0;
  return 0.5 + (0.5 * std::copysign(BendMacroTravel(std::fabs(fromCentre), exponent), fromCentre));
}

// The curve to fit, plus a per-harmonic weight of 1 / (value + floor)^2. Unweighted least squares is blind to
// the quiet end of the series -- harmonics a thousandth of the loudest one cost almost nothing to get wrong --
// yet those are exactly the harmonics a width or reach knob controls, and a non-linear Y transform makes them
// half the chart. Relative error weights every harmonic about equally.
//
// The floor is where the fit stops caring, as a fraction of the largest value: 1e-3 is 60 dB down.
struct MacroFitTarget {
  OscillatorParameterValues values{};
  OscillatorParameterValues weights{};
};

inline MacroFitTarget MakeMacroFitTarget(const OscillatorParameterValues& values, double relativeFloor) {
  double peak = 0.0;
  for (const double value : values) peak = std::max(peak, value);

  const double floorValue = std::max(peak * relativeFloor, kMacroEpsilon);

  MacroFitTarget target;
  target.values = values;
  for (std::size_t index = 0; index < values.size(); ++index) {
    const double scale = 1.0 / (values[index] + floorValue);
    target.weights[index] = scale * scale;
  }

  return target;
}

// The searched knobs as a point, so the searches below can do arithmetic on them. It is normalised knob travel
// throughout, in whatever order the tab lists its searched knobs.
template <std::size_t N> using MacroFitPoint = std::array<double, N>;

// What the grid covers along one axis. Usually a knob's whole travel, but a knob whose value can be measured
// off the target instead of searched for gets a band around that measurement.
struct MacroFitAxis {
  double min{0.0};
  double max{1.0};
};

template <std::size_t N> inline MacroFitPoint<N> BlendMacroFitPoints(const MacroFitPoint<N>& from, const MacroFitPoint<N>& to, double amount) {
  MacroFitPoint<N> blended{};
  for (std::size_t axis = 0; axis < N; ++axis) blended[axis] = from[axis] + (amount * (to[axis] - from[axis]));
  return blended;
}

// Every combination of steps + 1 samples along each axis. This is the only part of a fit that looks everywhere,
// and it is coarse -- its job is to find the basin the simplex then sharpens inside. Walked as an odometer
// rather than a nest of loops so that the axis count belongs to the caller, with the last axis moving fastest.
template <std::size_t N, typename ScoreFunc> void SweepMacroFitGrid(const std::array<MacroFitAxis, N>& axes, int steps, ScoreFunc&& score) {
  const double divisor = static_cast<double>(std::max(steps, 1));

  std::array<int, N> stepIndices{};
  for (;;) {
    MacroFitPoint<N> point{};
    for (std::size_t axis = 0; axis < N; ++axis) point[axis] = axes[axis].min + ((axes[axis].max - axes[axis].min) * (stepIndices[axis] / divisor));
    score(point);

    int axis = static_cast<int>(N) - 1;
    for (; axis >= 0 && stepIndices[axis] >= steps; --axis) stepIndices[axis] = 0;
    if (axis < 0) return;

    ++stepIndices[axis];
  }
}

// A Nelder-Mead simplex: a shape of N + 1 points that reflects the worst of itself through the rest, stretching
// along whatever direction pays and folding up when none does.
//
// The axes have to move together rather than one at a time, because macro knobs trade against each other -- a
// narrower curve bowed to hold its level longer looks much like a wider one that drops away sooner. The
// residual's valleys therefore run diagonally through all of them, and sweeping one axis at a time walks into a
// wall rather than running out of resolution.
//
// The initial step per axis matters, because the axes are not equally sharp: a step of a width knob moves
// harmonics on and off the end of the series, where a shape knob only bends what is already there. So this is
// per-axis rather than one number, and it is the caller's to tune.
//
// Points outside 0..1 are not prevented. A scoring function is expected to clamp, so the simplex may walk past
// an edge and be drawn back rather than having to know where the edges are.
template <std::size_t N, typename ScoreFunc>
void SearchMacroFitSimplex(const MacroFitPoint<N>& start, const MacroFitPoint<N>& steps, ScoreFunc&& score, double smallestSimplex = 1.0e-4) {
  constexpr int kIterations = 200;

  std::array<MacroFitPoint<N>, N + 1> points{};
  std::array<double, N + 1> residuals{};

  points[0] = start;
  for (std::size_t axis = 0; axis < N; ++axis) {
    points[axis + 1] = start;
    points[axis + 1][axis] += (start[axis] > 0.5) ? -steps[axis] : steps[axis];
  }
  for (std::size_t index = 0; index < points.size(); ++index) residuals[index] = score(points[index]);

  for (int iteration = 0; iteration < kIterations; ++iteration) {
    std::size_t lowest = 0;
    std::size_t highest = 0;
    for (std::size_t index = 1; index < residuals.size(); ++index) {
      if (residuals[index] < residuals[lowest]) lowest = index;
      if (residuals[index] > residuals[highest]) highest = index;
    }

    std::size_t nextHighest = (highest == 0) ? 1 : 0;
    for (std::size_t index = 0; index < residuals.size(); ++index) {
      if (index != highest && residuals[index] > residuals[nextHighest]) nextHighest = index;
    }

    MacroFitPoint<N> centroid{};
    for (std::size_t index = 0; index < points.size(); ++index) {
      if (index == highest) continue;

      for (std::size_t axis = 0; axis < N; ++axis) centroid[axis] += points[index][axis];
    }
    for (double& value : centroid) value /= static_cast<double>(N);

    double spread = 0.0;
    for (std::size_t axis = 0; axis < N; ++axis) spread = std::max(spread, std::fabs(points[highest][axis] - centroid[axis]));
    if (spread < smallestSimplex) break;

    const MacroFitPoint<N> reflected = BlendMacroFitPoints(points[highest], centroid, 2.0);
    const double reflectedResidual = score(reflected);

    if (reflectedResidual < residuals[lowest]) {
      // Reflecting beat everything, so the direction is worth following further than the simplex is wide.
      const MacroFitPoint<N> stretched = BlendMacroFitPoints(points[highest], centroid, 3.0);
      const double stretchedResidual = score(stretched);
      const bool stretch = stretchedResidual < reflectedResidual;

      points[highest] = stretch ? stretched : reflected;
      residuals[highest] = stretch ? stretchedResidual : reflectedResidual;
    } else if (reflectedResidual < residuals[nextHighest]) {
      points[highest] = reflected;
      residuals[highest] = reflectedResidual;
    } else {
      const MacroFitPoint<N> folded = BlendMacroFitPoints(points[highest], centroid, 0.5);
      const double foldedResidual = score(folded);

      if (foldedResidual < residuals[highest]) {
        points[highest] = folded;
        residuals[highest] = foldedResidual;
      } else {
        // Nothing along that direction helped, so the valley must be narrower than the simplex: shrink it.
        for (std::size_t index = 0; index < points.size(); ++index) {
          if (index == lowest) continue;

          points[index] = BlendMacroFitPoints(points[lowest], points[index], 0.5);
          residuals[index] = score(points[index]);
        }
      }
    }
  }
}
} // namespace editor
} // namespace plugin_ui
