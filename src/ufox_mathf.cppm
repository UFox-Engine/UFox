module;

#include <glm/glm.hpp>   // only for the glm::vec2 type, no glm math is used
#include <cmath>         // sqrt only (SafeNormalize)
#include <concepts>
#include <numbers>
#include <type_traits>

export module ufox_mathf;

// No imports from other ufox modules, so ufox_lib / ufox_gpu_lib can import this safely.
// No export macros: everything is constexpr / inline / template (compiled in the consumer).

export namespace ufox::mathf {

  // ───────────────────────── Concepts & constants ─────────────────────────

  template<typename T>
  concept Arithmetic = std::is_arithmetic_v<T>;

  inline constexpr float PI      = std::numbers::pi_v<float>;
  inline constexpr float TAU     = 2.0f * PI;
  inline constexpr float Deg2Rad = PI / 180.0f;
  inline constexpr float Rad2Deg = 180.0f / PI;
  inline constexpr float EPSILON = 1e-6f;

  // ───────────────────────── Basics ─────────────────────────

  constexpr float Min(const float a, const float b) noexcept { return a < b ? a : b; }
  constexpr float Max(const float a, const float b) noexcept { return a > b ? a : b; }
  constexpr float Abs(const float v) noexcept { return v < 0.0f ? -v : v; }
  constexpr float Sign(const float v) noexcept { return v > 0.0f ? 1.0f : (v < 0.0f ? -1.0f : 0.0f); }

  // NaN / Inf check without std::isnan (constexpr-friendly). Unreliable under -ffast-math.
  constexpr bool IsFinite(const float v) noexcept { return v == v && (v - v) == 0.0f; }
  constexpr bool IsFinite(const glm::vec2& v) noexcept { return IsFinite(v.x) && IsFinite(v.y); }

  constexpr float OneMinus(const float value) noexcept { return 1.0f - value; }

  template<typename T>
    requires (std::is_arithmetic_v<T> && !std::same_as<T, float>)
  constexpr float OneMinus(const T value) noexcept {
    return OneMinus(static_cast<float>(value));
  }

[[nodiscard]] constexpr float Delta(const float newValue, const float oldValue) noexcept {
    return newValue - oldValue;
  }

  constexpr void UpdateDelta(float& value, const float newValue, const float oldValue) noexcept {
      value = newValue - oldValue;
  }
  template<typename V>
    constexpr void UpdateDelta(V& value, const V& newPos, const V& oldPos) noexcept {
      value = newPos - oldPos;
    }

  // ───────────────────────── Safe Mul / Divide ─────────────────────────

  constexpr float Mul(const float a, const float b) noexcept {
    return a == 0.0f || b == 0.0f ? 0.0f : a * b;
  }

  constexpr float Divide(const float a, const float b) noexcept {
    return a == 0.0f || b == 0.0f ? 0.0f : a / b;
  }

  template<typename T, typename U>
    requires (std::is_arithmetic_v<T> && std::is_arithmetic_v<U> &&
              !(std::same_as<T, float> && std::same_as<U, float>))
  constexpr float Mul(const T a, const U b) noexcept {
    return Mul(static_cast<float>(a), static_cast<float>(b));
  }

  template<typename T, typename U>
    requires (std::is_arithmetic_v<T> && std::is_arithmetic_v<U> &&
              !(std::same_as<T, float> && std::same_as<U, float>))
  constexpr float Divide(const T a, const U b) noexcept {
    return Divide(static_cast<float>(a), static_cast<float>(b));
  }

  // Divide with an explicit fallback when the divisor is ~0 (unlike Divide, a == 0 is computed normally)
  constexpr float SafeDivide(const float a, const float b, const float fallback = 0.0f) noexcept {
    return Abs(b) < EPSILON ? fallback : a / b;
  }

  // ───────────────────────── Rounding (constexpr, no <cmath>) ─────────────────────────
  // Valid for |v| < ~1e15. Non-finite or huge inputs are returned unchanged / as 0 for int results.

  constexpr float Floor(const float v) noexcept {
    if (!IsFinite(v) || Abs(v) >= 1e15f) return v;
    const auto i = static_cast<long long>(v);
    return static_cast<float>(static_cast<float>(i) > v ? i - 1 : i);
  }

  constexpr float Ceil(const float v) noexcept {
    if (!IsFinite(v) || Abs(v) >= 1e15f) return v;
    const auto i = static_cast<long long>(v);
    return static_cast<float>(static_cast<float>(i) < v ? i + 1 : i);
  }

  // Half away from zero (same as std::lround)
  constexpr float Round(const float v) noexcept {
    if (!IsFinite(v) || Abs(v) >= 1e15f) return v;
    return static_cast<float>(static_cast<long long>(v + (v < 0.0f ? -0.5f : 0.5f)));
  }

  constexpr long RoundToNearest(const float v) noexcept {
    return IsFinite(v) ? static_cast<long>(Round(v)) : 0L;
  }

  constexpr int RoundToInt(const float v) noexcept {
    return IsFinite(v) ? static_cast<int>(Round(v)) : 0;
  }

  // float*float -> int (rounded). Ints convert implicitly, so one overload covers all combinations.
  constexpr int MulToInt(const float a, const float b) noexcept { return RoundToInt(Mul(a, b)); }

  constexpr int MulFloor(Arithmetic auto a, Arithmetic auto b) noexcept {
    return static_cast<int>(Floor(Mul(static_cast<float>(a), static_cast<float>(b))));
  }

  constexpr int MulCeil(Arithmetic auto a, Arithmetic auto b) noexcept {
    return static_cast<int>(Ceil(Mul(static_cast<float>(a), static_cast<float>(b))));
  }

  // ───────────────────────── Clamp / Lerp / Remap ─────────────────────────

  // If lo > hi this returns lo for out-of-range-below values; callers should pass lo <= hi.
  constexpr float Clamp(const float v, const float lo, const float hi) noexcept {
    return v < lo ? lo : (v > hi ? hi : v);
  }

  template<Arithmetic T>
    requires (!std::same_as<T, float>)
  constexpr T Clamp(const T v, const T lo, const T hi) noexcept {
    return v < lo ? lo : (v > hi ? hi : v);
  }

  constexpr float Saturate(const float v) noexcept { return Clamp(v, 0.0f, 1.0f); }

  // Unclamped linear interpolation
  constexpr float Lerp(const float a, const float b, const float t) noexcept {
    return a + (b - a) * t;
  }

  constexpr glm::vec2 Lerp(const glm::vec2& a, const glm::vec2& b, const float t) noexcept {
    return {Lerp(a.x, b.x, t), Lerp(a.y, b.y, t)};
  }

  // Clamped interpolation (the old `Mix` behavior)
  constexpr float Mix(const float a, const float b, const float t) noexcept {
    return Lerp(a, b, Saturate(t));
  }

  // Integer mix, rounded (old behavior)
  template<std::integral T>
  constexpr T Mix(const T a, const T b, const float t) noexcept {
    return static_cast<T>(RoundToInt(Lerp(static_cast<float>(a), static_cast<float>(b), Saturate(t))));
  }

  // Where does v sit between a and b (0..1)? Safe when a == b (returns 0).
  constexpr float InverseLerp(const float a, const float b, const float v) noexcept {
    const float d = b - a;
    return Abs(d) < EPSILON ? 0.0f : (v - a) / d;
  }

  // Old `Normalize(value, min, max, clamp)`. Note: zero range returns 0 here (old returned 1 or 0).
  template<Arithmetic U, Arithmetic V>
  constexpr float Normalize(const float value, const U min, const V max, const bool clamp = true) noexcept {
    const float r = InverseLerp(static_cast<float>(min), static_cast<float>(max), value);
    return clamp ? Saturate(r) : r;
  }

  // Old `InvertRatio`: 1 - clamp(ratio)
  constexpr float InvertRatio(const float ratio) noexcept { return OneMinus(Saturate(ratio)); }

  constexpr float Remap(const float v, const float inLo, const float inHi,
                        const float outLo, const float outHi) noexcept {
    return Lerp(outLo, outHi, InverseLerp(inLo, inHi, v));
  }

  constexpr float SmoothStep(const float edge0, const float edge1, const float x) noexcept {
    const float t = Saturate(InverseLerp(edge0, edge1, x));
    return t * t * (3.0f - 2.0f * t);
  }

  constexpr bool NearlyEqual(const float a, const float b, const float eps = EPSILON) noexcept {
    return Abs(a - b) <= eps;
  }

  // ───────────────────────── Wrapping / snapping ─────────────────────────

  // Wrap into [0, length), correct for negatives
  constexpr float Repeat(const float v, const float length) noexcept {
    if (length < EPSILON || !IsFinite(v)) return 0.0f;
    const float r = v - Floor(v / length) * length;
    return r < 0.0f ? r + length : r;
  }

  // Float: nearest grid multiple. grid <= 0 returns the value unchanged.
  constexpr float SnapToGrid(const float value, const float grid) noexcept {
    return grid < EPSILON ? value : Round(value / grid) * grid;
  }

  // Int: lower-or-equal grid multiple toward zero (old behavior). grid == 0 returns the value unchanged.
  constexpr int SnapToGrid(const int value, const int grid) noexcept {
    return grid == 0 ? value : value / grid * grid;
  }

  // Step toward a target without overshooting
  constexpr float MoveTowards(const float current, const float target, const float maxDelta) noexcept {
    const float d = target - current;
    return Abs(d) <= maxDelta ? target : current + Sign(d) * maxDelta;
  }

  // ───────────────────────── Angles ─────────────────────────

  constexpr float DegToRad(const float d) noexcept { return d * Deg2Rad; }
  constexpr float RadToDeg(const float r) noexcept { return r * Rad2Deg; }

  constexpr float WrapDegrees(const float deg) noexcept { return Repeat(deg + 180.0f, 360.0f) - 180.0f; }
  constexpr float WrapRadians(const float rad) noexcept { return Repeat(rad + PI, TAU) - PI; }

  // Shortest signed difference from current to target, degrees (-180..180)
  constexpr float DeltaAngle(const float currentDeg, const float targetDeg) noexcept {
    return WrapDegrees(targetDeg - currentDeg);
  }

  constexpr float LerpAngle(const float aDeg, const float bDeg, const float t) noexcept {
    return aDeg + DeltaAngle(aDeg, bDeg) * t;
  }

  constexpr float MoveTowardsAngle(const float currentDeg, const float targetDeg, const float maxDelta) noexcept {
    const float d = DeltaAngle(currentDeg, targetDeg);
    return Abs(d) <= maxDelta ? targetDeg : currentDeg + Sign(d) * maxDelta;
  }

  // ───────────────────────── Smoothing ─────────────────────────

  // Critically damped spring (Unity-style). Keep one `velocity` per animated value, dt in seconds.
  constexpr float SmoothDamp(const float current, const float target, float& velocity,
                             const float smoothTime, const float deltaTime,
                             const float maxSpeed = 1e9f) noexcept {
    if (deltaTime <= 0.0f) return current;

    const float st    = Max(0.0001f, smoothTime);
    const float omega = 2.0f / st;
    const float x     = omega * deltaTime;
    const float e     = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

    const float maxChange = maxSpeed * st;
    const float change    = Clamp(current - target, -maxChange, maxChange);
    const float clampedTo = current - change;

    const float temp = (velocity + omega * change) * deltaTime;
    velocity         = (velocity - omega * temp) * e;
    float output     = clampedTo + (change + temp) * e;

    if ((target - current > 0.0f) == (output > target)) {
      output   = target;
      velocity = 0.0f;
    }
    return output;
  }

  constexpr glm::vec2 SmoothDamp(const glm::vec2& current, const glm::vec2& target, glm::vec2& velocity,
                                 const float smoothTime, const float deltaTime,
                                 const float maxSpeed = 1e9f) noexcept {
    return {SmoothDamp(current.x, target.x, velocity.x, smoothTime, deltaTime, maxSpeed),
            SmoothDamp(current.y, target.y, velocity.y, smoothTime, deltaTime, maxSpeed)};
  }

  constexpr float SmoothDampAngle(const float currentDeg, const float targetDeg, float& velocity,
                                  const float smoothTime, const float deltaTime,
                                  const float maxSpeed = 1e9f) noexcept {
    return SmoothDamp(currentDeg, currentDeg + DeltaAngle(currentDeg, targetDeg),
                      velocity, smoothTime, deltaTime, maxSpeed);
  }

  // ───────────────────────── vec2 helpers (member access only) ─────────────────────────

  constexpr float Dot(const glm::vec2& a, const glm::vec2& b) noexcept { return a.x * b.x + a.y * b.y; }
  constexpr float LengthSq(const glm::vec2& v) noexcept { return Dot(v, v); }
  constexpr void UpdateDelta(glm::vec2& value, const glm::vec2& newPos, const glm::vec2& oldPos) noexcept {
    value = newPos - oldPos;
  }



  // Not constexpr (std::sqrt). Returns `fallback` for zero-length input instead of NaN.
  inline glm::vec2 SafeNormalize(const glm::vec2& v, const glm::vec2& fallback = {0.0f, 0.0f}) noexcept {
    const float lenSq = LengthSq(v);
    if (!(lenSq >= EPSILON * EPSILON) || !IsFinite(lenSq)) return fallback;
    const float inv = 1.0f / std::sqrt(lenSq);
    return {v.x * inv, v.y * inv};
  }
}
