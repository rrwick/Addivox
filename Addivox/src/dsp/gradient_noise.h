#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace dsp {
inline double Quintic(double t) {
  // Perlin fade curve for smooth interpolation.
  return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

inline uint32_t HashUint32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7FEB352Du;
  x ^= x >> 15;
  x *= 0x846CA68Bu;
  x ^= x >> 16;
  return x;
}

inline double HashToSignedUnitFloat(uint32_t x) { return (static_cast<double>(x) / 4294967295.0) * 2.0 - 1.0; }

inline double GradientNoise1D(double position, uint32_t seed) {
  const int lattice0 = static_cast<int>(std::floor(position));
  const double t = position - static_cast<double>(lattice0);
  const double fade = Quintic(t);

  const double gradient0 = HashToSignedUnitFloat(HashUint32(static_cast<uint32_t>(lattice0) ^ seed));
  const double gradient1 = HashToSignedUnitFloat(HashUint32(static_cast<uint32_t>(lattice0 + 1) ^ seed));
  const double value0 = gradient0 * t;
  const double value1 = gradient1 * (t - 1.0);
  const double blended = value0 + ((value1 - value0) * fade);
  return std::clamp(blended * 1.8, -1.0, 1.0);
}
struct CachedNoise1D {
  double evaluate(double position, uint32_t seed) {
    const int lattice = static_cast<int>(std::floor(position));
    if (lattice != cachedLattice) {
      if (lattice == cachedLattice + 1)
        gradient0 = gradient1;
      else
        gradient0 = HashToSignedUnitFloat(HashUint32(static_cast<uint32_t>(lattice) ^ seed));
      cachedLattice = lattice;
      gradient1 = HashToSignedUnitFloat(HashUint32(static_cast<uint32_t>(lattice + 1) ^ seed));
    }
    const double t = position - static_cast<double>(cachedLattice);
    const double fade = Quintic(t);
    return std::clamp((gradient0 * t + (gradient1 * (t - 1.0) - gradient0 * t) * fade) * 1.8, -1.0, 1.0);
  }

  void reset() { cachedLattice = std::numeric_limits<int>::min(); }

  int cachedLattice{std::numeric_limits<int>::min()};
  double gradient0{0.0};
  double gradient1{0.0};
};
} // namespace dsp
