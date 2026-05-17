module;
#include <vulkan/vulkan_raii.hpp>
#include <vector>
#define GLM_ENABLE_EXPERIMENTAL // Mandatory to unlock std::hash<glm::vec4> support
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>     // Directly exposes the hashes to the std namespace
#include <functional>

export module ufox_gui_lib;

import ufox_lib;
import ufox_discadelta_lib;
import ufox_discadelta_core;
import ufox_geometry_lib;
import ufox_gpu_lib;

using namespace ufox::geometry;

export namespace ufox::gui {
constexpr uint32_t MAX_GUI_TEXTURES = 256;
constexpr uint32_t MAX_GUI_ELEMENTS = 65536u;
constexpr uint32_t MAX_TOTAL_CHARACTERS = 1048576u;

Vertex RectVertices[]{
  {{0.0f, 0.0f, 0.0f}, {1.0f,1.0f,1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
  {{1.0f, 0.0f, 0.0f}, {1.0f,1.0f,1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
  {{1.0f, 1.0f, 0.0f}, {1.0f,1.0f,1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
  {{0.0f, 1.0f, 0.0f}, {1.0f,1.0f,1.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
};

uint16_t RectIndices[]{
  0, 1, 2, 2, 3, 0
};

constexpr auto RECT_MESH_NAME = "gui_rect_mesh";

struct StorageBufferIndices {
  uint32_t bgImageIndex{0};
  uint32_t parentElementIndex{0};
};

using SamplerConfig = std::tuple<
    vk::SamplerAddressMode,
    vk::SamplerAddressMode,
    vk::SamplerAddressMode,
    vk::BorderColor>;

constexpr std::array REPEAT_SAMPLER_CONFIGS{
  SamplerConfig{
    vk::SamplerAddressMode::eClampToBorder,
    vk::SamplerAddressMode::eClampToBorder,
    vk::SamplerAddressMode::eClampToBorder,
    vk::BorderColor::eFloatTransparentBlack
  },
  SamplerConfig{
    vk::SamplerAddressMode::eRepeat,
    vk::SamplerAddressMode::eClampToBorder,
    vk::SamplerAddressMode::eClampToBorder,
    vk::BorderColor::eFloatTransparentBlack
  },
  SamplerConfig{
    vk::SamplerAddressMode::eClampToBorder,
    vk::SamplerAddressMode::eRepeat,
    vk::SamplerAddressMode::eClampToBorder,
    vk::BorderColor::eFloatTransparentBlack
  },
  SamplerConfig{
    vk::SamplerAddressMode::eRepeat,
    vk::SamplerAddressMode::eRepeat,
    vk::SamplerAddressMode::eClampToBorder,
    vk::BorderColor::eFloatOpaqueBlack
  }
};

enum class DisplayOverflowMode : uint8_t {
  eVisible = 0,
  eHidden = 1
};

enum class ImageScaleMode : uint8_t {
  eStretchToFill,
  eScaleToFit,
  eScaleAndCrop
};

enum class ImageRepeatMode : uint8_t {
  eNone,
  eRepeatX,
  eRepeatY,
  eRepeatXY
};

enum class ImageAlignment : uint8_t {
  eTopLeft,
  eTopCenter,
  eTopRight,
  eCenterLeft,
  eCenter,
  eCenterRight,
  eBottomLeft,
  eBottomCenter,
  eBottomRight
};


struct Style : DiscadeltaRectLayoutStyleBase{
  std::string                    backgroundImage{"default"};
  DisplayOverflowMode            displayOverflowMode = DisplayOverflowMode::eVisible;
  ImageScaleMode                 imageScaleMode = ImageScaleMode::eScaleToFit;
  ImageRepeatMode                imageRepeatMode = ImageRepeatMode::eNone;
  ImageAlignment                 imageAlignment = ImageAlignment::eCenter;

  float                          borderTopWidth = 0.0f;
  float                          borderBottomWidth = 0.0f;
  float                          borderRightWidth = 0.0f;
  float                          borderLeftWidth = 0.0f;
  float                          borderTopLeftRadius = 0.0f;
  float                          borderTopRightRadius = 0.0f;
  float                          borderBottomLeftRadius = 0.0f;
  float                          borderBottomRightRadius = 0.0f;
  float                          marginTop = 0.0f;
  float                          marginBottom = 0.0f;
  float                          marginLeft = 0.0f;
  float                          marginRight = 0.0f;

  glm::vec4                      imageColor = {1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4                      backgroundColor = {0.0f, 0.0f, 0.0f, 0.0f};
  glm::vec4                      borderBottomColor = {0.0f, 0.0f, 0.0f, 1.0f};
  glm::vec4                      borderTopColor = {0.0f, 0.0f, 0.0f, 1.0f};
  glm::vec4                      borderLeftColor = {0.0f, 0.0f, 0.0f, 1.0f};
  glm::vec4                      borderRightColor = {0.0f, 0.0f, 0.0f, 1.0f};

  [[nodiscard]] glm::vec4 getBorderThicknessSet() const noexcept {
    return {borderTopWidth, borderRightWidth, borderBottomWidth, borderLeftWidth};
  }

  [[nodiscard]] glm::vec4 getCornerRadiusSet() const noexcept {
    return {borderBottomRightRadius, borderTopRightRadius, borderBottomLeftRadius, borderTopLeftRadius};
  }

  [[nodiscard]] glm::vec4 getMarginSet() const noexcept {
    return {marginTop, marginRight, marginBottom, marginLeft};
  }

  [[nodiscard]] constexpr glm::vec2 getMarginSpace() const noexcept {
    const float x = marginLeft + marginRight;
    const float y = marginTop + marginBottom;
    return {x, y};
  }

  [[nodiscard]] constexpr glm::vec2 getBorderThicknessSpace() const noexcept {
    const float x = borderLeftWidth + borderRightWidth;
    const float y = borderTopWidth + borderBottomWidth;
    return {x, y};
  }

  [[nodiscard]] glm::vec2 getImageSpace() const noexcept {
    const float x = marginLeft + marginRight + borderLeftWidth + borderRightWidth;
    const float y = marginTop + marginBottom + borderTopWidth + borderBottomWidth;
    return {x, y};
  }

  [[nodiscard]]  glm::vec2 getImageBRSpaceOffset() const noexcept {
    const float x = marginRight + borderRightWidth;
    const float y = marginBottom + borderBottomWidth;
    return {x, y};
  }

  [[nodiscard]] glm::vec2 getImageTLSpaceOffset() const noexcept {
    const float x = marginLeft + borderLeftWidth;
    const float y = marginTop + borderTopWidth;
    return {x, y};
  }
};


  // Size: 48 Bytes (Naturally aligned, no internal padding gaps)
  struct SDRectInputData {
    glm::vec4 offsetAndTiling;          // 16 bytes - Offset 0
    glm::vec2 size;                     //  8 bytes - Offset 16
    glm::vec2 pOffset;                  //  8 bytes - Offset 24
    glm::vec2 iSize;                    //  8 bytes - Offset 32
    glm::vec2 iPOffset;                 //  8 bytes - Offset 40
  };

  // Size: 144 Bytes -> Divisible by 16! (Zero gaps, perfect GPU striding)
  struct alignas(16) ElementData {
    glm::mat4       model{};            // 64 bytes - Offset 0
    glm::vec4       imageOffsetAndTiling;// 16 bytes - Offset 64
    SDRectInputData sdInput;            // 48 bytes - Offset 80
    glm::vec2       scrollingOffset{0.0f};// 8 bytes- Offset 128

    // --- SUCCESS: Dynamic asset context keys grouped right at the end ---
    uint32_t        styleIndex{0};      //  4 bytes - Offset 136
    uint32_t        imageID{0};         //  4 bytes - Offset 140
  };

  // Size: 144 Bytes -> Divisible by 16! (Explicit tailPadding handles Slang alignment matching)
  struct alignas(16) StyleStaticData {
    glm::vec4 radius;                   // 16 bytes - Offset 0
    glm::vec4 iRadius;                  // 16 bytes - Offset 16
    glm::vec4 imageColor;               // 16 bytes - Offset 32
    glm::vec4 backgroundColor;          // 16 bytes - Offset 48
    glm::vec4 borderBottomColor;        // 16 bytes - Offset 64
    glm::vec4 borderTopColor;           // 16 bytes - Offset 80
    glm::vec4 borderLeftColor;          // 16 bytes - Offset 96
    glm::vec4 borderRightColor;         // 16 bytes - Offset 112

    // --- REMOVED imageID ---
    uint32_t  imageRepeatMode;          //  4 bytes - Offset 128
    uint32_t  tailPadding[3];        // 12 bytes - Offset 132 (Fills out exactly to 144 bytes)

    static constexpr StyleStaticData Default() noexcept
    {
      StyleStaticData s{};
      s.radius               = glm::vec4(0.0f);
      s.iRadius              = glm::vec4(0.0f);
      s.imageColor           = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
      s.backgroundColor      = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
      s.borderTopColor       = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      s.borderLeftColor      = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      s.borderRightColor     = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      s.borderBottomColor    = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      s.imageRepeatMode      = 0;
      return s;
    }
  };

  constexpr StyleStaticData DefaultStyleGPU() noexcept
  {
    StyleStaticData s{};

    s.radius               = glm::vec4(0.0f);
    s.iRadius              = glm::vec4(0.0f);
    s.imageColor           = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    s.backgroundColor      = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    s.borderTopColor       = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    s.borderLeftColor      = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    s.borderRightColor     = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    s.borderBottomColor    = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    s.imageRepeatMode      = 0;
    // tailPadding remains zero due to {} initialization

    return s;
  }

  // Optional: Global constexpr instance (very convenient)
  inline constexpr StyleStaticData DEFAULT_STYLE = StyleStaticData::Default();

  // --- RUNTIME STRIDE VALIDATION ASSERTIONS PASS SECURELY ---
  static_assert(sizeof(ElementData) == 144,       "ElementData byte stride corrupted!");
  static_assert(sizeof(StyleStaticData) == 144,   "StyleStaticData byte stride corrupted!");
}

namespace std {

    template <>
    struct hash<ufox::gui::StyleStaticData> {
        size_t operator()(const ufox::gui::StyleStaticData& s) const noexcept {
            size_t h = std::hash<uint32_t>{}(s.imageRepeatMode);
            h ^= std::hash<glm::vec4>{}(s.imageColor)           + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<glm::vec4>{}(s.backgroundColor)      + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<glm::vec4>{}(s.borderBottomColor)    + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<glm::vec4>{}(s.borderTopColor)       + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<glm::vec4>{}(s.borderLeftColor)      + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<glm::vec4>{}(s.borderRightColor)     + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<glm::vec4>{}(s.radius)               + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<glm::vec4>{}(s.iRadius)              + 0x9e3779b9 + (h << 6) + (h >> 2);

            return h;
        }
    };

}
