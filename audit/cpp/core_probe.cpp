// audit/cpp/core_probe.cpp
// Differential probe of the production numeric stack:
//   T81Int<N> -> T81BigInt -> T81Fraction
// Every result is printed so Python's unbounded ints / Fraction can act as oracle.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include "t81/types/T81BigInt.hpp"
#include "t81/types/T81Fraction.hpp"
#include "t81/fraction.hpp"

using t81::v1::T81BigInt;
namespace frac = t81::v1;

static std::string S(const T81BigInt& v) {
  try { return v.to_string(); } catch (const std::exception& e) { return std::string("ERR:"); }
}

int main(int argc, char** argv) {
  int trials = argc > 1 ? std::atoi(argv[1]) : 100;
  std::mt19937_64 rng(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 2024);

  // ---------- BigInt basic algebra on small magnitudes ----------
  std::printf("SECTION bigint_small\n");
  {
    std::uniform_int_distribution<std::int64_t> d(-1000000000LL, 1000000000LL);
    for (int k = 0; k < trials; ++k) {
      std::int64_t a = d(rng), b = d(rng);
      T81BigInt A(a), B(b);
      std::printf("BIG %lld %lld %s %s %s\n", (long long)a, (long long)b,
                  S(A + B).c_str(), S(A - B).c_str(), S(A * B).c_str());
    }
  }

  // ---------- BigInt at magnitudes that force the multi-limb path ----------
  std::printf("SECTION bigint_wide\n");
  {
    // values ~ 3^40 and ~3^80
    T81BigInt b40(1);
    for (int i = 0; i < 40; ++i) b40 = b40 * T81BigInt(3);
    T81BigInt b80(1);
    for (int i = 0; i < 80; ++i) b80 = b80 * T81BigInt(3);
    std::uniform_int_distribution<std::int64_t> d(-1000, 1000);
    for (int k = 0; k < trials; ++k) {
      T81BigInt a = b40 + T81BigInt(d(rng));
      T81BigInt b = b80 + T81BigInt(d(rng));
      std::printf("WIDE3 %s %s %s %s\n", S(a).c_str(), S(b).c_str(),
                  S(a * b).c_str(), S(a - b).c_str());
    }
  }

  // ---------- deliberate overflow-boundary probe of the mul fast path ----------
  std::printf("SECTION mul_boundary\n");
  {
    // 41 significant trits each -> ta+tb = 82 (> 81) => exercises guarded branch
    T81BigInt t40(1);
    for (int i = 0; i < 40; ++i) t40 = t40 * T81BigInt(3);
    for (int da = -2; da <= 2; ++da) {
      for (int db = -2; db <= 2; ++db) {
        T81BigInt A = t40 + T81BigInt(da);
        T81BigInt B = t40 + T81BigInt(db);
        bool threw = false;
        std::string prod = "THREW";
        try { prod = S(A * B); } catch (const std::exception& e) { threw = true; }
        std::printf("BND %s %s %s %d\n", S(A).c_str(), S(B).c_str(), prod.c_str(),
                    (int)threw);
      }
    }
  }

  // ---------- VM fraction type: t81::T81Fraction (T81BigInt-backed) ----------
  std::printf("SECTION fraction_bigint\n");
  {
    std::uniform_int_distribution<std::int64_t> n(-1000, 1000);
    std::uniform_int_distribution<std::int64_t> q(1, 500);
    for (int k = 0; k < trials; ++k) {
      std::int64_t a = n(rng), b = q(rng), c = n(rng), d = q(rng);
      t81::T81Fraction A{t81::T81BigInt(a), t81::T81BigInt(b)};
      t81::T81Fraction B{t81::T81BigInt(c), t81::T81BigInt(d)};
      auto s = t81::T81Fraction::add(A, B);
      auto sb = t81::T81Fraction::sub(A, B);
      auto m = t81::T81Fraction::mul(A, B);
      int cmp = 0;
      std::string dv = "THREW";
      try { dv = t81::T81Fraction::div(A, B).to_string(); } catch (...) {}
      if (b != 0 && d != 0) cmp = t81::T81Fraction::cmp(A, B);
      std::printf("FRAC %lld %lld %lld %lld | %s | %s | %s | %s | %d\n", (long long)a,
                  (long long)b, (long long)c, (long long)d, s.to_string().c_str(),
                  sb.to_string().c_str(), m.to_string().c_str(), dv.c_str(), cmp);
    }
  }

  // ---------- T81Fraction<81>: T81Int-backed exact rationals ----------
  std::printf("SECTION fraction_int81\n");
  {
    using F81 = t81::v1::T81Fraction<81>;
    std::uniform_int_distribution<std::int64_t> n(-10000, 10000);
    std::uniform_int_distribution<std::int64_t> q(1, 5000);
    for (int k = 0; k < trials; ++k) {
      std::int64_t a = n(rng), b = q(rng), c = n(rng), d = q(rng);
      F81 A{t81::T81Int<81>(a), t81::T81Int<81>(b)};
      F81 B{t81::T81Int<81>(c), t81::T81Int<81>(d)};
      auto s = A + B;
      auto sb = A - B;
      auto m = A * B;
      long long sn = s.num().to_int64(), sd = s.den().to_int64();
      long long mn = m.num().to_int64(), md = m.den().to_int64();
      std::printf("F81 %lld %lld %lld %lld | %lld/%lld | %lld/%lld\n", (long long)a,
                  (long long)b, (long long)c, (long long)d, sn, sd, mn, md);
    }
  }
  return 0;
}
