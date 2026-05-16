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
    DisplayOverflowMode            displayOverflowMode = DisplayOverflowMode::eHidden;
    ImageScaleMode                 imageScaleMode = ImageScaleMode::eStretchToFill;
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

  // Size: 48 Bytes (Perfect 16-byte internal block alignment)
  struct SDRectInputData {
      glm::vec4 offsetAndTiling;          // 16 bytes - Offset 0
      glm::vec2 size;                     //  8 bytes - Offset 16
      glm::vec2 pOffset;                  //  8 bytes - Offset 24
      glm::vec2 iSize;                    //  8 bytes - Offset 32
      glm::vec2 iPOffset;                 //  8 bytes - Offset 40
  };

  // Size: 128 Bytes (Perfect multiple of 16, Zero Hidden Gaps, Zero PCIe Overhead!)
  struct alignas(16) ElementData {
      glm::mat4       model{};            // 64 bytes - Offset 0
      SDRectInputData sdInput;            // 48 bytes - Offset 64
      glm::vec2       scrollingOffset{0.0f};// 8 bytes- Offset 112
      uint32_t        styleIndex{0};      //  4 bytes - Offset 120
      uint32_t        padding{0};         //  4 bytes - Offset 124 (Pads struct to 128 total)
  };

  // Size: 160 Bytes (Perfect multiple of 16)
  struct alignas(16) StyleStaticData {
      glm::vec4 radius;                   // 16 bytes - Offset 0
      glm::vec4 iRadius;                  // 16 bytes - Offset 16
      glm::vec4 imageOffsetAndTiling;     // 16 bytes - Offset 32
      glm::vec4 imageColor;               // 16 bytes - Offset 48
      glm::vec4 backgroundColor;          // 16 bytes - Offset 64
      glm::vec4 borderBottomColor;        // 16 bytes - Offset 80
      glm::vec4 borderTopColor;           // 16 bytes - Offset 96
      glm::vec4 borderLeftColor;          // 16 bytes - Offset 112
      glm::vec4 borderRightColor;         // 16 bytes - Offset 128

      uint32_t  imageID;                  //  4 bytes - Offset 144
      uint32_t  imageRepeatMode;          //  4 bytes - Offset 148
      uint32_t  tailPadding[2];           //  8 bytes - Offset 152 (Guarantees Slang 160-byte stride matching!)
  };

  // --- MULTI-INSTANCE STRIDE VALIDATION ASSERTIONS ---
  static_assert(sizeof(ElementData) == 128,       "ElementData byte stride corrupted!");
  static_assert(sizeof(StyleStaticData) == 160,   "StyleStaticData byte stride corrupted!");

  struct ShapeUniformData {
    uint32_t                       imageIndex = 0;
    uint32_t                       imageRepeatMode = 0;
    alignas(16) glm::mat4                      model{};
    glm::vec4                      imageOffsetAndTiling = {0.0f, 0.0f, 1.0f, 1.0f};
    glm::vec4                      imageColor = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4                      backgroundColor = {0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4                      borderBottomColor = {0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4                      borderTopColor = {0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4                      borderLeftColor = {0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4                      borderRightColor = {0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4                      cornerRadius{0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4                      borderThickness{0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4                      margin{0.0f, 0.0f, 0.0f, 0.0f};
  };
}

namespace std {

    template <>
    struct hash<ufox::gui::StyleStaticData> {
        size_t operator()(const ufox::gui::StyleStaticData& s) const noexcept {
            size_t h = std::hash<uint32_t>{}(s.imageID);
            h ^= std::hash<uint32_t>{}(s.imageRepeatMode) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<glm::vec4>{}(s.imageOffsetAndTiling) + 0x9e3779b9 + (h << 6) + (h >> 2);
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
