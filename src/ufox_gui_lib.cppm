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

  Vertex RectVertices[]{
    {{0.0f, 0.0f, 0.0f}, {1.0f,1.0f,1.0f}, {0.001f, 0.001f}, {1.0f, 0.0f, 0.0f}},
    {{1.0f, 0.0f, 0.0f}, {1.0f,1.0f,1.0f}, {0.999f, 0.001f}, {0.0f, 1.0f, 0.0f}},
    {{1.0f, 1.0f, 0.0f}, {1.0f,1.0f,1.0f}, {0.999f, 0.999f}, {0.0f, 0.0f, 1.0f}},
    {{0.0f, 1.0f, 0.0f}, {1.0f,1.0f,1.0f}, {0.001f, 0.999f}, {1.0f, 1.0f, 1.0f}},
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
    eHidden,
    eVisible
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

  struct Style {
    std::string                    backgroundImage{"default"};
    DisplayOverflowMode            displayOverflowMode = DisplayOverflowMode::eHidden;
    PositionMode                   positionMode = PositionMode::eRelative;
    ImageScaleMode                 imageScaleMode = ImageScaleMode::eStretchToFill;
    ImageRepeatMode                imageRepeatMode = ImageRepeatMode::eNone;
    ImageAlignment                 imageAlignment = ImageAlignment::eCenter;
    Length                         width{};
    Length                         height{};
    Length                         minWidth{};
    Length                         minHeight{};
    Length                         maxWidth{};
    Length                         maxHeight{};
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
  };

  struct UniformData {
    int32_t                        parentIndex = 0;
    uint32_t                       displayOverflowMode = 0;
    uint32_t                       imageIndex = 0;
    uint32_t                       imageRepeatMode = 0;
    glm::mat4                      model{};
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
