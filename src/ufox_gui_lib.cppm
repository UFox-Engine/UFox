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
    std::vector<gpu::vulkan::Buffer>                rectBuffers{};
    std::vector<void*>                              rectBuffersMapped{};
    std::vector<vk::raii::DescriptorSet>            descriptorSets{};
  };
}
