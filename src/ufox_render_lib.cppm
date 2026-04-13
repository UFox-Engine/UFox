module;

#include <optional>
#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <glm/glm.hpp>

export module ufox_render_lib;

import ufox_engine_lib;
import ufox_gpu_lib;

export namespace ufox::render {

  inline size_t PIXEL_BYTE_SIZE = 4;
  inline vk::Format STANDARD_COLOR_TEXTURE_FORMAT = vk::Format::eR8G8B8A8Unorm;
  inline auto TEXTURE_RESOURCE_PATH = "res/textures";
  inline std::array<std::string_view, 5> TEXTURE_RESOURCE_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".tga", ".bmp"
  };

  inline  vk::ImageSubresourceRange DEFAULT_COLOR_SUBRESOURCE_RANGE{
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
    vk::ImageType                         type{vk::ImageType::e2D};
    vk::Format                            format{STANDARD_COLOR_TEXTURE_FORMAT};
    std::vector<std::byte>                pixels{};
    vk::Extent2D                          extent{1,1};
    vk::ImageTiling                       tiling{vk::ImageTiling::eOptimal};
    vk::ImageSubresourceRange             subresourceRange{DEFAULT_COLOR_SUBRESOURCE_RANGE};

    gpu::Image                            image{};
    std::optional<vk::raii::Sampler>      sampler{};

    explicit Texture(const std::string_view name_view, const std::span<std::byte>& _pixels, const vk::Extent2D _extent, const engine::ResourceID& cid)
        : ResourceBase(name_view, cid), extent(_extent) {
      pixels.assign(std::begin(_pixels), std::end(_pixels));
    }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    [[nodiscard]] bool hasGpuResources() const noexcept override {
      return image.hasGpuResources();
    }

    [[nodiscard]] bool hasSampler() const noexcept { return sampler.has_value(); }


    [[nodiscard]] uint32_t width() const noexcept  { return extent.width; }
    [[nodiscard]] uint32_t height() const noexcept { return extent.height; }
    [[nodiscard]] vk::DeviceSize size() const noexcept {
      return static_cast<vk::DeviceSize>(extent.width)
           * static_cast<vk::DeviceSize>(extent.height) * PIXEL_BYTE_SIZE;
    }

    void releaseGpuResources() noexcept override {
      image.clear();
      if (sampler.has_value()) sampler.reset();
    }
  };
  namespace msdf {
    struct GlyphContext {
      uint32_t codepoint = 0;
      double advance = 0.0;
      int boxWidth {0};
      int boxHeight {0};
      glm::dvec4 plane{0.0,0.0,0.0,0.0};
      glm::dvec4 atlas{0.0,0.0,0.0,0.0};
    };

    struct MSDFFontData {
      std::vector<std::byte> pixels{};
      vk::Extent2D extent{0,0};
      std::vector<GlyphContext> glyphs{};
      engine::ResourceContextCreateInfo contextCreateInfo{};
    };
  }

}