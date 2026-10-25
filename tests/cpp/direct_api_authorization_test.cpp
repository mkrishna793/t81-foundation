#include "t81/direct_api/t81_direct_api.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace t81::direct_api;
namespace {
void require(bool value, const std::string& message) {
  if (!value) throw std::runtime_error(message);
}
struct TempDir {
  fs::path path;
  TempDir() {
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    path = fs::temp_directory_path() / ("t81-direct-api-security-" + std::to_string(tick));
    require(fs::create_directory(path), "create isolated test directory");
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};
void rejected(const AssessResult& result, const std::string& prefix) {
  require(!result.success && result.result_ref.empty() && result.provenance_ref.empty(),
          "Direct API must not turn failure into success/provenance");
  require(result.error_message.starts_with(prefix),
          "Direct API must propagate backend failure: " + result.error_message);
}
}  // namespace
int main() {
  try {
    TempDir temp;
    auto api = create_direct_api();
    rejected(api->assess_fixed("input"), "ERR_AI_NOT_INITIALIZED");
    require(!api->initialize("", ""), "empty paths are not valid initial cache hits");
    rejected(api->assess_fixed("input"), "ERR_AI_NOT_INITIALIZED");
    t81::weights::NativeTensor tensor;
    tensor.shape = {1};
    tensor.trits = 1;
    tensor.data = {0};
    t81::weights::NativeModel model;
    model["test.weight"] = tensor;
    const auto model_path = temp.path / "model.t81w";
    const auto policy_path = temp.path / "allow.apl";
    const auto deny_path = temp.path / "deny.apl";
    t81::weights::save_t81w(model, model_path);
    std::ofstream(policy_path) << "(policy (tier 0))";
    std::ofstream(deny_path)
        << "(policy (tier 0) (require-axion-event (reason \"required evidence\")))";

    require(!api->initialize(model_path.string(), ""), "empty policy is not a valid cache hit");
    rejected(api->assess_fixed("input"), "ERR_AI_NOT_INITIALIZED");
    require(api->initialize(model_path.string(), deny_path.string()), "initialize denying policy");
    auto denied = api->assess_fixed("input");
    rejected(denied, "ERR_AI_AUTHORIZATION_DENIED: Deny:");
    require(denied.error_message.find("Missing Axion event") != std::string::npos,
            "Direct API must use final authorization");
    require(denied.to_json().find("\\\"required evidence\\\"") != std::string::npos,
            "failure reason must be escaped in result JSON");

    DirectAPIConfig config;
    config.enable_evidence_collection = false;  // Cannot bypass the backend's controls.
    require(api->initialize(model_path.string(), policy_path.string(), config), "initialize Allow");
    rejected(api->assess_fixed("input"), "ERR_AI_CONSENT_UNSUPPORTED");
    require(api->initialize(model_path.string(), policy_path.string(), config),
            "reinitialize cached policy");
    rejected(api->assess_fixed("input"), "ERR_AI_CONSENT_UNSUPPORTED");
    for (const auto& result : api->batch_assess_fixed({"one", "two"})) {
      rejected(result, "ERR_AI_CONSENT_UNSUPPORTED");
    }
    require(!api->initialize(model_path.string(), (temp.path / "missing.apl").string()),
            "missing policy initialization must fail");
    rejected(api->assess_fixed("input"), "ERR_AI_NOT_INITIALIZED");
    require(api->initialize(model_path.string(), policy_path.string()),
            "restore Allow initialization");
    require(!api->initialize((temp.path / "missing.t81w").string(), policy_path.string()),
            "missing model initialization must fail");
    rejected(api->assess_fixed("input"), "ERR_AI_NOT_INITIALIZED");
    std::cout
        << "PASS: Direct API final authorization, consent, cached reinit, batch and invalidation\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
