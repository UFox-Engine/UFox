module;

#include <optional>
#include <vulkan/vulkan_raii.hpp>

export module ufox_render_lib;

import ufox_engine_lib;

export namespace ufox::render {

  inline std::string_view TEXTURE_RESOURCE_PATH = "res/textures";
  inline std::string TEXTURE_RESOURCE_EXTENSION[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp" };

  struct Texture2D final : engine::ResourceBase {
    std::optional<vk::raii::Image>        image{};
    std::optional<vk::raii::DeviceMemory> memory{};
    std::optional<vk::raii::ImageView>    view{};
    std::optional<vk::raii::Sampler>      sampler{};

    vk::Format                            format{vk::Format::eUndefined};
    vk::ImageTiling                       tiling      { vk::ImageTiling::eOptimal};
    vk::Extent2D                          extent{1, 1};
    vk::ImageSubresourceRange             subresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    explicit Texture2D(const std::string_view name_view, const engine::ResourceID& cid)
        : ResourceBase(name_view, cid) {}

    [[nodiscard]] bool hasGpuResources() const noexcept override {
      return view.has_value() && sampler.has_value();
    }

    void releaseGpuResources() noexcept override {
      sampler.reset();
      view.reset();
      image.reset();
      memory.reset();
    }
  };
  struct TextureUser final : engine::ResourceUserBase {
    Texture2D* texture = nullptr;
    uint32_t   textureIndex = UINT32_MAX;   // assigned by manager

    TextureUser() = default;
    TextureUser(const engine::ResourceID* _id, Texture2D* t)
        : ResourceUserBase(_id), texture(t) {}

    void setNewTarget(const engine::ResourceID* id, void* target) override {
      this->id = id;
      this->texture = static_cast<Texture2D*>(target);
    }

    void clear() override {
      id = nullptr;
      texture = nullptr;
      textureIndex = UINT32_MAX;
    }
  };

  struct TexturePacket {
    std::vector<vk::DescriptorImageInfo> imageInfos{};
    uint32_t textureCount = 0;
  };

}