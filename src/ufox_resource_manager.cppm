module;
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <stdexcept>
#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include <ktx.h>  // KTX library for texture loading
#include <nlohmann/json.hpp>

export module ufox_resource_manager;

import ufox_lib;  // For GPUResources, Buffer, GenerateUniqueID
import ufox_graphic_device;  // For utility functions like CopyBufferToImage, SetImageLayout

export namespace ufox {

    // Enum class for resource types
    enum class ResourceType {
        eUnidentify,  // Default for unidentified resources
        eTexture,
        eMesh,
        eAudio
    };

    struct Resource {
        virtual ~Resource() = default;

        bool                isPrivate{false};
        std::string         name;
        size_t              id{0};
        std::string         filePath;
        ResourceType        type{ResourceType::eUnidentify};

        void generateID() {
            std::vector<float> hashFields;
            for (const char c : filePath) {
                hashFields.push_back(c);
            }
            id = utilities::GenerateUniqueID(hashFields);
        }
    };

    struct TextureImageConfiguration {
        vk::Format                  format{vk::Format::eR8G8B8A8Unorm};
        vk::Extent3D                extent{1, 1, 1};
        vk::ImageUsageFlags         usage{vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled};
        vk::SampleCountFlagBits     sampleCount{vk::SampleCountFlagBits::e1};
        vk::MemoryPropertyFlags     properties{vk::MemoryPropertyFlagBits::eDeviceLocal};
        vk::ImageType               type{vk::ImageType::e2D};
        vk::ImageTiling             tiling{vk::ImageTiling::eOptimal};
        vk::SharingMode             shareMode{vk::SharingMode::eExclusive};
        uint32_t                    mipLevels{0};
        uint32_t                    arrayLayers{0};
        vk::ImageSubresourceRange   subresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        vk::ImageViewType           imageViewType{vk::ImageViewType::e2D};
    };

    constexpr TextureImageConfiguration DefaultTextureImageConfiguration{
        vk::Format::eR8G8B8A8Unorm, vk::Extent3D{512,512,1},
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::SampleCountFlagBits::e1, vk::MemoryPropertyFlagBits::eDeviceLocal,
        vk::ImageType::e2D, vk::ImageTiling::eOptimal, vk::SharingMode::eExclusive, 0,0,
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
        vk::ImageViewType::e2D
    };


    struct TextureImage : Resource {
        TextureImage() = default;
        explicit TextureImage(const std::string &filePath_, const TextureImageConfiguration& configuration_ = DefaultTextureImageConfiguration, const bool isPrivate_ = false) : configuration(configuration_) {
            std::filesystem::path path(filePath_);
            name  = path.stem().string();
            filePath = filePath_;
            isPrivate = isPrivate_;
            type = ResourceType::eTexture;
            generateID();

            debug::log(debug::LogLevel::eInfo, "Creating TextureImage {} from file {}", name, filePath);

            metadata = serializeMetadata();
            saveMetadataToFile();
        }
        TextureImage(const std::string &filePath_,
            const vk::Extent3D extent_, const vk::Format format_,
            const vk::ImageUsageFlags usage_ = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
            const vk::SampleCountFlagBits sampleCount_ = vk::SampleCountFlagBits::e1,
            const vk::MemoryPropertyFlags properties_ = vk::MemoryPropertyFlagBits::eDeviceLocal,
            const vk::ImageType type_ = vk::ImageType::e2D, vk::ImageViewType viewType_ = vk::ImageViewType::e2D, const vk::ImageTiling tiling_ = vk::ImageTiling::eOptimal,
            const vk::SharingMode shareMode_ = vk::SharingMode::eExclusive, const uint32_t mipLevels_ = 0, const uint32_t arrayLayers_ = 0,
            const vk::ImageSubresourceRange &subresourceRange_ = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
            const bool isPrivate_ = false){

            std::filesystem::path path(filePath_);
            name  = path.stem().string();
            filePath = filePath_;
            isPrivate = isPrivate_;
            type = ResourceType::eTexture;
            generateID();

            debug::log(debug::LogLevel::eInfo, "Creating TextureImage {} from file {}", name, filePath);

            configuration.format = format_;
            configuration.extent = extent_;
            configuration.usage = usage_;
            configuration.sampleCount = sampleCount_;
            configuration.properties = properties_;
            configuration.type = type_;
            configuration.tiling = tiling_;
            configuration.shareMode = shareMode_;
            configuration.mipLevels = mipLevels_;
            configuration.arrayLayers = arrayLayers_;
            configuration.subresourceRange = subresourceRange_;
            configuration.imageViewType = viewType_;

            metadata = serializeMetadata();
            saveMetadataToFile();
        }

        [[nodiscard]] vk::ImageCreateInfo GetImageCreateInfo() const {
            vk::ImageCreateInfo info{};
            info.setImageType(configuration.type);
            info.setFormat(configuration.format);
            info.setExtent(configuration.extent);
            info.setMipLevels(configuration.mipLevels);
            info.setArrayLayers(configuration.arrayLayers);
            info.setSamples(configuration.sampleCount);
            info.setTiling(configuration.tiling);
            info.setUsage(configuration.usage);
            info.setSharingMode(configuration.shareMode);
            return info;
        }

        [[nodiscard]] vk::ImageViewCreateInfo GetImageViewCreateInfo() const {
            vk::ImageViewCreateInfo info{};
            info.setImage(*data);
            info.setViewType(configuration.imageViewType);
            info.setFormat(configuration.format);
            info.setSubresourceRange(configuration.subresourceRange);
            return info;
        }


        std::optional<vk::raii::Image>              data{};
        std::optional<vk::raii::DeviceMemory>       memory{};
        std::optional<vk::raii::ImageView>          view{};
        TextureImageConfiguration                   configuration{};
        std::string                                 metadata{};


        void clear() {
            view.reset();
            data.reset();
            memory.reset();
        }

        [[nodiscard]] std::string serializeMetadata() const {
            nlohmann::json json;
            json["type"] = type;
            json["name"] = name;
            json["id"] = id;
            json["filePath"] = filePath;
            json["isPrivate"] = isPrivate;
            json["configuration"] = {
                {"format", static_cast<int>(configuration.format)},
                {"extent", {
                        {"width", configuration.extent.width},
                        {"height", configuration.extent.height},
                        {"depth", configuration.extent.depth}
                }},
                {"usage", static_cast<uint32_t>(configuration.usage)},
                {"sampleCount", static_cast<int>(configuration.sampleCount)},
                {"properties", static_cast<uint32_t>(configuration.properties)},
                {"type", static_cast<int>(configuration.type)},
                {"tiling", static_cast<int>(configuration.tiling)},
                {"shareMode", static_cast<int>(configuration.shareMode)},
                {"mipLevels", configuration.mipLevels},
                {"arrayLayers", configuration.arrayLayers},
                {"subresourceRange", {
                        {"aspectMask", static_cast<uint32_t>(configuration.subresourceRange.aspectMask)},
                        {"baseMipLevel", configuration.subresourceRange.baseMipLevel},
                        {"levelCount", configuration.subresourceRange.levelCount},
                        {"baseArrayLayer", configuration.subresourceRange.baseArrayLayer},
                        {"layerCount", configuration.subresourceRange.layerCount}
                }},
                {"imageViewType", static_cast<int>(configuration.imageViewType)}
            };
            return json.dump(2); // Pretty print with 2-space indentation
        }

        void saveMetadataToFile() const {
            try {
                std::filesystem::path texturePath(filePath);
                std::filesystem::path metadataDir = texturePath.parent_path(); // Get directory of texture file
                std::filesystem::create_directories(metadataDir); // Ensure directory exists
                std::filesystem::path filePath = metadataDir / (name + ".png.meta"); // Use .png.meta extension
                std::ofstream file(filePath, std::ios::out | std::ios::binary);
                if (!file.is_open()) {
                    debug::log(debug::LogLevel::eError, "Failed to open metadata file for writing: {}", filePath.string());
                    return;
                }
                file << metadata;
                file.close();
                debug::log(debug::LogLevel::eInfo, "Saved metadata for TextureImage {} to {}", name, filePath.string());
            } catch (const std::filesystem::filesystem_error& e) {
                debug::log(debug::LogLevel::eError, "Failed to save metadata for TextureImage {}: {}", name, e.what());
            } catch (const std::exception& e) {
                debug::log(debug::LogLevel::eError, "Unexpected error saving metadata for TextureImage {}: {}", name, e.what());
            }
        }
    };




    // Enum class for image file types (flags)
    enum class ImageFileType {
        None = 0,
        PNG = 1 << 0,  // Bit 0
        JPEG = 1 << 1  // Bit 1
    };
    // Allow combining flags
    inline ImageFileType operator|(ImageFileType lhs, ImageFileType rhs) {
        return static_cast<ImageFileType>(static_cast<int>(lhs) | static_cast<int>(rhs));
    }
    inline ImageFileType operator&(ImageFileType lhs, ImageFileType rhs) {
        return static_cast<ImageFileType>(static_cast<int>(lhs) & static_cast<int>(rhs));
    }

    class ResourceManager {
    public:
        explicit ResourceManager(const gpu::vulkan::GPUResources& gpu_)
            : gpu(gpu_) {
            // Create command pool using helper
            commandPool.emplace(gpu::vulkan::MakeCommandPool(gpu));

            // Allocate primary command buffer
            vk::CommandBufferAllocateInfo   allocInfo{};
                                            allocInfo
                                                .setCommandPool(*commandPool)
                                                .setLevel(vk::CommandBufferLevel::ePrimary)
                                                .setCommandBufferCount(1);
            auto buffers = gpu.device->allocateCommandBuffers(allocInfo);
            commandBuffer.emplace(std::move(buffers[0]));
        }

        ~ResourceManager() = default;


        // Set the root path for resources
        void SetRootPath(const std::string& rootPath_) {
            rootPath = rootPath_;
            // Ensure rootPath ends with a separator for consistency
            if (!rootPath.empty() && rootPath.back() != '/' && rootPath.back() != '\\') {
                rootPath += '/';
            }
        }

#pragma region Texture Resource

        // Scan for image files within rootPath and its subdirectories based on extensions
        std::vector<std::string> ScanForImageFiles(const std::vector<std::string>& extensions) const {
            std::vector<std::string> imageFiles;
            if (rootPath.empty() || extensions.empty()) {
                return imageFiles; // Return empty list if rootPath or extensions not set
            }
            try {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(rootPath)) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        // Convert extension to lowercase for case-insensitive comparison
                        std::ranges::transform(ext, ext.begin(),
                                               [](const unsigned char c) { return std::tolower(c); });
                        // Check if extension matches any in the provided list
                        for (const auto& targetExt : extensions) {
                            std::string target = targetExt;
                            // Ensure target extension starts with '.' and is lowercase
                            if (!target.empty() && target[0] != '.') {
                                target = std::format(".{}", target);
                            }
                            std::ranges::transform(target, target.begin(),
                                                   [](const unsigned char c) { return std::tolower(c); });
                            if (ext == target) {
                                imageFiles.push_back(entry.path().string());
                                break; // Found a match, no need to check other extensions
                            }
                        }
                    }
                }
            } catch (const std::filesystem::filesystem_error& e) {
                debug::log(debug::LogLevel::eError, "Failed to scan directory {}: {}", rootPath, e.what());
            }
            return imageFiles;
        }

        // void CreateTextureImage() {
        //     // Load KTX2 texture instead of using stb_image
        //     ktxTexture* kTexture;
        //     KTX_error_code result = ktxTexture_CreateFromNamedFile(
        //         "res/textures/rgb.png",
        //         KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        //         &kTexture);
        //
        //     if (result != KTX_SUCCESS) {
        //         throw std::runtime_error("failed to load ktx texture image!");
        //     }
        //
        //     // Get texture dimensions and data
        //     uint32_t texWidth = kTexture->baseWidth;
        //     uint32_t texHeight = kTexture->baseHeight;
        //     ktx_size_t imageSize = ktxTexture_GetImageSize(kTexture, 0);
        //     ktx_uint8_t* ktxTextureData = ktxTexture_GetData(kTexture);
        //
        //     // Create staging buffer
        //     gpu::vulkan::Buffer stagingBuffer(gpu, imageSize, vk::BufferUsageFlagBits::eTransferSrc);
        //
        //     // Copy texture data to staging buffer
        //     auto pData = static_cast<uint8_t *>( stagingBuffer.memory->mapMemory( 0, imageSize ) );
        //     memcpy( pData, ktxTextureData, imageSize );
        //     stagingBuffer.memory->unmapMemory();
        //
        //     // Determine the Vulkan format from KTX format
        //     vk::Format textureFormat = vk::Format::eR8G8B8A8Srgb; // Default format, should be determined from KTX metadata
        //
        //     // Create the texture image
        //     textureImage.emplace(gpu, vk::Extent3D{texWidth,texHeight,1}, textureFormat, vk::ImageTiling::eOptimal,
        //         vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
        //
        //     // Copy data from staging buffer to texture image
        //     gpu::vulkan::SetImageLayout(commandBuffer, *textureImage, textureFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        //     gpu::vulkan::CopyBufferToImage(gpu,stagingBuffer, *textureImage, {texWidth, texHeight, 1});
        //     gpu::vulkan::SetImageLayout(commandBuffer, *textureImage, textureFormat, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        //
        //
        //
        //     // Cleanup KTX resources
        //     ktxTexture_Destroy(kTexture);
        // }
#pragma endregion

    private:
        const gpu::vulkan::GPUResources& gpu;
        std::optional<vk::raii::CommandPool> commandPool{};
        std::optional<vk::raii::CommandBuffer> commandBuffer{};
        std::string rootPath;  // Root path for resources
        std::unordered_map<std::string, std::unique_ptr<TextureImage>> textures;
    };
}  // namespace ufox::resources