//
// Created by b-boy on 27.11.2026.
//
module;
#include <cstddef>     // std::size_t
#include <functional>  // std::hash
#include <ostream>     // std::ostream (operator<<)
#include <string>      // std::string
#include <string_view> // std::string_view
#include <ufox_engine_export.h>

export module ufox_key;

namespace ufox {

export struct UFOX_ENGINE_EXPORT KeyID final {
  std::string data{};

  KeyID() = default;
  explicit KeyID(std::string_view s) : data(s) {}

  [[nodiscard]] bool empty() const noexcept { return data.empty(); }
  [[nodiscard]] bool isValid() const noexcept { return !empty(); }
  [[nodiscard]] explicit operator bool() const noexcept { return isValid(); }

  [[nodiscard]] std::string_view view() const noexcept { return data; }
  [[nodiscard]] const std::string& str() const noexcept { return data; }

  friend auto operator<=>(const KeyID&, const KeyID&) noexcept = default;
  friend bool operator==(const KeyID&, const KeyID&) noexcept = default;

  friend std::ostream& operator<<(std::ostream& os, const KeyID& id) {
    return os << (id.isValid() ? id.data : "[invalid]");
  }
};

// Prefer the function form so storage lives in the DLL
export const UFOX_ENGINE_EXPORT KeyID& InvalidContentID() noexcept;

export struct UFOX_ENGINE_EXPORT KeyIDHash {
  using is_transparent = void;

  [[nodiscard]] std::size_t operator()(const KeyID& id) const noexcept {
    return std::hash<std::string_view>{}(id.view());
  }

  [[nodiscard]] std::size_t operator()(std::string_view id) const noexcept {
    return std::hash<std::string_view>{}(id);
  }

  [[nodiscard]] std::size_t operator()(const char* id) const noexcept {
    return id ? std::hash<std::string_view>{}(id) : 0u;
  }
};

export struct UFOX_ENGINE_EXPORT KeyIDEq {
  using is_transparent = void;

  [[nodiscard]] bool operator()(const KeyID& a, const KeyID& b) const noexcept {
    return a.data == b.data;
  }

  [[nodiscard]] bool operator()(const KeyID& a, std::string_view b) const noexcept {
    return a.data == b;
  }

  [[nodiscard]] bool operator()(std::string_view a, const KeyID& b) const noexcept {
    return a == b.data;
  }

  [[nodiscard]] bool operator()(const KeyID& a, const char* b) const noexcept {
    return b && a.data == b;
  }

  [[nodiscard]] bool operator()(const char* a, const KeyID& b) const noexcept {
    return a && std::string_view{a} == b.data;
  }
};

}

