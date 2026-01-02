module;

#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <vulkan/vulkan_raii.hpp>

#ifdef USE_SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#else
#include <GLFW/glfw3.h>
#endif

export module ufox_engine_lib;

import ufox_lib;

export namespace ufox::engine {
  using ContentIndex   = uint32_t;
  using ContentVersion = uint16_t;
  using ResizeEventHandler = void(*)(const float& w, const float& h, void* user);
  using SystemInitEventHandler = void(*)(void* user);
  using ResourceInitEventHandler = void(*)(void* user);
  using StartEventHandler = void(*)(void* user);
  using UpdateBufferEventHandler = void(*)(const uint32_t& currentImage, void* user);
  using DrawCanvasEventHandler = void(*)(const vk::raii::CommandBuffer& cmb, const uint32_t& imageIndex, const windowing::WindowResource& winResource, void* user );

  constexpr ContentIndex   INVALID_INDEX   = 0xFFFFFFFFu;
  constexpr ContentVersion INVALID_VERSION = 0u;

  template<typename T>
  concept Arithmetic = std::is_arithmetic_v<T>;

  struct ContentID final {
    ContentIndex   index   { INVALID_INDEX };
    ContentVersion version { INVALID_VERSION };

    [[nodiscard]] constexpr bool IsValid() const noexcept { return index != INVALID_INDEX; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return IsValid(); }

    [[nodiscard]] constexpr bool operator==(const ContentID& o) const noexcept = default;
  };

  inline constexpr ContentID INVALID_CONTENT_ID {};

  enum class ContentSourceType : uint8_t {
    eBuiltIn   = 0,
    ePortIn    = 1,
  };

  struct Content {
    virtual ~Content() = default;

    std::string               name;
    const ContentID           cid;
    const ContentSourceType   sourceType;
    std::string               category;

    uint64_t                  lastModifiedMs  {0};
    size_t                    memoryFootprint {0};

    explicit Content(const std::string_view name_view,
                     const ContentSourceType src,
                     const std::string_view cat,  const ContentID& cid_)
        : name(name_view), cid(cid_), sourceType(src), category(cat) {}

  };

  struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
  };

  struct Transform {
    glm::vec3 position;
    glm::vec3 scale;
    glm::vec3 rotation;
  };
}