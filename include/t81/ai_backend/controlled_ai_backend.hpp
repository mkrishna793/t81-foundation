#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "t81/axion/verdict.hpp"

// Forward declaration outside namespace
namespace t81::axion {
class PolicyEngine;
}

namespace t81::ai_backend {

// AI inference request structure
struct InferenceRequest {
  std::string model_hash;
  size_t input_tokens;
  float temperature;
  std::string input_text;

  InferenceRequest(const std::string& model, size_t tokens, float temp, const std::string& input)
      : model_hash(model), input_tokens(tokens), temperature(temp), input_text(input) {}
};

// AI inference result structure
struct InferenceResult {
  bool success = false;
  size_t output_tokens = 0;
  std::string output_text;
  std::string error_message;

  InferenceResult() = default;
  InferenceResult(bool s, size_t tokens, const std::string& text)
      : success(s), output_tokens(tokens), output_text(text) {}
};

// AI backend configuration
struct AIBackendConfig {
  std::string determinism_level = "strict";  // strict | controlled | permissive
  std::optional<std::string> external_ai_endpoint = std::nullopt;
  std::optional<std::string> external_ai_auth_token = std::nullopt;
  bool evidence_collection = true;  // Security decisions are always retained.
  // No verified-consent contract exists yet. true blocks dispatch, even on Allow.
  // false explicitly selects policy-only operation, not verified user consent.
  bool require_user_consent = true;
};

class ControlledRandomness;

// Evidence collection structure for AI operations
struct AIOperationEvidence {
  std::string operation_id;
  std::string operation_type;
  std::string determinism_level;
  bool user_consent = false;  // Never inferred from policy approval.
  bool external_ai_used = false;
  std::map<std::string, std::string> metadata;
  std::vector<std::string> policy_decisions;
  std::optional<t81::axion::Verdict> authorization_verdict;
  bool policy_authorized = false;
  bool inference_dispatched = false;
  std::string failure_reason;

  std::string to_json() const;
};

// Main controlled AI backend class
class ControlledAIBackend {
public:
  ControlledAIBackend(std::unique_ptr<t81::axion::PolicyEngine> policy_engine,
                      std::optional<std::string> external_ai_endpoint = std::nullopt,
                      std::optional<std::string> external_ai_auth_token = std::nullopt,
                      bool require_user_consent = true);
  ~ControlledAIBackend();

  // Configuration
  void set_determinism_level(const std::string& level);
  void enable_external_ai(const std::string& endpoint, const std::string& auth_token);

  // AI operations
  InferenceResult inference(const InferenceRequest& request);

  // Evidence collection
  std::vector<AIOperationEvidence> get_evidence_log() const;
  std::string get_evidence_json() const;
  void clear_evidence_log();

private:
  InferenceResult inference_with_external_ai(const InferenceRequest& request,
                                             AIOperationEvidence& evidence);
  InferenceResult inference_with_controlled_randomness(const InferenceRequest& request,
                                                       AIOperationEvidence& evidence);

  bool authorize(const InferenceRequest& request, AIOperationEvidence& evidence);
  std::string generate_operation_id() const;

  template <typename T>
  T with_controlled_randomness(std::function<T(ControlledRandomness&)> operation);

  std::unique_ptr<ControlledRandomness> randomness_;
  std::unique_ptr<t81::axion::PolicyEngine> policy_engine_;
  std::vector<AIOperationEvidence> evidence_log_;
  std::string current_operation_id_;

  std::optional<std::string> external_ai_endpoint_;
  std::optional<std::string> external_ai_auth_token_;
  bool require_user_consent_ = true;
};

// Factory function
std::unique_ptr<ControlledAIBackend> create_controlled_ai_backend(
    std::unique_ptr<t81::axion::PolicyEngine> policy_engine, const AIBackendConfig& config);

}  // namespace t81::ai_backend
