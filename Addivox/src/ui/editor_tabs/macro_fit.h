#pragma once

#include "../../settings/oscillator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Shared grid and simplex searches. Callers score candidates and retain their best fit.
// Keep this header independent of the UI so numerical harnesses can include it directly.
namespace plugin_ui {
namespace editor {
using OscillatorParameterValues = CompoundPatch::OscillatorParameterValues;

inline constexpr double kMacroEpsilon = 1.0e-12;

// Map 0..1 travel: exponents above 1 give the lower end more room.
inline double BendMacroTravel(double knobValue, double exponent) {
  const double clamped = std::clamp(knobValue, 0.0, 1.0);
  return (exponent == 1.0) ? clamped : std::pow(clamped, exponent);
}

// Preserve the centre while bending each half of the travel.
inline double BendMacroTravelAboutCentre(double knobValue, double exponent) {
  const double fromCentre = (2.0 * std::clamp(knobValue, 0.0, 1.0)) - 1.0;
  return 0.5 + (0.5 * std::copysign(BendMacroTravel(std::fabs(fromCentre), exponent), fromCentre));
}

// Weight by 1 / (value + floor)^2 so quiet harmonics contribute to the fit.
// The floor is relative to the peak (1e-3 is 60 dB down).
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

// Search coordinates; callers map these to their model or knob units.
template <std::size_t N> using MacroFitPoint = std::array<double, N>;

struct MacroFitAxis {
  double min{0.0};
  double max{1.0};
};

template <std::size_t N> inline MacroFitPoint<N> BlendMacroFitPoints(const MacroFitPoint<N>& from, const MacroFitPoint<N>& to, double amount) {
  MacroFitPoint<N> blended{};
  for (std::size_t axis = 0; axis < N; ++axis) blended[axis] = from[axis] + (amount * (to[axis] - from[axis]));
  return blended;
}

// Visit (steps + 1)^N grid points, with the last axis moving fastest.
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

// Nelder-Mead with per-axis initial steps. The scorer must clamp out-of-range coordinates.
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
