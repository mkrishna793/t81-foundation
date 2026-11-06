// audit/cpp/int_probe.cpp
// Exhaustive/high-volume differential probe of T81Int<N>.
// Emits only observable behaviour (decimal strings / trit vectors);
// the oracle is Python's unbounded integers.
#include <array>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>

#include "t81/types/T81Int.hpp"

using t81::T81Int;

template <std::size_t N>
static std::string to_decimal_ish(const T81Int<N>& v) {
  try {
    return std::to_string(v.to_int64());
  } catch (...) {
    return "OVERFLOW";
  }
}

template <std::size_t N>
static std::string to_str(const T81Int<N>& v) {
  try {
    return std::to_string(v.to_int64());
  } catch (...) {
    return "OVERFLOW";
  }
}

template <std::size_t N>
static std::string trits(const T81Int<N>& v) {
  std::string s;
  for (std::size_t i = 0; i < N; ++i) {
    int t = static_cast<int>(v[i]);
    s += ' ';
    s += (t == 1 ? '+' : (t == -1 ? '-' : '0'));
  }
  return s;
}

int main(int argc, char** argv) {
  int trials = argc > 1 ? std::atoi(argv[1]) : 200;
  std::mt19937_64 rng(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 99);

  // ---- 1. round-trip from_int64 near the int64 edges ----
  {
    std::printf("SECTION roundtrip_int64\n");
    std::int64_t vals[] = {0, 1, -1, 2, 3, -3, 40, -40, 1215766545905692880LL,
                           -1215766545905692880LL, 2147483647, -2147483648LL,
                           9223372036854775807LL, -9223372036854775807LL - 1};
    for (auto v : vals) {
      T81Int<48> x(v);
      std::printf("RT %lld %s\n", static_cast<long long>(v), to_str(x).c_str());
    }
  }

  // ---- 2. multiplication by 3^k consistency with shifts ----
  {
    std::printf("SECTION mul_pow3\n");
    std::uniform_int_distribution<int> d(-1000, 1000);
    for (int k = 0; k < trials; ++k) {
      std::int64_t a = d(rng);
      int s = (k % 20) + 1;
      T81Int<96> x(a);
      T81Int<96> shifted = x << s;
      T81Int<96> three(s == 0 ? 1 : 1);
      std::printf("SHIFT %lld %d %s\n", static_cast<long long>(a), s,
                  to_str(shifted).c_str());
    }
  }

  // ---- 3. div/mod identity: a == (a/b)*b + (a%b) ----
  {
    std::printf("SECTION divmod\n");
    std::uniform_int_distribution<std::int64_t> da(-1000000, 1000000);
    std::uniform_int_distribution<std::int64_t> db(-1000, 1000);
    for (int k = 0; k < trials; ++k) {
      std::int64_t a = da(rng), b = db(rng);
      if (b == 0) continue;
      T81Int<96> A(a), B(b);
      T81Int<96> q = A / B, r = A % B;
      std::printf("DIVMOD %lld %lld %s %s\n", static_cast<long long>(a),
                  static_cast<long long>(b), to_str(q).c_str(), to_str(r).c_str());
    }
  }

  // ---- 4. wide multiplication (exceeds int64) reported as trit vector ----
  {
    std::printf("SECTION widemul\n");
    std::uniform_int_distribution<int> d(-1, 1);
    for (int k = 0; k < trials; ++k) {
      T81Int<64> A, B;
      for (std::size_t i = 0; i < 64; ++i) {
        A[i] = static_cast<t81::Trit>(d(rng));
        B[i] = static_cast<t81::Trit>(d(rng));
      }
      T81Int<64> P = A * B;
      std::printf("WIDE%s |%s |%s\n", trits(P).c_str(), trits(A).c_str(),
                  trits(B).c_str());
    }
  }

  // ---- 5. comparison / ordering ----
  {
    std::printf("SECTION cmp\n");
    std::uniform_int_distribution<int> d(-500, 500);
    for (int k = 0; k < trials; ++k) {
      int a = d(rng), b = d(rng);
      T81Int<32> A(a), B(b);
      int c = (A < B) ? -1 : (A > B ? 1 : 0);
      std::printf("CMP %d %d %d\n", a, b, c);
    }
  }

  // ---- 6. float/rational conversion surface is handled in a later probe ----
  return 0;
}
