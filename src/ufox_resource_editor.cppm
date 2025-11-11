export module ufox_resource_editor;

import ufox_lib;  // For SerializeElement, debug::log
import ufox_resource_manager;  // For TextureImage
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <vulkan/vulkan_raii.hpp>

export namespace ufox {


    struct TextureImageResourceEditor {
        std::vector<std::string> metadata; // Store serialized metadata
        std::vector<SerializeElement> elements; // Hierarchical serialization data
        TextureImage* target; // Reference to the associated TextureImage

        // Constructor to initialize with a TextureImage for testing
        TextureImageResourceEditor(TextureImage& texture) : target(&texture) {
            // Initialize SerializeElement hierarchy

            elements = {
                {"resourceType"     ,   "eTexture"                              ,   {}},
                {"name"             ,   texture.name                            ,   {}},
                {"id"               ,   std::to_string(texture.id)              ,   {}},
                {"filePath"         ,   texture.filePath                        ,   {}},
                {"isPrivate"        ,   texture.isPrivate ? "true" : "false"    ,   {}},
                {"configuration"    ,   ""                                      ,   {
                    { "format"      ,   std::to_string(static_cast<int>(texture.configuration.format)), {}},
                    { "extent"      ,   ""                                      ,   {
                        { "width"   ,   std::to_string(texture.configuration.extent.width)  ,   {}},
                        { "height"  ,   std::to_string(texture.configuration.extent.height) ,   {}},
                        { "depth"   ,   std::to_string(texture.configuration.extent.depth)  ,   {}}}},
                    { "usage"       ,   std::to_string(static_cast<uint32_t>(texture.configuration.usage))      , {}},
                    { "sampleCount" ,   std::to_string(static_cast<int>(texture.configuration.sampleCount))     , {}},
                    { "properties"  ,   std::to_string(static_cast<uint32_t>(texture.configuration.properties)) , {}},
                    { "type"        ,   std::to_string(static_cast<int>(texture.configuration.type))            , {}},
                    { "tiling"      ,   std::to_string(static_cast<int>(texture.configuration.tiling))          , {}},
                    { "shareMode"   ,   std::to_string(static_cast<int>(texture.configuration.shareMode))       , {}},
                    { "mipLevels"   ,   std::to_string(texture.configuration.mipLevels)                         , {}},
                    { "arrayLayers" ,   std::to_string(texture.configuration.arrayLayers)                       , {}},
                    { "subresourceRange"    , "", {
                        { "aspectMask"      , std::to_string(static_cast<uint32_t>(texture.configuration.subresourceRange.aspectMask))  , {}},
                        { "baseMipLevel"    , std::to_string(texture.configuration.subresourceRange.baseMipLevel)                       , {}},
                        { "levelCount"      , std::to_string(texture.configuration.subresourceRange.levelCount)                         , {}},
                        { "baseArrayLayer"  , std::to_string(texture.configuration.subresourceRange.baseArrayLayer)                     , {}},
                        { "layerCount"      , std::to_string(texture.configuration.subresourceRange.layerCount)                         , {}}}},
                    { "imageViewType"       , std::to_string(static_cast<int>(texture.configuration.imageViewType))                     , {}}}}
            };


            // Serialize to JSON and store in metadata
            metadata.push_back(serializeMetadata());
            saveMetadataToFile();
            debug::log(debug::LogLevel::eInfo, "Created TextureImageResourceEditor for texture {} with target set", texture.name);
        }

        // Deserialize metadata from file and create new TextureImage
        std::unique_ptr<TextureImage> deserializeMetadata(const std::string& filePath) {
            try {
                std::ifstream file(filePath, std::ios::in | std::ios::binary);
                if (!file.is_open()) {
                    debug::log(debug::LogLevel::eError, "Failed to open metadata file for reading: {}", filePath);
                    return nullptr;
                }
                std::stringstream buffer;
                buffer << file.rdbuf();
                file.close();
                std::string content = buffer.str();
                nlohmann::json json = nlohmann::json::parse(content);

                // Clear existing element and metadata
                element = SerializeElement();
                metadata.clear();
                metadata.push_back(content);

                // Check if JSON is nested under "texture"
                if (json.contains("texture")) {
                    json = json["texture"];
                }

                // Populate SerializeElement
                element.name = "texture";
                element.value = json.contains("name") ? json["name"].get<std::string>() : std::filesystem::path(filePath).stem().string();
                element.children = {
                    { "type", json.contains("type") ? (json["type"].is_number() ? "eTexture" : json["type"].get<std::string>()) : "eTexture", {} },
                    { "name", element.value, {} },
                    { "id", json.contains("id") ? json["id"].get<std::string>() : "0", {} },
                    { "filePath", json.contains("filePath") ? json["filePath"].get<std::string>() : filePath, {} },
                    { "isPrivate", json.contains("isPrivate") ? (json["isPrivate"].get<bool>() ? "true" : "false") : "false", {} }
                };
                if (json.contains("configuration")) {
                    auto& config = json["configuration"];
                    SerializeElement configElement{ "configuration", "", {} };
                    if (config.contains("extent")) {
                        SerializeElement extentElement{ "extent", "", {
                            { "width", config["extent"].contains("width") ? config["extent"]["width"].get<std::string>() : "512", {} },
                            { "height", config["extent"].contains("height") ? config["extent"]["height"].get<std::string>() : "512", {} },
                            { "depth", config["extent"].contains("depth") ? config["extent"]["depth"].get<std::string>() : "1", {} }
                        } };
                        configElement.children.push_back(extentElement);
                    }
                    configElement.children.insert(configElement.children.end(), {
                        { "format", config.contains("format") ? config["format"].get<std::string>() : "37", {} },
                        { "usage", config.contains("usage") ? config["usage"].get<std::string>() : "24", {} },
                        { "sampleCount", config.contains("sampleCount") ? config["sampleCount"].get<std::string>() : "1", {} },
                        { "properties", config.contains("properties") ? config["properties"].get<std::string>() : "1", {} },
                        { "type", config.contains("type") ? config["type"].get<std::string>() : "1", {} },
                        { "tiling", config.contains("tiling") ? config["tiling"].get<std::string>() : "1", {} },
                        { "shareMode", config.contains("shareMode") ? config["shareMode"].get<std::string>() : "0", {} },
                        { "mipLevels", config.contains("mipLevels") ? config["mipLevels"].get<std::string>() : "0", {} },
                        { "arrayLayers", config.contains("arrayLayers") ? config["arrayLayers"].get<std::string>() : "0", {} },
                        { "subresourceRange", "", {
                            { "aspectMask", config["subresourceRange"].contains("aspectMask") ? config["subresourceRange"]["aspectMask"].get<std::string>() : "1", {} },
                            { "baseMipLevel", config["subresourceRange"].contains("baseMipLevel") ? config["subresourceRange"]["baseMipLevel"].get<std::string>() : "0", {} },
                            { "levelCount", config["subresourceRange"].contains("levelCount") ? config["subresourceRange"]["levelCount"].get<std::string>() : "1", {} },
                            { "baseArrayLayer", config["subresourceRange"].contains("baseArrayLayer") ? config["subresourceRange"]["baseArrayLayer"].get<std::string>() : "0", {} },
                            { "layerCount", config["subresourceRange"].contains("layerCount") ? config["subresourceRange"]["layerCount"].get<std::string>() : "1", {} }
                        } },
                        { "imageViewType", config.contains("imageViewType") ? config["imageViewType"].get<std::string>() : "1", {} }
                    });
                    element.children.push_back(configElement);
                }

                // Create new TextureImage
                std::string textureFilePath;
                bool isPrivate = false;
                TextureImageConfiguration config = DefaultTextureImageConfiguration;

                for (const auto& child : element.children) {
                    if (child.name == "filePath") {
                        textureFilePath = child.value;
                    } else if (child.name == "isPrivate") {
                        isPrivate = (child.value == "true");
                    } else if (child.name == "configuration") {
                        for (const auto& configChild : child.children) {
                            try {
                                if (configChild.name == "format") config.format = static_cast<vk::Format>(std::stoi(configChild.value));
                                else if (configChild.name == "extent") {
                                    for (const auto& extentChild : configChild.children) {
                                        if (extentChild.name == "width") config.extent.width = std::stoi(extentChild.value);
                                        else if (extentChild.name == "height") config.extent.height = std::stoi(extentChild.value);
                                        else if (extentChild.name == "depth") config.extent.depth = std::stoi(extentChild.value);
                                    }
                                } else if (configChild.name == "usage") config.usage = static_cast<vk::ImageUsageFlags>(std::stoi(configChild.value));
                                else if (configChild.name == "sampleCount") config.sampleCount = static_cast<vk::SampleCountFlagBits>(std::stoi(configChild.value));
                                else if (configChild.name == "properties") config.properties = static_cast<vk::MemoryPropertyFlags>(std::stoi(configChild.value));
                                else if (configChild.name == "type") config.type = static_cast<vk::ImageType>(std::stoi(configChild.value));
                                else if (configChild.name == "tiling") config.tiling = static_cast<vk::ImageTiling>(std::stoi(configChild.value));
                                else if (configChild.name == "shareMode") config.shareMode = static_cast<vk::SharingMode>(std::stoi(configChild.value));
                                else if (configChild.name == "mipLevels") config.mipLevels = std::stoi(configChild.value);
                                else if (configChild.name == "arrayLayers") config.arrayLayers = std::stoi(configChild.value);
                                else if (configChild.name == "subresourceRange") {
                                    for (const auto& subChild : configChild.children) {
                                        if (subChild.name == "aspectMask") config.subresourceRange.aspectMask = static_cast<vk::ImageAspectFlags>(std::stoi(subChild.value));
                                        else if (subChild.name == "baseMipLevel") config.subresourceRange.baseMipLevel = std::stoi(subChild.value);
                                        else if (subChild.name == "levelCount") config.subresourceRange.levelCount = std::stoi(subChild.value);
                                        else if (subChild.name == "baseArrayLayer") config.subresourceRange.baseArrayLayer = std::stoi(subChild.value);
                                        else if (subChild.name == "layerCount") config.subresourceRange.layerCount = std::stoi(subChild.value);
                                    }
                                } else if (configChild.name == "imageViewType") config.imageViewType = static_cast<vk::ImageViewType>(std::stoi(configChild.value));
                            } catch (const std::exception& e) {
                                debug::log(debug::LogLevel::eWarning, "Invalid value for {} in metadata: {}", configChild.name, e.what());
                            }
                        }
                    }
                }

                // Create and set new TextureImage
                auto texture = std::make_unique<TextureImage>(textureFilePath, config, isPrivate);
                target = texture.get();
                debug::log(debug::LogLevel::eInfo, "Deserialized metadata from {} and created TextureImage {}", filePath, texture->name);
                return texture;
            } catch (const std::exception& e) {
                debug::log(debug::LogLevel::eError, "Failed to deserialize metadata from {}: {}", filePath, e.what());
                return nullptr;
            }
        }

        // Serialize SerializeElement hierarchy to JSON
        [[nodiscard]] std::string serializeMetadata() const {
            nlohmann::json json = serializeElementToJson(elements);
            return json.dump(2); // Pretty print with 2-space indentation
        }

        // Save metadata to file
        void saveMetadataToFile() const {
            try {
                std::filesystem::path texturePath(target->filePath);
                std::filesystem::path metadataDir = texturePath.parent_path();
                std::filesystem::create_directories(metadataDir);
                std::string extension = texturePath.extension().string();
                std::string metaFilename = target->name + (extension.empty() ? "" : extension) + ".meta";
                std::filesystem::path filePath = metadataDir / metaFilename;
                std::ofstream file(filePath, std::ios::out | std::ios::binary);
                if (!file.is_open()) {
                    debug::log(debug::LogLevel::eError, "Failed to open metadata file for writing: {}", filePath.string());
                    return;
                }
                file << metadata.front();
                file.close();
                debug::log(debug::LogLevel::eInfo, "Saved metadata for TextureImageResourceEditor {} to {}", target->name, filePath.string());
            } catch (const std::filesystem::filesystem_error& e) {
                debug::log(debug::LogLevel::eError, "Failed to save metadata for TextureImageResourceEditor {}: {}", target->name, e.what());
            } catch (const std::exception& e) {
                debug::log(debug::LogLevel::eError, "Unexpected error saving metadata for TextureImageResourceEditor {}: {}", target->name, e.what());
            }
        }

    private:
        // Helper to convert SerializeElement to JSON
        nlohmann::json serializeElementToJson(const SerializeElement& elem) const {
            nlohmann::json json;
            json[elem.name] = elem.value.empty() ? nlohmann::json::object() : elem.value;
            if (!elem.children.empty()) {
                for (const auto& child : elem.children) {
                    json[elem.name][child.name] = serializeElementToJson(child);
                }
            }
            return json;
        }
    };

} // namespace ufox
