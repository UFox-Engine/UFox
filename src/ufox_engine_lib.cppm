module;

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
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
  using ResourceInitEventHandler = void(*)(const float& w, const float& h, void* user);
  using StartEventHandler = void(*)(void* user);
  using UpdateBufferEventHandler = void(*)(const uint32_t& currentImage, void* user);
  using DrawCanvasEventHandler = void(*)(const vk::raii::CommandBuffer& cmb, const uint32_t& imageIndex, const windowing::WindowResource& winResource, void* user );

  constexpr ContentIndex   INVALID_INDEX   = 0xFFFFFFFFu;
  constexpr ContentVersion INVALID_VERSION = 0u;

  constexpr auto SCREEN_VIEW_METRIX = glm::mat4(1.0f);
  constexpr vk::DeviceSize MAT4_BUFFER_SIZE = sizeof(glm::mat4);
  constexpr auto SCREEN_VIEW_PROJECTION_BUFFER_TYPE = vk::DescriptorType::eUniformBuffer;

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

  enum class ViewProjectionType : uint8_t {
    ePerspective = 0,
    eOrthographic = 1,
  };

  struct UFoxContentAsset {
    virtual ~UFoxContentAsset() = default;

    std::string               name;
    const ContentID           cid;
    const ContentSourceType   sourceType;
    std::string               category;

    uint64_t                  lastModifiedMs  {0};
    size_t                    memoryFootprint {0};

    explicit UFoxContentAsset(const std::string_view name_view,
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
    glm::vec3             position{0.0f};
    glm::quat             rotation{glm::vec3{0.0f}};
    glm::vec3             scale{1.0f};
  };

  struct Camera {
    Transform             transform{};
    ViewProjectionType  projectionType{ViewProjectionType::ePerspective};
    float                 nearPlane{0.1f};
    float                 farPlane{5000.0f};
    float                 fov{90.0f};
    float                 orthoSize{10.0f};
    float                 aspectRatio{1.0f};
  };
}