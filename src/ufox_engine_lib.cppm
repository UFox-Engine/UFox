module;

#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vulkan/vulkan_raii.hpp>
#include <chrono>

#ifdef USE_SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#else
#include <GLFW/glfw3.h>
#endif

export module ufox_engine_lib;

import ufox_lib;

export namespace ufox::engine {

  using ResizeEventHandler = void(*)(const float& w, const float& h, void* user);
  using SystemInitEventHandler = void(*)(void* user);
  using ResourceInitEventHandler = void(*)(const float& w, const float& h, void* user);
  using StartEventHandler = void(*)(void* user);
  using UpdateBufferEventHandler = void(*)(const uint32_t& currentImage, void* user);
  using DrawCanvasEventHandler = void(*)(const vk::raii::CommandBuffer& cmb, const uint32_t& imageIndex, const windowing::WindowResource& winResource, void* user );

  constexpr auto META_TAG_RESOURCE_CONTEXT = "resource-context";

  constexpr auto SCREEN_VIEW_METRIX = glm::mat4(1.0f);
  constexpr vk::DeviceSize MAT4_BUFFER_SIZE = sizeof(glm::mat4);
  constexpr auto SCREEN_VIEW_PROJECTION_BUFFER_TYPE = vk::DescriptorType::eUniformBuffer;

  template<typename T>
  concept Arithmetic = std::is_arithmetic_v<T>;

  struct ResourceID final {
    std::string data{};

    constexpr ResourceID() noexcept = default;
    explicit constexpr ResourceID(std::string s) noexcept : data(std::move(s)) {}

    [[nodiscard]] constexpr bool        IsValid()     const noexcept { return !data.empty(); }
    [[nodiscard]] constexpr explicit operator bool()  const noexcept { return IsValid(); }

    friend constexpr auto operator<=>(const ResourceID&, const ResourceID&) noexcept = default;
    friend constexpr bool operator==(const ResourceID&, const ResourceID&)  noexcept = default;

    // Very useful for logging / debugging
    friend std::ostream& operator<<(std::ostream& os, const ResourceID& id) {
      return os << (id.IsValid() ? id.data : "[invalid]");
    }

    [[nodiscard]] std::string_view view() const noexcept { return data; }
  };

  inline constexpr ResourceID INVALID_CONTENT_ID {};

  enum class SourceType : uint8_t {
    eBuiltIn   = 0,
    ePortIn    = 1,
  };

  enum class ViewProjectionType : uint8_t {
    ePerspective = 0,
    eOrthographic = 1,
  };

  struct ResourceBase {
    virtual ~ResourceBase() = default;

    std::string               name;
    const ResourceID           iD;

    explicit ResourceBase(const std::string_view name_view,ResourceID cid_)
        : name(name_view), iD(std::move(cid_)){}

    [[nodiscard]] virtual bool hasBuffer() const noexcept=0;

    virtual void releaseGpuResources() noexcept =0;

    template<typename T>
    T* getContent() {
      return dynamic_cast<T*>(this);
    }
  };

  struct ResourceUserBase {
    virtual ~ResourceUserBase() = default;
    ResourceUserBase() = default;
    explicit ResourceUserBase(const ResourceID* _id) : id(_id){}

    const ResourceID* id = nullptr;

    virtual void setNewTarget(const ResourceID* id, void* target) = 0;
    virtual void clear() = 0;
  };

  struct ResourceContext {
    std::string                                 name{"empty"};
    SourceType                           sourceType;
    std::string                                 category;
    std::unique_ptr<ResourceBase>               dataPtr{nullptr};
    std::vector<ResourceUserBase*>              users{};
    std::filesystem::path                       sourcePath{};
    std::chrono::file_clock::time_point         lastImportTime{};
    std::string                                 lastWriteTime{};

    void clear() noexcept {
      dataPtr.reset();
      users.clear();
      sourcePath.clear();
      lastImportTime = {};
      lastWriteTime.clear();
    }
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
    ViewProjectionType    projectionType{ViewProjectionType::ePerspective};
    float                 nearPlane{0.1f};
    float                 farPlane{5000.0f};
    float                 fov{90.0f};
    float                 orthoSize{10.0f};
    float                 aspectRatio{1.0f};
  };

  struct ResourceContextCreateInfo {
    std::filesystem::path sourcePath{};
    std::string_view name{};
    std::string_view category{};
    std::string_view lastWriteTimeFromMeta{};
    SourceType  sourceType{SourceType::eBuiltIn};
    ResourceID       id{};

    constexpr ResourceContextCreateInfo& setID(const ResourceID& id) noexcept {
      this->id = id; return *this;
    }
    constexpr ResourceContextCreateInfo& setSourcePath(const std::filesystem::path& path) noexcept {
      this->sourcePath = path; return *this;
    }
    constexpr ResourceContextCreateInfo& setName(const std::string_view& name) noexcept {
      this->name = name; return *this;
    }
    constexpr ResourceContextCreateInfo& setCategory(const std::string_view& category) noexcept {
      this->category = category; return *this;
    }
    constexpr ResourceContextCreateInfo& setLastWriteTime(const std::string_view& time) noexcept {
      this->lastWriteTimeFromMeta = time; return *this;
    }
    constexpr ResourceContextCreateInfo& setSourceType(const SourceType& type) noexcept {
      this->sourceType = type; return *this;
    }
  };

  using ResourceContextCreateEventHandler = void(*)(const ResourceContextCreateInfo& info,void* userData);
}

namespace std {
  template <>
  struct hash<ufox::engine::ResourceID>{
    [[nodiscard]] size_t operator()(const ufox::engine::ResourceID& id) const noexcept{
      return std::hash<std::string>{}(id.data);
    }
  };
}
