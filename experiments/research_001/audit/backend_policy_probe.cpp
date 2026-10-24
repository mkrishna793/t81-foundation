// Audit reproducer for the merged #563 backend, not an acceptance test.
#include "t81/ai_backend/controlled_ai_backend.hpp"
#include "t81/axion/policy_engine.hpp"
#include <iostream>
#include <memory>
#include <optional>

using namespace t81::axion;
using namespace t81::ai_backend;

class ProbeEngine final : public PolicyEngine {
 public:
  explicit ProbeEngine(VerdictKind kind) : PolicyEngine(std::nullopt), kind_(kind) {}
  Verdict evaluate(const SyscallContext& ctx) override {
    std::cout << "syscall=" << ctx.syscall << " caller_empty=" << ctx.caller.empty()
              << " payload_empty=" << ctx.payload.empty()
              << " final_authorization=" << ctx.final_authorization << '\n';
    return {kind_, "probe verdict"};
  }
 private:
  VerdictKind kind_;
};

int main() {
  int unsafe = 0;
  for (auto kind : {VerdictKind::Allow, VerdictKind::Deny, VerdictKind::Defer,
                    VerdictKind::Warn, VerdictKind::Quarantine}) {
    ControlledAIBackend backend(std::make_unique<ProbeEngine>(kind));
    auto result = backend.inference({"model", 10, 0.0f, "input"});
    auto evidence = backend.get_evidence_log();
    std::cout << "verdict=" << static_cast<int>(kind) << " success=" << result.success
              << " user_consent=" << evidence.at(0).user_consent
              << " current_policy_records=" << evidence.at(0).policy_decisions.size() << '\n';
    if (kind != VerdictKind::Allow && result.success) ++unsafe;
  }
  ControlledAIBackend no_policy(std::make_unique<PolicyEngine>(std::nullopt));
  auto no_policy_result = no_policy.inference({"model", 10, 0.0f, "input"});
  auto first = no_policy.get_evidence_log();
  no_policy.inference({"model", 10, 0.0f, "input"});
  auto second = no_policy.get_evidence_log();
  std::cout << "real_engine_without_policy_success=" << no_policy_result.success
            << " consent=" << first.at(0).user_consent
            << " previous_record_count=" << second.at(0).policy_decisions.size()
            << " current_record_count=" << second.at(1).policy_decisions.size() << '\n';
  ControlledAIBackend missing(nullptr);
  auto result = missing.inference({"model", 10, 0.0f, "input"});
  std::cout << "null_engine_success=" << result.success << '\n';
  // Success here means the vulnerability was reproduced, not that it is safe.
  return unsafe == 4 && result.success && no_policy_result.success &&
         first.at(0).policy_decisions.empty() &&
         second.at(0).policy_decisions.size() == 1 &&
         second.at(1).policy_decisions.empty() ? 0 : 1;
}
