#pragma once

#include <string_view>

namespace motorforge {

inline constexpr std::string_view kContractVersion{"1"};
inline constexpr std::string_view kFallbackVersion{"0.1.0"};

[[nodiscard]] std::string_view version() noexcept;
[[nodiscard]] std::string_view contract_version() noexcept;
[[nodiscard]] std::string_view capabilities_json() noexcept;

} // namespace motorforge
