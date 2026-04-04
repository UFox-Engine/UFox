module;

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <filesystem>
#include <chrono>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

export module ufox_geometry_lib;

import ufox_lib;
import ufox_graphic_device;
import ufox_engine_lib;

export namespace ufox::geometry {
/**
 *Path: "res/meshes"
 */
    constexpr std::string_view MESH_RESOURCE_PATH = "res/meshes";
    constexpr std::array<std::string_view, 2> MESH_RESOURCE_EXTENSION = {
        ".glb", ".obj"
    };

    struct Vertex
    {
        glm::vec3 pos{0.0f};
        glm::vec3 normal{0.0f};
        glm::vec2 texCoord{0.0f};
        glm::vec3 color{0.0f};

        bool operator==(const Vertex& other) const {
            return pos == other.pos &&
                   color == other.color &&
                   texCoord == other.texCoord;
        }

        static vk::VertexInputBindingDescription getBindingDescription(){ return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};}

        static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions(){
            return {
                vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
                vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)),
                vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)),
                vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))};
        }
    };

    struct Vertex2D{
        glm::vec3 pos;
        glm::vec2 texCoord;
        glm::vec3 color;

        static vk::VertexInputBindingDescription getBindingDescription(){ return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};}

        static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions(){
            return {
                vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
                vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)),
                vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))};
        }
    };

    vk::DeviceSize VERTEX_BUFFER_SIZE         = sizeof(Vertex);

    Vertex QuadVertices[]{
        {{-0.5f, -0.5f, 0.0f}, {1.0f,1.0f,1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f, 0.0f}, {1.0f,1.0f,1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f, 0.0f}, {1.0f,1.0f,1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f, 0.0f}, {1.0f,1.0f,1.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
      };

    uint16_t QuadIndices[]{
        0, 1, 2, 2, 3, 0
      };

    constexpr auto DEFAULT_QUAD_MESH_NAME = "default_quad_mesh";


    Vertex CubeVertices[] = {
        // Back face (z = -0.5)
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.2f, 0.2f}, {0.0f, 0.0f}},   // 0 – reddish
        {{ 0.5f, -0.5f, -0.5f}, {0.2f, 1.0f, 0.2f}, {1.0f, 0.0f}},   // 1 – greenish
        {{ 0.5f,  0.5f, -0.5f}, {0.2f, 0.2f, 1.0f}, {1.0f, 1.0f}},   // 2 – blueish
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.2f}, {0.0f, 1.0f}},   // 3 – yellowish

        // Front face (z = +0.5)
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},   // 4 – magenta
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},   // 5 – cyan
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},   // 6 – white
        {{-0.5f,  0.5f,  0.5f}, {0.7f, 0.7f, 0.7f}, {0.0f, 1.0f}},   // 7 – light gray
    };

    uint16_t CubeIndices[] = {
        0, 1, 2,    2, 3, 0,      // back
        4, 5, 6,    6, 7, 4,      // front
        0, 1, 5,    5, 4, 0,      // bottom
        3, 2, 6,    6, 7, 3,      // top
        0, 3, 7,    7, 4, 0,      // left
        1, 2, 6,    6, 5, 1       // right
    };



    constexpr auto DEFAULT_CUBE_MESH_NAME = "default_cube_mesh";

    constexpr auto QUAD = "quad";
    constexpr auto CUBE = "cube";

    constexpr engine::ResourceID BUILTIN_QUAD_ID  {QUAD};
    constexpr engine::ResourceID BUILTIN_CUBE_ID  {CUBE};

    struct Mesh final : engine::ResourceBase {
        std::vector<Vertex>                     vertices{};
        std::vector<uint16_t>                   indices{};

        std::optional<gpu::vulkan::Buffer>      vertexBuffer{};
        std::optional<gpu::vulkan::Buffer>      indexBuffer{};

        explicit Mesh(const std::string_view name_view, const std::span<Vertex>& _vertices, const std::span<uint16_t>& _indices, const engine::ResourceID& cid_): ResourceBase(name_view, cid_) {
            vertices.assign(std::begin(_vertices), std::end(_vertices));
            indices.assign(std::begin(_indices), std::end(_indices));
        }

        [[nodiscard]] bool hasValidGeometry() const noexcept { return !vertices.empty(); }
        [[nodiscard]] bool hasGpuResources() const noexcept override { return vertexBuffer.has_value() && indexBuffer.has_value(); }
        [[nodiscard]] uint32_t vertexCount() const noexcept { return static_cast<uint32_t>(vertices.size()); }
        [[nodiscard]] uint32_t indexCount() const noexcept { return static_cast<uint32_t>(indices.size()); }

        void releaseGpuResources() noexcept override{
            vertexBuffer.reset();
            indexBuffer.reset();
        }

    };

    struct MeshUser final : engine::ResourceUserBase {
        Mesh* mesh = nullptr;
        MeshUser() = default;

        MeshUser(const engine::ResourceID* _id, Mesh* mesh) : ResourceUserBase(_id), mesh(mesh) {}

        void setNewTarget(const engine::ResourceID* id, void* mesh) override{
            this->id = id;
            this->mesh = static_cast<Mesh*>(mesh);
        }

        void clear() override {
            id = nullptr;
            mesh = nullptr;
        }
    };
}

export namespace ufox::geometry::gltf {

    constexpr uint32_t GLB_MAGIC           = 0x46546C67u;   // "glTF"
    constexpr uint32_t GLB_VERSION         = 2u;
    constexpr uint32_t CHUNK_TYPE_JSON     = 0x4E4F534Au;   // "JSON"
    constexpr uint32_t CHUNK_TYPE_BIN      = 0x004E4942u;   // "BIN\0"

    struct GlbHeader {
        uint32_t magic   {0};
        uint32_t version {0};
        uint32_t length  {0};       // total file size
    };

    struct GlbChunkHeader {
        uint32_t chunkLength {0};
        uint32_t chunkType   {0};
    };

    enum class ComponentType : uint16_t {
        eByte           = 5120,
        eUnsignedByte   = 5121,
        eShort          = 5122,
        eUnsignedShort  = 5123,
        eUnsignedInt    = 5125,
        eFloat          = 5126,
    };

    enum class AccessorType : uint8_t {
        eScalar = 1,
        eVec2   = 2,
        eVec3   = 3,
        eVec4   = 4,
        // matrices skipped for now
    };

}
namespace std {
    template <>
    struct hash<ufox::geometry::Vertex>{
        size_t operator()(ufox::geometry::Vertex const &vertex) const noexcept{
            return (hash<glm::vec3>()(vertex.pos) ^
                  hash<glm::vec3>()(vertex.color) << 1) >>
                     1 ^ hash<glm::vec2>()(vertex.texCoord) << 1;
        }
    };
}


