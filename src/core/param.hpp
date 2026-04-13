#pragma once

namespace core {

// Bundles a JSON key name with its current value.
// The initial `value` serves as the compiled-in default — no separate k-constant needed.
//
// Config sub-structs using Param fields should also declare:
//   static constexpr const char* kSection = "<json_section_name>";
// so that app_config.cpp can locate the correct JSON section without a separate constant.
template<typename T>
struct Param {
  const char* key;
  T           value;
};

}  // namespace core
