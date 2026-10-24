#include "t81/ai_backend/controlled_ai_backend.hpp"
#include "t81/axion/policy_engine.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace t81::ai_backend;
using namespace t81::axion;

namespace {
void require(bool value, const std::string& message) {
  if (!value) throw std::runtime_error(message);
}

const std::string reason = "policy \"decision\"\nwith\\context\t";

class ScriptedEngine final : public PolicyEngine {
public:
  explicit ScriptedEngine(std::vector<VerdictKind> kinds, bool has_policy = true)
      : PolicyEngine(has_policy ? std::optional<Policy>(Policy{}) : std::nullopt),
        kinds_(std::move(kinds)) {}
  Verdict evaluate(const SyscallContext& ctx) override {
    require(ctx.policy == active_policy(), "context must reference engine-owned policy");
    require(ctx.final_authorization, "authorization must complete evidence requirements");
    require(ctx.next_opcode == t81::tisc::Opcode::Nop, "do not invent a VM instruction");
    require(ctx.caller == "controlled_ai_backend", "truthful wrapper caller");
    require(ctx.syscall == "ai.inference.inference", "preserve syscall name");
    require(ctx.payload.starts_with("sha3-512:"), "bind the request identity");
    payloads.push_back(ctx.payload);
    if (throws) throw std::runtime_error(reason);
    if (throws_unknown) throw 42;
    return {kinds_.at(calls++), reason};
  }
  bool throws = false;
  bool throws_unknown = false;
  size_t calls = 0;
  std::vector<std::string> payloads;

private:
  std::vector<VerdictKind> kinds_;
};

void blocked(const InferenceResult& result, const AIOperationEvidence& evidence) {
  require(!result.success && result.output_tokens == 0 && result.output_text.empty(),
          "failure must not return inference output");
  require(!evidence.inference_dispatched && !evidence.external_ai_used,
          "failure must stop before either dispatch routine");
  require(!evidence.metadata.contains("ai_service"), "no dispatch routine may have run");
  require(!evidence.user_consent, "policy approval must never fabricate user consent");
  require(!result.error_message.empty() && result.error_message == evidence.failure_reason,
          "failure reason must be attributed to this operation");
  require(evidence.metadata.at("success") == "false", "truthful failure metadata");
  require(evidence.policy_decisions.size() == 1, "each operation needs its own decision");
}

AIBackendConfig policy_only(const std::string& mode = "strict", bool external = false) {
  AIBackendConfig config;
  // Explicitly select the existing policy-only option; every authorization
  // requirement remains enforced. This is not a consent proof.
  config.require_user_consent = false;
  config.determinism_level = mode;
  if (external) {
    config.external_ai_endpoint = "https://simulated.invalid/ai";
    config.external_ai_auth_token = "secret-not-evidence";
  }
  return config;
}

void verdict_matrix() {
  for (const auto& mode : {"strict", "controlled", "permissive"}) {
    for (bool external : {false, true}) {
      for (auto kind : {VerdictKind::Deny, VerdictKind::Defer, VerdictKind::Warn,
                        VerdictKind::Quarantine, static_cast<VerdictKind>(99)}) {
        auto backend = create_controlled_ai_backend(
            std::make_unique<ScriptedEngine>(std::vector{kind}), policy_only(mode, external));
        auto result = backend->inference({"model", 10, 0.0f, "input"});
        const auto evidence = backend->get_evidence_log().at(0);
        blocked(result, evidence);
        require(!evidence.policy_authorized, "non-Allow is never authorization");
        require(evidence.authorization_verdict && evidence.authorization_verdict->kind == kind &&
                    evidence.authorization_verdict->reason == reason,
                "preserve exact verdict");
      }
      for (bool null_engine : {false, true}) {
        std::unique_ptr<PolicyEngine> engine;
        if (!null_engine) engine = std::make_unique<PolicyEngine>(std::nullopt);
        auto backend = create_controlled_ai_backend(std::move(engine), policy_only(mode, external));
        auto result = backend->inference({"model", 10, 0.0f, "input"});
        const auto evidence = backend->get_evidence_log().at(0);
        blocked(result, evidence);
        require(evidence.metadata.at("active_policy") == "false", "record missing policy");
        if (null_engine) {
          require(!evidence.authorization_verdict, "null engine produced no verdict");
        } else {
          require(evidence.authorization_verdict &&
                      evidence.authorization_verdict->kind == VerdictKind::Deny &&
                      evidence.authorization_verdict->reason ==
                          "Final authorization requires active policy",
                  "use real Axion final-authorization rejection");
        }
      }
    }
  }
  // Even a custom Allow engine cannot make an absent configured policy sufficient.
  auto backend = create_controlled_ai_backend(
      std::make_unique<ScriptedEngine>(std::vector{VerdictKind::Allow}, false), policy_only());
  auto result = backend->inference({"model", 10, 0.0f, "input"});
  blocked(result, backend->get_evidence_log().at(0));
}

void final_evidence_and_exceptions() {
  Policy policy;
  policy.axion_event_requirements.push_back({"required-operation-evidence"});
  auto backend =
      create_controlled_ai_backend(std::make_unique<PolicyEngine>(policy), policy_only());
  auto result = backend->inference({"model", 10, 0.0f, "input"});
  auto evidence = backend->get_evidence_log().at(0);
  blocked(result, evidence);
  require(evidence.authorization_verdict && evidence.authorization_verdict->reason.find(
                                                "Missing Axion event") != std::string::npos,
          "final authorization must enforce required trace evidence");
  for (bool unknown : {false, true}) {
    auto engine = std::make_unique<ScriptedEngine>(std::vector<VerdictKind>{});
    engine->throws = !unknown;
    engine->throws_unknown = unknown;
    auto failing = create_controlled_ai_backend(std::move(engine), policy_only());
    auto failure = failing->inference({"model", 10, 0.0f, "input"});
    auto record = failing->get_evidence_log().at(0);
    blocked(failure, record);
    require(!record.authorization_verdict, "throwing engine produced no verdict");
    require(record.policy_decisions.at(0).starts_with("EvaluationError:"),
            "record evaluation exception");
    if (!unknown)
      require(record.failure_reason.find(reason) != std::string::npos, "preserve exception");
  }
}

void consent_and_authorized_success() {
  for (bool external : {false, true}) {
    auto config = policy_only("strict", external);
    config.require_user_consent = true;
    config.evidence_collection = false;  // Must not suppress denied security evidence.
    auto backend = create_controlled_ai_backend(std::make_unique<PolicyEngine>(Policy{}), config);
    auto result = backend->inference({"model", 10, 0.0f, "input"});
    auto evidence = backend->get_evidence_log().at(0);
    blocked(result, evidence);
    require(
        evidence.policy_authorized && evidence.authorization_verdict->kind == VerdictKind::Allow,
        "policy Allow is distinct from the blocking consent requirement");
    require(result.error_message.starts_with("ERR_AI_CONSENT_UNSUPPORTED"),
            "consent remains unsupported");

    auto authorized = create_controlled_ai_backend(std::make_unique<PolicyEngine>(Policy{}),
                                                   policy_only("strict", external));
    auto success = authorized->inference({"model", 10, 0.0f, "input"});
    auto record = authorized->get_evidence_log().at(0);
    require(success.success && success.output_tokens == 10 && !success.output_text.empty(),
            "explicit configured policy and real Allow authorize simulated dispatch");
    require(record.policy_authorized && record.inference_dispatched && !record.user_consent,
            "success is policy-only, never verified consent");
    require(record.external_ai_used == external && record.failure_reason.empty(),
            "truthful success");
    require(record.policy_decisions.size() == 1, "successful decision is also recorded");
  }
  ControlledAIBackend defaults(std::make_unique<PolicyEngine>(Policy{}));
  auto denied = defaults.inference({"model", 10, 0.0f, "input"});
  blocked(denied, defaults.get_evidence_log().at(0));
}

void evidence_isolation_and_request_binding() {
  auto engine = std::make_unique<ScriptedEngine>(std::vector{VerdictKind::Allow, VerdictKind::Deny,
                                                             VerdictKind::Allow, VerdictKind::Allow,
                                                             VerdictKind::Allow});
  auto* probe = engine.get();
  auto backend = create_controlled_ai_backend(std::move(engine), policy_only());
  backend->inference({"model", 10, 0.0f, "first"});
  const auto first_json = backend->get_evidence_log().at(0).to_json();
  auto denied = backend->inference({"model", 10, 0.0f, "second"});
  auto records = backend->get_evidence_log();
  blocked(denied, records.at(1));
  require(records.at(0).to_json() == first_json, "later decision must not mutate prior evidence");
  require(records.at(0).operation_id != records.at(1).operation_id, "unique operation attribution");
  backend->inference({"other-model", 10, 0.0f, "second"});
  backend->set_determinism_level("controlled");
  backend->inference({"other-model", 10, 0.0f, "second"});
  backend->enable_external_ai("https://simulated.invalid/ai", "secret");
  backend->inference({"other-model", 10, 0.0f, "second"});
  for (size_t i = 1; i < probe->payloads.size(); ++i) {
    require(probe->payloads[i] != probe->payloads[i - 1],
            "identity binds changed request/mode/target");
  }
  require(backend->get_evidence_log().at(4).external_ai_used,
          "external dispatch authorized independently");
}
}  // namespace

int main(int argc, char**) {
  try {
    if (argc > 1) {
      auto backend = create_controlled_ai_backend(
          std::make_unique<ScriptedEngine>(std::vector{VerdictKind::Quarantine}), policy_only());
      backend->inference({"model\"\\\n\t", 10, 0.0f, "private prompt"});
      std::cout << backend->get_evidence_json();
      return 0;
    }
    verdict_matrix();
    std::cout << "PASS: 30 non-Allow + 12 missing-engine/policy dispatch combinations\n";
    final_evidence_and_exceptions();
    std::cout << "PASS: final evidence and authorization exceptions\n";
    consent_and_authorized_success();
    std::cout << "PASS: consent blocks; explicit policy-only Allow dispatches\n";
    evidence_isolation_and_request_binding();
    std::cout << "PASS: per-operation evidence and request identity\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
