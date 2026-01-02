module;
#include <iostream>
#include <unordered_map>
#include <vulkan/vulkan_raii.hpp>
#ifdef USE_SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#else
#include <GLFW/glfw3.h>
#endif

#include <chrono>
#include <glm/glm.hpp>
#include <optional>
#include <set>
#include <vector>

export module ufox_geometry_core;

import ufox_lib;
import ufox_engine_lib;
import ufox_geometry_lib;
import ufox_input;
import ufox_graphic_device;

using namespace ufox::engine;

export namespace ufox::geometry {

    class MeshManager final {
    public:
        static constexpr std::string_view kDefaultMeshName{"unnamed_mesh"};
        static constexpr std::string_view kDefaultCategory{"Miscellaneous"};

        explicit MeshManager(const gpu::vulkan::GPUResources& gpu) noexcept
            : gpuResources(&gpu) {}

        MeshManager(const MeshManager&) = delete;
        MeshManager& operator=(const MeshManager&) = delete;

        ~MeshManager() {
            clearAllGpuResources();
        }

        const ContentID* createMesh(std::string_view name = kDefaultMeshName, ContentSourceType src = ContentSourceType::eBuiltIn, std::string_view category = kDefaultCategory) {
            const ContentIndex idx = acquireSlot();
            auto& slot = slots[idx];
            slot.mesh = std::make_unique<Mesh>(name, src, category, ContentID{idx, slot.version});
            return &slot.mesh->cid;
        }

        void destroy(const ContentID id, MeshContainerSlot& slot) {
            releaseMeshSlot(id.index, slot);
        }

        void destroy(const ContentID id) {
            auto* slot = tryGetSlot(id);
            if (slot == nullptr || slot->mesh == nullptr) return;

            releaseMeshSlot(id.index, *slot);
        }

        Mesh* get(const ContentID id) {
          const auto * slot = tryGetSlot(id);
            return slot && slot->mesh ? slot->mesh.get() : nullptr;
        }

        [[nodiscard]] const Mesh* get(const ContentID id) const {
            const auto* slot = tryGetSlot(id);
            return slot && slot->mesh ? slot->mesh.get() : nullptr;
        }

        [[nodiscard]] size_t getUsage(const ContentID& id) const {
            const auto* slot = tryGetSlot(id);
            return slot ? slot->users.size() : 0;
        }

        [[nodiscard]] size_t aliveCount() const noexcept {
            return aliveMeshes;
        }

        void useMesh(MeshUser& user) {
            if (user.id == nullptr) return;

            auto* slot = tryGetSlot(*user.id);
            if (slot == nullptr || slot->mesh == nullptr) return;

            Mesh& mesh = *slot->mesh;
            user.mesh = &mesh;

            if (const auto it = std::ranges::find(slot->users, &user); it == slot->users.end()) {
                slot->users.push_back(&user);
            }

            ensureMeshBuffers(mesh);
        }

        void unuseMesh(MeshUser& user, const bool destroyLast = false) {
            if (user.id == nullptr) return;
            user.mesh = nullptr;

            auto* slot = tryGetSlot(*user.id);
            if (slot == nullptr) return;


            if (const auto it = std::ranges::find(slot->users, &user); it != slot->users.end()) {
                slot->users.erase(it);
            }

            if (!slot->users.empty()) return;


            if (destroyLast) {
                destroy(*user.id, *slot);
                return;
            }

            if (slot->mesh == nullptr) return;
            Mesh& mesh = *slot->mesh;

            if (mesh.hasBuffer()) {
                mesh.releaseGpuResources();
                debug::log(debug::LogLevel::eInfo,
                           "MeshManager: unuseMesh [{}]: mesh released buffers",
                           mesh.name);
            }
        }

        void clearAllGpuResources() const {
            size_t clearedCount = 0;

            for (auto& slot : slots) {
                if (!slot.occupied || !slot.mesh || !slot.mesh->hasBuffer()) continue;

                slot.mesh->releaseGpuResources();
                ++clearedCount;

                debug::log(debug::LogLevel::eInfo,
                           "MeshManager: clearAllGpuResources: cleared mesh GPU resources for mesh: {}",
                           slot.mesh->name);
            }

            if (clearedCount > 0) {
                debug::log(debug::LogLevel::eInfo,
                           "MeshManager: clearAllGpuResources: cleared {} mesh GPU resources",
                           clearedCount);
            }
        }

        void debugPrintStatus(std::ostream& os = std::cout) const {
            os << "MeshManager status:\n"
               << "  Alive meshes   : " << aliveMeshes << "\n"
               << "  Total slots    : " << slots.size() << "\n"
               << "  Free slots     : " << freeSlots.size() << "\n"
               << "  Fragmentation  : " << (slots.size() - aliveMeshes) << " holes\n";
        }

    private:

        template <typename Container>
        static void createGpuBuffer(const gpu::vulkan::GPUResources& gpu, const Container& source, gpu::vulkan::Buffer& destinationBuffer, vk::BufferUsageFlags finalUsage) {
            if (source.empty()) return;

            if (!destinationBuffer.isDataEmpty()) {
                destinationBuffer.data.reset();
            }

            const vk::DeviceSize bufferSize = sizeof(source[0]) * source.size();

            gpu::vulkan::Buffer stagingBuffer{};
            gpu::vulkan::MakeBuffer(
                stagingBuffer,
                gpu,
                bufferSize,
                vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            gpu::vulkan::CopyToDevice(stagingBuffer.memory.value(), source.data(), bufferSize);

            gpu::vulkan::MakeBuffer(
                destinationBuffer,
                gpu,
                bufferSize,
                finalUsage | vk::BufferUsageFlagBits::eTransferDst,
                vk::MemoryPropertyFlagBits::eDeviceLocal);

            gpu::vulkan::CopyBuffer(gpu, stagingBuffer, destinationBuffer, bufferSize);
        }

        static void createVertexBuffer(const gpu::vulkan::GPUResources& gpu, Mesh& mesh) {
            createGpuBuffer(gpu, mesh.vertices, mesh.vertexBuffer, vk::BufferUsageFlagBits::eVertexBuffer);
        }

        static void createIndexBuffer(const gpu::vulkan::GPUResources& gpu, Mesh& mesh) {
            createGpuBuffer(gpu, mesh.indices, mesh.indexBuffer, vk::BufferUsageFlagBits::eIndexBuffer);
        }

        void ensureMeshBuffers(Mesh& mesh) const {
            if (mesh.hasBuffer()) return;
            createVertexBuffer(*gpuResources, mesh);
            createIndexBuffer(*gpuResources, mesh);
        }

        [[nodiscard]] bool isAlive(const ContentID& id) const noexcept {
            return id.IsValid() && id.index < slots.size();
        }

        [[nodiscard]] MeshContainerSlot* tryGetSlot(const ContentID& id) noexcept {
            if (!isAlive(id)) return nullptr;
            MeshContainerSlot* slot = &slots[id.index];

            return slot->occupied && slot->version == id.version ? slot : nullptr;
        }

        [[nodiscard]] const MeshContainerSlot* tryGetSlot(const ContentID& id) const noexcept {
            return isAlive(id) ? &slots[id.index] : nullptr;
        }

        ContentIndex acquireSlot() {
            ContentIndex idx;

            if (!freeSlots.empty()) {
                idx = freeSlots.back();
                freeSlots.pop_back();
            } else {
                idx = static_cast<ContentIndex>(slots.size());
                slots.emplace_back();
            }

            auto& slot = slots[idx];
            slot.occupied = true;
            slot.version = nextVersion++;
            ++aliveMeshes;
            return idx;
        }

        void releaseMeshSlot(const ContentIndex idx, MeshContainerSlot& slot) {
            if (slot.mesh) {
                for (const std::span users{slot.users.data(), slot.users.size()};
                   MeshUser * other : users) {
                    if (other) unuseMesh(*other, false);
                   }

                const std::string_view meshName = slot.mesh->name;
                slot.mesh->releaseGpuResources();
                slot.mesh.reset();

                debug::log(debug::LogLevel::eInfo,"MeshManager: Destroy and Release Buffer [{}]",meshName);
            }

            slot.users.clear();
            slot.occupied = false;
            --aliveMeshes;
            freeSlots.push_back(idx);
        }

        const gpu::vulkan::GPUResources* gpuResources{nullptr};
        std::vector<MeshContainerSlot> slots{};
        std::vector<ContentIndex> freeSlots{};
        size_t aliveMeshes{0};
        ContentVersion nextVersion{1};
    };

    const ContentID* CreateMeshContent(MeshManager& manager, const std::string_view& name, const std::span<Vertex>& vertices, const std::span<uint16_t>& indices, const ContentSourceType& sourType = ContentSourceType::eBuiltIn, const std::string_view& category = "Miscellaneous") {
        const ContentID* id = manager.createMesh(name, sourType, category);
        Mesh* m = manager.get(*id);
        if (!m) return nullptr;
        m->vertices.assign(std::begin(vertices), std::end(vertices));
        m->indices.assign(std::begin(indices), std::end(indices));
        return id;
    }

    const ContentID* CreateQuad(MeshManager& manager) {
        return CreateMeshContent(manager, DEFAULT_QUAD_MESH_NAME, QuadVertices, QuadIndices, ContentSourceType::eBuiltIn, "Default");
    }

    const ContentID* CreateCube(MeshManager& manager) {
        return CreateMeshContent(manager, DEFAULT_CUBE_MESH_NAME, CubeVertices, CubeIndices, ContentSourceType::eBuiltIn, "Default");
    }

    using DefaultMeshesResources = std::unordered_map<std::string, const ContentID*>;

    DefaultMeshesResources CreateDefaultMeshResources(MeshManager &manager) {
        return {{"quad", CreateQuad(manager)},
                {"cube", CreateCube(manager)}};
    }
}