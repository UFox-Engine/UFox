module;
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vector>

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
