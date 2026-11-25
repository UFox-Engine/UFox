module;
#include <vulkan/vulkan_raii.hpp>
#include <vector>
#define GLM_ENABLE_EXPERIMENTAL // Mandatory to unlock std::hash<glm::vec4> support
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>     // Directly exposes the hashes to the std namespace
#include <functional>

export module ufox_gui_lib;

import ufox_lib;
import ufox_geometry_lib;
import ufox_gpu_lib;
import ufox_font_lib;

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


  struct Style : discadelta::DiscadeltaRectLayoutStyleBase{
    std::string                    backgroundImage{"default"};
    std::string                    fontFamily{"NotoSans"};

    DisplayOverflowMode            displayOverflowMode = DisplayOverflowMode::eVisible;
    ImageScaleMode                 imageScaleMode = ImageScaleMode::eScaleToFit;
    ImageRepeatMode                imageRepeatMode = ImageRepeatMode::eNone;
    ImageAlignment                 imageAlignment = ImageAlignment::eCenter;

    float                          fontSize= 12.0f;

    glm::vec4                      imageColor = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4                      fontColor = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4                      backgroundColor = {0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4                      borderBottomColor = {0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4                      borderTopColor = {0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4                      borderLeftColor = {0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4                      borderRightColor = {0.0f, 0.0f, 0.0f, 1.0f};

    font::FontType                fontType{font::FontType::eRegular};
    font::GlyphUnicodeBlock        languageCodeBlock{font::GlyphUnicodeBlock::eLGC};

    [[nodiscard]] glm::vec4 getCornerRadiusSet() const noexcept {
      return {borderBottomRightRadius, borderTopRightRadius, borderBottomLeftRadius, borderTopLeftRadius};
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

  struct alignas(16) ElementData {
    glm::mat4       model{};                 // 64 bytes - Offset 0
    glm::vec4       imageOffsetAndTiling;    // 16 bytes - Offset 64
    SDRectInputData sdInput;                 // 48 bytes - Offset 80
    glm::vec2       scrollingOffset{0.0f};   //  8 bytes - Offset 128

    uint32_t        styleIndex{0};           //  4 bytes - Offset 136
    uint32_t        imageID{0};              //  4 bytes - Offset 140
    uint32_t        tailPadding[2];          //  8 bytes - Offset 148 (keeps 16-byte alignment)
    // Total: 160 bytes
  };

  // Size: 144 Bytes -> Divisible by 16!
  struct alignas(16) StyleStaticData {
    glm::vec4 radius;                   // 16 bytes - Offset 0
    glm::vec4 iRadius;                  // 16 bytes - Offset 16
    glm::vec4 imageColor;               // 16 bytes - Offset 32
    glm::vec4 backgroundColor;          // 16 bytes - Offset 48
    glm::vec4 borderBottomColor;        // 16 bytes - Offset 64
    glm::vec4 borderTopColor;           // 16 bytes - Offset 80
    glm::vec4 borderLeftColor;          // 16 bytes - Offset 96
    glm::vec4 borderRightColor;         // 16 bytes - Offset 112

    uint32_t  imageRepeatMode;          //  4 bytes - Offset 128
    uint32_t  isFont;                   //  4 bytes - Offset 132  (0 = normal image, 1 = MSDF glyph)
    uint32_t  tailPadding[2];           //  8 bytes - Offset 136  (keeps total at 144)

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
      s.imageRepeatMode      = 0u;
      s.isFont               = 0u;
      return s;
    }
  };

  constexpr StyleStaticData DefaultStyleGPU() noexcept{
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
  static_assert(sizeof(ElementData) == 160,       "ElementData byte stride corrupted!");
  static_assert(sizeof(StyleStaticData) == 144,   "StyleStaticData byte stride corrupted!");
}

namespace std {

    template <>
    struct hash<ufox::gui::StyleStaticData> {
        size_t operator()(const ufox::gui::StyleStaticData& s) const noexcept {
            size_t h = std::hash<uint32_t>{}(s.imageRepeatMode);
            h ^= std::hash<uint32_t>{}(s.isFont)                + 0x9e3779b9 + (h << 6) + (h >> 2);
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
