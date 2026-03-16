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
#include <filesystem>
#include <vector>
#include <nlohmann/json.hpp>
#include <tiny_obj_loader.h>
#include <string>
#include <unordered_map>

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



        static void createVertexBuffer(const gpu::vulkan::GPUResources& gpu, Mesh& mesh) {
            gpu::vulkan::MakeAndCopyBuffer(gpu, mesh.vertices, vk::BufferUsageFlagBits::eVertexBuffer, mesh.vertexBuffer);
        }

        static void createIndexBuffer(const gpu::vulkan::GPUResources& gpu, Mesh& mesh) {
            gpu::vulkan::MakeAndCopyBuffer(gpu, mesh.indices, vk::BufferUsageFlagBits::eIndexBuffer, mesh.indexBuffer);
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

    // const ContentID* CreatePortInMesh(MeshManager& manager,std::string_view displayName,const std::filesystem::path& sourceFilePath,std::string_view category = "Imported")
    // {
    //     if (!std::filesystem::exists(sourceFilePath)) {
    //         debug::log(debug::LogLevel::eError, "Source mesh not found: {}", sourceFilePath.string());
    //         return nullptr;
    //     }
    //
    //     std::string slotName = std::string(displayName);
    //     if (slotName.empty()) {
    //         slotName = sourceFilePath.stem().string();
    //     }
    //
    //     ContentIndex idx = manager.acquireSlot(slotName);
    //     auto& slot = manager.slots[idx];
    //
    //     const auto contentRoot = paths::GetMeshContentRoot();
    //     std::filesystem::create_directories(contentRoot);
    //
    //     slot.setCacheFilenames(contentRoot);
    //
    //     slot.name           = slotName;
    //     slot.sourcePath     = sourceFilePath;
    //     slot.lastImportTime = std::chrono::file_clock::now();
    //     // slot.sourceHash     = ComputeFileHash(sourceFilePath);   // ← to be added
    //
    //     slot.mesh = std::make_unique<Mesh>(
    //         displayName.empty() ? slotName : displayName,
    //         ContentSourceType::ePortIn,
    //         category,
    //         ContentID{idx, slot.version}
    //     );
    //
    //     // Save initial metadata even before geometry import
    //     SaveSlotMetadataToJson(slot, slot.metaDataPath);
    //
    //     debug::log(debug::LogLevel::eInfo,
    //                "Created port-in mesh slot: \"{}\" from \"{}\"",
    //                slot.name, sourceFilePath.string());
    //
    //     return &slot.mesh->cid;
    // }

    // Loads the FIRST primitive of the FIRST mesh from a .glb file.
    // Currently supports:
    //   - POSITION attribute (required)
    //   - indices (optional, UNSIGNED_SHORT only)
    // Returns ContentID* on success, nullptr on any failure.
    [[nodiscard]] const engine::ContentID* CreateMeshFromFirstGlbPrimitive(MeshManager& manager,std::span<const std::byte> glbData,std::string_view name = "gltf_mesh") {
        // ────────────────────────────────────────────────────────────────
        // Step 1: Basic validation of input data size
        // ────────────────────────────────────────────────────────────────
        if (glbData.size() < 20){
            debug::log(debug::LogLevel::eError, "glTF invalid size");
            return nullptr;
        }

        // ────────────────────────────────────────────────────────────────
        // Step 2: Validate GLB header (magic, version, total length)
        // ────────────────────────────────────────────────────────────────
        if (glbData.size() < sizeof(gltf::GlbHeader))
        {
            debug::log(debug::LogLevel::eError, "Data smaller than GLB header");
            return nullptr;
        }

        auto header = reinterpret_cast<const gltf::GlbHeader*>(glbData.data());
        if (header->magic   != gltf::GLB_MAGIC   ||
            header->version != gltf::GLB_VERSION ||
            header->length  != glbData.size())
        {
            debug::log(debug::LogLevel::eError, "Invalid GLB header (magic/version/length mismatch)");
            return nullptr;
        }

        glbData = glbData.subspan(sizeof(gltf::GlbHeader));

        // ────────────────────────────────────────────────────────────────
        // Step 3: Extract JSON chunk
        // ────────────────────────────────────────────────────────────────
        if (glbData.size() < sizeof(gltf::GlbChunkHeader))
        {
            debug::log(debug::LogLevel::eError, "Data too short for JSON chunk header");
            return nullptr;
        }

        auto jsonChunk = reinterpret_cast<const gltf::GlbChunkHeader*>(glbData.data());
        if (jsonChunk->chunkType != gltf::CHUNK_TYPE_JSON)
        {
            debug::log(debug::LogLevel::eError, "First chunk is not JSON");
            return nullptr;
        }

        std::string_view jsonView{
            reinterpret_cast<const char*>(glbData.data() + sizeof(gltf::GlbChunkHeader)),
            jsonChunk->chunkLength
        };

        glbData = glbData.subspan(sizeof(gltf::GlbChunkHeader) + jsonChunk->chunkLength);

        // ────────────────────────────────────────────────────────────────
        // Step 4: Extract BIN chunk (raw vertex/index data)
        // ────────────────────────────────────────────────────────────────
        if (glbData.size() < sizeof(gltf::GlbChunkHeader))
        {
            debug::log(debug::LogLevel::eError, "Data too short for BIN chunk header");
            return nullptr;
        }

        auto binChunk = reinterpret_cast<const gltf::GlbChunkHeader*>(glbData.data());
        if (binChunk->chunkType != gltf::CHUNK_TYPE_BIN)
        {
            debug::log(debug::LogLevel::eError, "Second chunk is not BIN");
            return nullptr;
        }

        std::span<const std::byte> binary{
            glbData.data() + sizeof(gltf::GlbChunkHeader),
            binChunk->chunkLength
        };

        // ────────────────────────────────────────────────────────────────
        // Step 5: Parse JSON using nlohmann::json
        // ────────────────────────────────────────────────────────────────
        nlohmann::json gltfJson;
        try
        {
            gltfJson = nlohmann::json::parse(jsonView);
        }
        catch (const nlohmann::json::parse_error& e)
        {
            debug::log(debug::LogLevel::eError,
                       "JSON parse error at byte {}: {}", e.byte, e.what());
            return nullptr;
        }

        // ────────────────────────────────────────────────────────────────
        // Step 6: Navigate to first mesh → first primitive → attributes
        //         Extract POSITION and optional indices accessor indices
        // ────────────────────────────────────────────────────────────────
        uint32_t positionAccessorIndex = UINT32_MAX;
        std::optional<uint32_t> optIndicesAccessor;

        try
        {
            const auto& meshes = gltfJson.at("meshes");
            if (!meshes.is_array() || meshes.empty())
            {
                debug::log(debug::LogLevel::eWarning, "No meshes array or empty");
                return nullptr;
            }

            const auto& firstMesh = meshes[0];
            const auto& primitives = firstMesh.at("primitives");
            if (!primitives.is_array() || primitives.empty())
            {
                debug::log(debug::LogLevel::eWarning, "No primitives in first mesh");
                return nullptr;
            }

            const auto& firstPrim = primitives[0];
            const auto& attributes = firstPrim.at("attributes");

            if (!attributes.contains("POSITION"))
            {
                debug::log(debug::LogLevel::eWarning, "First primitive has no POSITION attribute");
                return nullptr;
            }

            positionAccessorIndex = attributes["POSITION"].get<uint32_t>();
            debug::log(debug::LogLevel::eInfo, "POSITION accessor index = {}", positionAccessorIndex);

            if (firstPrim.contains("indices"))
            {
                optIndicesAccessor = firstPrim["indices"].get<uint32_t>();
                debug::log(debug::LogLevel::eInfo, "Indices accessor index = {}", *optIndicesAccessor);
            }
        }
        catch (const std::exception& e)
        {
            debug::log(debug::LogLevel::eError, "Failed to read mesh/primitive/attributes: {}", e.what());
            return nullptr;
        }

        // ────────────────────────────────────────────────────────────────
        // Step 7: Load POSITION attribute (vertices)
        // ────────────────────────────────────────────────────────────────
        if (!gltfJson.contains("accessors") ||
            !gltfJson["accessors"].is_array() ||
            positionAccessorIndex >= gltfJson["accessors"].size())
        {
            debug::log(debug::LogLevel::eError, "Invalid or missing accessors array for POSITION");
            return nullptr;
        }

        const auto& posAccessor = gltfJson["accessors"][positionAccessorIndex];

        // Required fields check
        if (!posAccessor.contains("bufferView") ||
            !posAccessor.contains("count") ||
            !posAccessor.contains("type") ||
            !posAccessor.contains("componentType"))
        {
            debug::log(debug::LogLevel::eError, "POSITION accessor missing required fields");
            return nullptr;
        }

        uint32_t bufferViewIdx      = posAccessor["bufferView"].get<uint32_t>();
        uint32_t accessorByteOffset = posAccessor.value("byteOffset", 0u);
        uint32_t count              = posAccessor["count"].get<uint32_t>();
        std::string typeStr         = posAccessor["type"].get<std::string>();
        uint32_t compType           = posAccessor["componentType"].get<uint32_t>();

        // Only support FLOAT VEC3 right now
        if (typeStr != "VEC3" || compType != 5126)
        {
            debug::log(debug::LogLevel::eError,
                       "Unsupported POSITION format: type={}, componentType={}", typeStr, compType);
            return nullptr;
        }

        if (count == 0 || count > 1'000'000)
        {
            debug::log(debug::LogLevel::eError, "Invalid POSITION count: {}", count);
            return nullptr;
        }

        // ─── Get bufferView info ───────────────────────────────────────
        if (!gltfJson.contains("bufferViews") ||
            bufferViewIdx >= gltfJson["bufferViews"].size())
        {
            debug::log(debug::LogLevel::eError, "Invalid bufferView index: {}", bufferViewIdx);
            return nullptr;
        }

        const auto& bufferView = gltfJson["bufferViews"][bufferViewIdx];

        uint32_t viewByteOffset = bufferView.value("byteOffset", 0u);
        uint32_t byteStride     = bufferView.value("byteStride", 12u); // default packed vec3

        if (byteStride != 12)
        {
            debug::log(debug::LogLevel::eWarning,
                       "Non-packed stride {} for POSITION — assuming interleaved (may need adjustment)",
                       byteStride);
        }

        // Final memory range check
        size_t finalStart = viewByteOffset + accessorByteOffset;
        size_t finalSize  = count * byteStride;

        if (finalStart + finalSize > binary.size())
        {
            debug::log(debug::LogLevel::eError,
                       "POSITION data out of binary bounds (start={}, size={})", finalStart, finalSize);
            return nullptr;
        }

        // ─── Copy real positions into vertices ─────────────────────────
        std::vector<Vertex> vertices;
        vertices.reserve(count);

        const std::byte* srcData = binary.data() + finalStart;

        for (uint32_t i = 0; i < count; ++i)
        {
            const glm::vec3* posPtr = reinterpret_cast<const glm::vec3*>(
                srcData + i * byteStride);

            Vertex v{};
            v.pos      = *posPtr;
            v.color    = {1.0f, 1.0f, 1.0f};  // white default
            v.texCoord = {0.0f, 0.0f};        // zero default
            vertices.push_back(v);
        }

        debug::log(debug::LogLevel::eInfo, "Successfully loaded {} real vertex positions", count);

        // ────────────────────────────────────────────────────────────────
        // Step 8: Load indices (if present)
        // ────────────────────────────────────────────────────────────────
        std::vector<uint16_t> indices;

        if (optIndicesAccessor)
        {
            uint32_t idxAccessorIdx = *optIndicesAccessor;

            if (!gltfJson.contains("accessors") ||
                idxAccessorIdx >= gltfJson["accessors"].size())
            {
                debug::log(debug::LogLevel::eWarning, "Invalid indices accessor index: {}", idxAccessorIdx);
            }
            else
            {
                const auto& idxAccessor = gltfJson["accessors"][idxAccessorIdx];

                if (!idxAccessor.contains("bufferView") ||
                    !idxAccessor.contains("count") ||
                    !idxAccessor.contains("componentType"))
                {
                    debug::log(debug::LogLevel::eWarning, "Indices accessor missing required fields");
                }
                else
                {
                    uint32_t idxBufferView  = idxAccessor["bufferView"].get<uint32_t>();
                    uint32_t idxByteOffset  = idxAccessor.value("byteOffset", 0u);
                    uint32_t idxCount       = idxAccessor["count"].get<uint32_t>();
                    uint32_t idxCompType    = idxAccessor["componentType"].get<uint32_t>();

                    // Only UNSIGNED_SHORT (5123) supported for now
                    if (idxCompType != 5123)
                    {
                        debug::log(debug::LogLevel::eWarning,
                                   "Unsupported indices componentType: {} (only UNSIGNED_SHORT supported)",
                                   idxCompType);
                    }
                    else if (idxCount == 0 || idxCount > 100'000)
                    {
                        debug::log(debug::LogLevel::eError, "Invalid indices count: {}", idxCount);
                    }
                    else if (!gltfJson.contains("bufferViews") ||
                             idxBufferView >= gltfJson["bufferViews"].size())
                    {
                        debug::log(debug::LogLevel::eError, "Invalid indices bufferView: {}", idxBufferView);
                    }
                    else
                    {
                        const auto& idxView = gltfJson["bufferViews"][idxBufferView];
                        uint32_t viewOffset = idxView.value("byteOffset", 0u);

                        size_t finalIdxStart = viewOffset + idxByteOffset;
                        size_t finalIdxSize  = idxCount * sizeof(uint16_t);

                        if (finalIdxStart + finalIdxSize > binary.size())
                        {
                            debug::log(debug::LogLevel::eError, "Indices data out of bounds");
                        }
                        else
                        {
                            indices.resize(idxCount);
                            const uint16_t* srcIdx = reinterpret_cast<const uint16_t*>(
                                binary.data() + finalIdxStart);

                            std::copy(srcIdx, srcIdx + idxCount, indices.begin());

                            debug::log(debug::LogLevel::eInfo,
                                       "Loaded {} real indices (uint16)", idxCount);
                        }
                    }
                }
            }
        }
        else
        {
            debug::log(debug::LogLevel::eInfo, "No indices found — mesh will be rendered without indexing");
        }

        // ────────────────────────────────────────────────────────────────
        // Step 9: Create final mesh content
        // ────────────────────────────────────────────────────────────────
        return CreateMeshContent(
            manager,
            name,
            std::span(vertices),
            std::span(indices),
            engine::ContentSourceType::ePortIn,
            "glTF import"
        );
    }

void LoadOBJ(std::string_view MODEL_PATH,
         std::vector<Vertex>& vertices,
         std::vector<uint16_t>& indices) {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string  err;

        if (!LoadObj(&attrib, &shapes, &materials, &err, MODEL_PATH.data()))
        {
            throw std::runtime_error(err);
        }

        vertices.clear();
        indices.clear();

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        for (const auto &shape : shapes)
        {
            for (const auto &index : shape.mesh.indices)
            {
                Vertex vertex{};

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]};

                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};

                vertex.color = {1.0f, 1.0f, 1.0f};

                if (!uniqueVertices.contains(vertex))
                {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
        }
    }
}