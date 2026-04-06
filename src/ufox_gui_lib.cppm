module;
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vector>

export module ufox_gui_lib;

import ufox_lib;
import ufox_discadelta_lib;
import ufox_discadelta_core;
import ufox_geometry_lib;

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

  struct RenderResource {
    std::optional<vk::raii::PipelineCache>          pipelineCache{};
    std::optional<vk::raii::Pipeline>               pipeline{};
    std::optional<vk::raii::DescriptorSetLayout>    descriptorSetLayout{};
    std::optional<vk::raii::PipelineLayout>         pipelineLayout{};
    std::optional<vk::raii::DescriptorPool>         descriptorPool{};

    std::vector<vk::raii::DescriptorSet>            descriptorSets{};
  };

  struct Style {
    std::string                                     backgroundImage{"default"};
    glm::vec4                                       imageColor = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4                                       backgroundColor = {0.0f, 0.0f, 0.0f, 0.0f};

    // glm::vec4                                       borderTopColor = {0.0f, 0.0f, 0.0f, 1.0f};
    // glm::vec4                                       borderRightColor = {0.0f, 0.0f, 0.0f, 1.0f};
    // glm::vec4                                       borderBottomColor = {0.0f, 0.0f, 0.0f, 1.0f};
    // glm::vec4                                       borderLeftColor = {0.0f, 0.0f, 0.0f, 1.0f};
    // glm::vec4                                       borderThickness = {1.0f, 1.0f, 1.0f, 1.0f}; // Top, right, bottom, left
    // glm::vec4                                       cornerRadius = {10.0f, 10.0f, 10.0f, 10.0f}; // Top-left, top-right, bottom-left, bottom-right
  };

  struct UniformData {
    uint32_t                                        imageIndex = 0;
    alignas(16)glm::mat4                            model{};
    glm::vec4                                       imageColor = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4                                       backgroundColor = {0.0f, 0.0f, 0.0f, 0.0f};
  };
}
