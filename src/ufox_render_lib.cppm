module;

#include <optional>
#include <vulkan/vulkan_raii.hpp>

export module ufox_render_lib;

import ufox_engine_lib;

export namespace ufox::render {

  constexpr size_t PIXEL_BYTE_SIZE = 4;
  constexpr vk::Format STANDARD_COLOR_TEXTURE_FORMAT = vk::Format::eR8G8B8A8Unorm;
  constexpr std::string_view TEXTURE_RESOURCE_PATH = "res/textures";
  constexpr std::array<std::string_view, 5> TEXTURE_RESOURCE_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".tga", ".bmp"
  };
  constexpr vk::ImageSubresourceRange DEFAULT_COLOR_SUBRESOURCE_RANGE{
  vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

  struct Color final {
    std::byte r{255};
    std::byte g{255};
    std::byte b{255};
    std::byte a{255};

    constexpr Color() noexcept = default;

    constexpr Color(const std::byte red, const std::byte green,
                             const std::byte blue,
                             const std::byte alpha = std::byte{255}) noexcept
        : r(red), g(green), b(blue), a(alpha) {}

    static constexpr Color White()  noexcept { return {}; }
    static constexpr Color Black()  noexcept { return {std::byte{0}, std::byte{0}, std::byte{0}}; }
    static constexpr Color Red()    noexcept { return {std::byte{255}, std::byte{0},   std::byte{0}}; }
    static constexpr Color Green()  noexcept { return {std::byte{0},   std::byte{255}, std::byte{0}}; }
    static constexpr Color Blue()   noexcept { return {std::byte{0},   std::byte{0},   std::byte{255}}; }
    static constexpr Color Gray()   noexcept { return {std::byte{128}, std::byte{128}, std::byte{128}}; }
    static constexpr Color Cyan()   noexcept { return {std::byte{0},   std::byte{255}, std::byte{255}}; }
  };


  struct Texture final : engine::ResourceBase {
    vk::Format                            format{STANDARD_COLOR_TEXTURE_FORMAT};
    std::vector<std::byte>                pixels{};
    vk::Extent2D                          extent{1,1};
    vk::ImageTiling                       tiling{vk::ImageTiling::eOptimal};
    vk::ImageSubresourceRange             subresourceRange{DEFAULT_COLOR_SUBRESOURCE_RANGE};

    std::optional<vk::raii::Image>        image{};
    std::optional<vk::raii::DeviceMemory> memory{};
    std::optional<vk::raii::ImageView>    view{};
    std::optional<vk::raii::Sampler>      sampler{};

    explicit Texture(const std::string_view name_view, const std::span<std::byte>& _pixels, const vk::Extent2D _extent, const engine::ResourceID& cid)
        : ResourceBase(name_view, cid), extent(_extent) {
      pixels.assign(std::begin(_pixels), std::end(_pixels));
    }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    [[nodiscard]] bool hasGpuResources() const noexcept override {
      return image.has_value() && view.has_value() && sampler.has_value();
    }

    [[nodiscard]] uint32_t width() const noexcept  { return extent.width; }
    [[nodiscard]] uint32_t height() const noexcept { return extent.height; }
    [[nodiscard]] vk::DeviceSize size() const noexcept {
      return static_cast<vk::DeviceSize>(extent.width)
           * static_cast<vk::DeviceSize>(extent.height) * PIXEL_BYTE_SIZE;
    }

    void releaseGpuResources() noexcept override {
      if (sampler.has_value()) sampler.reset();
      if (view.has_value()) view.reset();
      if (image.has_value()) image.reset();
      if (memory.has_value()) memory.reset();
    }

  };


}