module;

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

export module ufox_geometry_lib;

import ufox_lib;
import ufox_graphic_device;
import ufox_engine_lib;

export namespace ufox::geometry {

        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 color;
            glm::vec2 texCoord;

            static vk::VertexInputBindingDescription getBindingDescription(){ return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};}

            static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions(){
                return {
                    vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
                    vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
                    vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord))};
            }
        };

        vk::DeviceSize VERTEX_BUFFER_SIZE         = sizeof(Vertex);

        Vertex QuadVertices[]{
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
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

        constexpr auto QUAD = "Quad";
        constexpr auto CUBE = "Cube";

        struct Mesh final : engine::Content {
            std::vector<Vertex>                     vertices{};
            std::vector<uint16_t>                   indices{};

            gpu::vulkan::Buffer                     vertexBuffer{};
            gpu::vulkan::Buffer                     indexBuffer{};

            explicit Mesh(const std::string_view name_view, const engine::ContentSourceType src, const std::string_view category_view, const engine::ContentID& cid_): Content(name_view, src, category_view, cid_){}

            [[nodiscard]] bool hasValidGeometry() const noexcept { return !vertices.empty(); }
            [[nodiscard]] bool hasBuffer() const noexcept { return !vertexBuffer.isDataEmpty() && !indexBuffer.isDataEmpty(); }
            [[nodiscard]] uint32_t vertexCount() const noexcept { return static_cast<uint32_t>(vertices.size()); }
            [[nodiscard]] uint32_t indexCount() const noexcept { return static_cast<uint32_t>(indices.size()); }

            void releaseGpuResources() noexcept {
                vertexBuffer.clear();
                indexBuffer.clear();
            }

        };

        struct MeshUser {
            const engine::ContentID* id = nullptr;
            Mesh* mesh = nullptr;

            MeshUser() = default;
            ~MeshUser() = default;
            MeshUser(const engine::ContentID* id, Mesh* mesh) : id(id), mesh(mesh) {}

            void setNewTarget(const engine::ContentID* id, Mesh* mesh) {
                this->id = id;
                this->mesh = mesh;
            }

            void clear() {
                id = nullptr;
                mesh = nullptr;
            }
        };

        struct MeshContainerSlot {
            std::unique_ptr<Mesh>  mesh{nullptr};
            engine::ContentVersion         version{1};
            bool                   occupied{false};
            std::vector<MeshUser*> users{};
        };
}




