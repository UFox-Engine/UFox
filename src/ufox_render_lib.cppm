module;

#include <optional>
#include <vulkan/vulkan_raii.hpp>

export module ufox_render_lib;

import ufox_engine_lib;

export namespace ufox::render {
struct Texture2D{
  std::optional<vk::raii::Image>            data        {};
  std::optional<vk::raii::DeviceMemory>     memory      {};
  std::optional<vk::raii::ImageView>        view        {};
  vk::Format                                format      { vk::Format::eUndefined};
  vk::ImageTiling                           tiling      { vk::ImageTiling::eOptimal};
  vk::Extent2D                              extent      { 1, 1 };
  vk::ImageSubresourceRange                 subresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
  std::optional<vk::raii::Sampler>          sampler{};

  constexpr static auto viewType{ vk::ImageViewType::e2D };
};
struct TextureUser{};
struct TextureContainerSlot{};

}