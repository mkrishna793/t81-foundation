// audit/cpp/limb_probe.cpp
// Emits arithmetic results as trit vectors so an INDEPENDENT big-integer oracle
// (Python) can validate them. The repo's own "reference_*" functions are NOT
// trusted here: they share helpers (decode_tryte/from_trits) with the code
// under test and therefore cannot falsify encoding-level bugs.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "t81/types/T81Limb.hpp"

using t81::core::T81Limb;
using t81::core::T81Limb27;
using t81::core::T81Limb54;

static void emit(const char* tag, const int8_t* t, int n) {
  std::printf("%s", tag);
  for (int i = 0; i < n; ++i) std::printf(" %d", static_cast<int>(t[i]));
  std::printf("\n");
}

template <std::size_t N>
static void emitv(const char* tag, const std::array<int8_t, N>& t) {
  std::printf("%s", tag);
  for (int i = 0; i < N; ++i) std::printf(" %d", static_cast<int>(t[i]));
  std::printf("\n");
}

template <std::size_t N>
static std::array<int8_t, N> rand_trits(std::mt19937_64& rng, int lo, int hi) {
  std::array<int8_t, N> t{};
  std::uniform_int_distribution<int> d(lo, hi);
  for (auto& x : t) x = static_cast<int8_t>(d(rng));
  return t;
}

int main(int argc, char** argv) {
  int trials = argc > 1 ? std::atoi(argv[1]) : 300;
  std::mt19937_64 rng(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 12345);
  std::printf("# trials=%d\n", trials);

  // ---------- 48-trit limb ----------
  for (int k = 0; k < trials; ++k) {
    auto ta = rand_trits<T81Limb::TRITS>(rng, -1, 1);
    auto tb = rand_trits<T81Limb::TRITS>(rng, -1, 1);
    T81Limb a = T81Limb::from_trits(ta);
    T81Limb b = T81Limb::from_trits(tb);

    // encoding round-trip (must be identity on canonical trits)
    emitv("ENC_A", a.to_trits());
    emitv("ENC_B", b.to_trits());

    auto [sum, cout] = a.addc(b);
    emitv("ADD", sum.to_trits());
    std::printf("CARRY %d\n", static_cast<int>(cout));

    auto [lo, hi] = T81Limb::mul_wide(a, b);
    emitv("MUL_LOW", lo.to_trits());
    emitv("MUL_HIGH", hi.to_trits());

    T81Limb prod = a * b;  // operator* -> mul_booth_karatsuba -> mul_wide().first
    emitv("MUL_TRUNC", prod.to_trits());

    T81Limb bm = T81Limb::booth_mul(a, b);
    emitv("BOOTH", bm.to_trits());
    std::printf("---\n");
  }

  // ---------- 54-trit limb ----------
  for (int k = 0; k < trials; ++k) {
    auto ta = rand_trits<T81Limb54::TRITS>(rng, -1, 1);
    auto tb = rand_trits<T81Limb54::TRITS>(rng, -1, 1);
    T81Limb54 a = T81Limb54::from_trits(ta);
    T81Limb54 b = T81Limb54::from_trits(tb);
    emitv("L54_A", a.to_trits());
    emitv("L54_B", b.to_trits());
    auto [s, c] = a.addc(b);
    emitv("L54_ADD", s.to_trits());
    std::printf("L54_CARRY %d\n", static_cast<int>(c));
    emitv("L54_SUB", (a - b).to_trits());
    emitv("L54_KARATSUBA", (a * b).to_trits());
    emitv("L54_BOOTH", T81Limb54::booth_mul(a, b).to_trits());
    emitv("L54_REFMUL", T81Limb54::reference_mul(a, b).to_trits());
    std::printf("---\n");
  }

  // ---------- 27-trit x 27-trit -> 54 trit ----------
  for (int k = 0; k < trials; ++k) {
    auto ta = rand_trits<T81Limb::TRITS>(rng, -1, 1);
    auto tb = rand_trits<T81Limb::TRITS>(rng, -1, 1);
    // low 27 trits of a 48-trit limb form a T81Limb27 block
    T81Limb la = T81Limb::from_trits(ta);
    T81Limb lb = T81Limb::from_trits(tb);
    T81Limb27 a = T81Limb27::from_low_block(la);
    T81Limb27 b = T81Limb27::from_low_block(lb);
    emitv("L27_A", la.to_trits());
    emitv("L27_B", lb.to_trits());
    T81Limb54 p = a * b;
    emitv("L27_MUL", p.to_trits());
    std::printf("---\n");
  }

  // ---------- tryte encode/decode sweep (exhaustive) ----------
  for (int v = -13; v <= 13; ++v) {
    int8_t d[3];
    t81::decode_tryte(static_cast<int8_t>(v), d);
    int8_t back = 0;
    t81::encode_tryte(d, back);
    std::printf("TRYTE %d %d %d %d %d\n", v, (int)d[0], (int)d[1], (int)d[2], (int)back);
  }
  return 0;
}
