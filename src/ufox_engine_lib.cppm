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
import ufox_gpu_lib;
import ufox_key;
import ufox_core;
import ufox_windowing;


export namespace ufox::action {
enum class ActionEventType : uint8_t {
  eKey,
  eAnalog,
  eButton,
  ePointer,
  eScroll,
  eText,
  eNone
};

struct PointerActionContext {
  glm::vec2 pos{0.0f};
  glm::vec2 relPos{0.0f};
  glm::vec2 delta{0.0f};
};

struct ActionContext {
  float pointMotionX{0.0f};
  float pointMotionY{0.0f};

  glm::vec2 rawGamepadAnalogLeft{0.0f};
  glm::vec2 gamepadAnalogLeft{0.0f};

  glm::vec2 rawGamepadAnalogRight{0.0f};
  glm::vec2 gamepadAnalogRight{0.0f};


  float keyboardKey_A{0.0f};
};

}


export namespace ufox::engine {


  enum class EventType {
    eResize,
    eSystemInit,
    ePostSystemInit,
    eStart,
    eGainsFocus,
    ePreDraw,
    eUpdateBuffer,
    eRenderPass,
    ePointer,
    eProcessInput
  };

  enum class PointerButton : uint8_t { eNone, eLeft, eRight, eMiddle, eX1, eX2 };
  enum class PointerPhase  : uint8_t { eMove, eDown, eUp, eWheel, eLeave };

  struct PointerEvent {
    PointerPhase  phase{PointerPhase::eMove};
    PointerButton button{PointerButton::eNone};
    glm::vec2     pos{0.0f};      // window space, y-down, same space as layout x/y
    glm::vec2     delta{0.0f};    // motion delta, or wheel amount
    uint8_t       clicks{0};      // SDL click count: 1 single, 2 double
  };

  using PointerEventHandler      = bool(*)(const PointerEvent& e, void* user);   // true = consumed
  using ProcessInputEventHandler = void(*)(void* user);

  using ResizeEventHandler = void(*)(const float& w, const float& h, void* user);
  using SystemInitEventHandler = void(*)(void* user);
  using PostSystemInitEventHandler = void(*)(const float& w, const float& h, void* user);
  using StartEventHandler = void(*)(void* user);
  using PreDrawEventHandler = void(*)(void* user);
  using UpdateBufferEventHandler = void(*)(const uint32_t& currentImage, void* user);
  using RenderPassEventHandler = void(*)(const vk::raii::CommandBuffer& cmb, const uint32_t& imageIndex, const windowing::WindowResource& winResource, const vk::RenderingAttachmentInfo& colorAttachment, const vk::RenderingAttachmentInfo& stencilAttachment, void* user );

  template<EventType Type>
  struct EventTraits;

  template<>
  struct EventTraits<EventType::eResize> {
    using Handler = ResizeEventHandler;
  };

  template<>
  struct EventTraits<EventType::eSystemInit> {
    using Handler = SystemInitEventHandler;
  };

  template<>
  struct EventTraits<EventType::ePostSystemInit> {
    using Handler = PostSystemInitEventHandler;
  };

  template<>
  struct EventTraits<EventType::eStart> {
    using Handler = StartEventHandler;
  };


  template<>
  struct EventTraits<EventType::ePreDraw> {
    using Handler = PreDrawEventHandler;
  };

  template<>
  struct EventTraits<EventType::eUpdateBuffer> {
    using Handler = UpdateBufferEventHandler;
  };

  template<>
  struct EventTraits<EventType::eRenderPass> {
    using Handler = RenderPassEventHandler;
  };

  template<>
  struct EventTraits<EventType::ePointer> {
    using Handler = PointerEventHandler;
  };

  template<>
    struct EventTraits<EventType::eProcessInput> {
    using Handler = ProcessInputEventHandler;
  };

  constexpr auto META_TAG_RESOURCE_CONTEXT = "resource-context";

  inline auto SCREEN_VIEW_METRIX = glm::mat4(1.0f);
  constexpr vk::DeviceSize MAT4_BUFFER_SIZE = sizeof(glm::mat4);
  constexpr auto SCREEN_VIEW_PROJECTION_BUFFER_TYPE = vk::DescriptorType::eUniformBuffer;

  template<typename T>
  concept Arithmetic = std::is_arithmetic_v<T>;



  enum class SourceType : uint8_t {
    eBuiltIn   = 0,
    ePortIn    = 1,
  };

  enum class ViewProjectionType : uint8_t {
    ePerspective = 0,
    eOrthographic = 1,
  };

  struct DirectoryContext final {
    std::filesystem::path   directoryPath{};
    std::string             lastWriteTimeIso{};   // ISO 8601 string
    uintmax_t               totalSizeBytes{0};
    size_t                  stateHash{0};

    [[nodiscard]] constexpr bool isValid() const noexcept {return !directoryPath.empty() && std::filesystem::exists(directoryPath) && std::filesystem::is_directory(directoryPath);}
    [[nodiscard]] constexpr explicit operator bool() const noexcept {return isValid();}

    void updateHash() noexcept {
      // Simple, fast, collision-resistant hash for our fields
      size_t h1 = std::hash<std::string>{}(directoryPath.string());
      size_t h2 = std::hash<std::string>{}(lastWriteTimeIso);
      size_t h3 = std::hash<uintmax_t>{}(totalSizeBytes);

      // Combine with good mixing (similar to your utilities style)
      stateHash = h1 ^ h2 << 1 ^ h3 << 2 ^ h1 >> 3 ^ h2 >> 5 ^ h3 >> 7;
    }

    constexpr void printState() const noexcept {
      if (isValid()) {
        const std::string pathStr = directoryPath.string();
        debug::log(debug::LogLevel::eInfo, "Directory state ({}): valid | lastWriteTime : {} | totalSizeBytes : {}", pathStr, lastWriteTimeIso, totalSizeBytes);
      } else {
        debug::log(debug::LogLevel::eInfo, "Directory state ({}): invalid", directoryPath.string());
      }
    }
  };

  struct ResourceBase {
    virtual ~ResourceBase() = default;

    std::string                       name;
    const KeyID                  iD;

    explicit ResourceBase(const std::string_view name_view,KeyID cid_)
        : name(name_view), iD(std::move(cid_)){}

    [[nodiscard]] virtual bool hasGpuResources() const noexcept=0;

    virtual void releaseGpuResources() noexcept = 0;

    template<typename T>
    T* getContent() {
      return dynamic_cast<T*>(this);
    }
  };

  using ResourceAttachment = std::pair<std::string, std::filesystem::path>;

  struct ResourceContext {
    std::string                                 name{"empty"};
    SourceType                                  sourceType{SourceType::eBuiltIn};
    std::string                                 category{"none"};
    std::unique_ptr<ResourceBase>               dataPtr{nullptr};
    std::filesystem::path                       sourcePath{};
    std::chrono::file_clock::time_point         lastImportTime{};
    std::string                                 lastWriteTime{};
    std::vector<ResourceAttachment>             attachments{};

    void clear() noexcept {
      dataPtr.reset();
      sourcePath.clear();
      lastImportTime = {};
      lastWriteTime.clear();
      attachments.clear();
    }
  };

  struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
  };

  struct Transform {
    glm::vec3                         position{0.0f};
    glm::quat                         rotation{glm::vec3{0.0f}};
    glm::vec3                         scale{1.0f};
  };

  struct Camera {
    Transform                         transform{};
    ViewProjectionType                projectionType{ViewProjectionType::ePerspective};
    float                             nearPlane{0.1f};
    float                             farPlane{5000.0f};
    float                             fov{90.0f};
    float                             orthoSize{10.0f};
    float                             aspectRatio{1.0f};
  };

  struct ResourceContextCreateInfo {
    std::filesystem::path             sourcePath{};
    std::string                       name{};
    std::string                       category{};
    std::string                       lastWriteTimeFromMeta{};
    SourceType                        sourceType{SourceType::eBuiltIn};
    KeyID                        id{};
    std::vector<ResourceAttachment>   attachments{};
    bool                              overwrite{false};

    constexpr ResourceContextCreateInfo& setID(const KeyID& id) noexcept {
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
    constexpr ResourceContextCreateInfo& setAttachments(const std::vector<ResourceAttachment>& attachments) noexcept {
      this->attachments = attachments; return *this;
    }
    constexpr ResourceContextCreateInfo& setOverwrite(const bool& overwrite) noexcept {
      this->overwrite = overwrite; return *this;
    }
  };

  using ResourceContextCreateEventHandler = void(*)(const ResourceContextCreateInfo& info,void* userData);

  using BuiltInResources = std::unordered_map<std::string, const KeyID*>;
}


namespace std {
template <>
struct hash<ufox::KeyID> {          // or ufox::engine::KeyID if still nested
  [[nodiscard]] size_t operator()(const ufox::KeyID& id) const noexcept {
    return std::hash<std::string_view>{}(id.view());
  }
};
}




