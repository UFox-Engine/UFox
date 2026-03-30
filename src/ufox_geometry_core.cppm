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



export module ufox_geometry_core;

import ufox_lib;
import ufox_engine_lib;
import ufox_engine_core;
import ufox_geometry_lib;
import ufox_input;
import ufox_graphic_device;
import ufox_nanoid;

using namespace ufox::engine;

export namespace ufox::geometry {
    class MeshManager;
    const ResourceID* CreateQuad(MeshManager& manager);
    const ResourceID* CreateCube(MeshManager& manager);
    bool LoadMeshFromFile(const std::filesystem::path& sourcePath,std::vector<Vertex>& outVertices,std::vector<uint16_t>& outIndices);
    const ResourceID* CreateMeshContent(MeshManager& manager, const std::span<Vertex>& vertices, const std::span<uint16_t>& indices, const ResourceContextCreateInfo& info);

    using DefaultMeshesResources = std::unordered_map<std::string, const ResourceID*>;

    BuiltInResources MakeBuiltInMeshResources(MeshManager &manager) {
        return {{"quad", CreateQuad(manager)},
                {"cube", CreateCube(manager)}};
    }

    [[nodiscard]] const ResourceID* LoadGLTF(MeshManager& manager,std::span<const std::byte> glbData,std::string_view name);

    class MeshManager final : ResourceManagerBase{
    public:
        MeshManager(const gpu::vulkan::GPUResources& gpu, const std::string_view& directory, const std::span<std::string>& extension) :
        ResourceManagerBase(gpu, directory, extension) {}

        ~MeshManager() override {
            clearAllGpuResources();
        }

        void makeResource(const ResourceContextCreateInfo &info) override {
            std::vector<Vertex> vertices;
            std::vector<uint16_t> indices;

            if (LoadMeshFromFile(info.sourcePath, vertices, indices)) {
                CreateMeshContent(*this, vertices, indices, info);
            }
        }

        void init() override {
            ReadResourceContextMetaData(directory, {sourceExtensions}, this);
            builtInResources = MakeBuiltInMeshResources(*this);
            debug::log(debug::LogLevel::eInfo, "MeshManager: init: success");
        }

        const ResourceID* makeMesh(const std::span<Vertex>& vertices, const std::span<uint16_t>& indices, const ResourceContextCreateInfo& info) {
            const ResourceID& id = makeResourceContext(info.id);
            std::unique_ptr<ResourceBase> res = std::make_unique<Mesh>(info.name, vertices, indices, id);
            debug::log(debug::LogLevel::eInfo, "MeshManager: makeMesh: created mesh: {}", res->name);
            return bindResource(res, info);
        }

        void destroy(const ResourceID& id) {

            if (isBuiltInResource(id)) {
                debug::log(debug::LogLevel::eWarning, "MeshManager: destroy: built-in mesh cannot be destroyed");
                return;
            }

            releaseResource(id);
        }

        [[nodiscard]] Mesh* getMesh(const ResourceID& id) {
            const auto* ctx = getResourceContext(id);
            if (ctx == nullptr || ctx->dataPtr == nullptr) return nullptr;

            auto* mesh = dynamic_cast<Mesh*>(ctx->dataPtr.get());
            return mesh;
        }

        void useMesh(MeshUser& user) {
            if (user.id == nullptr) return;
            auto* ctx = getResourceContext(*user.id);
            if (ctx == nullptr || ctx->dataPtr == nullptr) return;

            auto* mesh = ctx->dataPtr->getContent<Mesh>();

            if (mesh == nullptr) return;

            user.mesh = mesh;
            if (const auto it = std::ranges::find(ctx->users, &user); it == ctx->users.end()) {
                ctx->users.push_back(&user);
            }

            ensureMeshBuffers(user.mesh);
        }

        void unuseMesh(MeshUser& user) {
            if (user.id == nullptr) return;
            user.mesh = nullptr;

            auto* ctx = getResourceContext(*user.id);
            if (ctx == nullptr) return;


            if (const auto it = std::ranges::find(ctx->users, &user); it != ctx->users.end()) {
                ctx->users.erase(it);
            }

            if (!ctx->users.empty() || ctx->dataPtr == nullptr) return;

            if (ctx->dataPtr->hasGpuResources()) {
                ctx->dataPtr->releaseGpuResources();
                debug::log(debug::LogLevel::eInfo, "MeshManager: unuseMesh [{}]: mesh released buffers", ctx->dataPtr->name);
            }
        }

        void clearAllGpuResources() const {
            size_t clearedCount = 0;

            for (const auto &ctx : container | std::views::values) {
                if (!ctx.dataPtr || !ctx.dataPtr->hasGpuResources()) continue;

                ctx.dataPtr->releaseGpuResources();
                ++clearedCount;

                debug::log(debug::LogLevel::eInfo,
                           "MeshManager: clearAllGpuResources: cleared mesh GPU resources for mesh: {}",
                           ctx.dataPtr->name);
            }

            if (clearedCount > 0) {
                debug::log(debug::LogLevel::eInfo,
                           "MeshManager: clearAllGpuResources: cleared {} mesh GPU resources",
                           clearedCount);
            }
        }

    private:
        static void createVertexBuffer(const gpu::vulkan::GPUResources& gpu, Mesh& mesh) {
            gpu::vulkan::MakeAndCopyBuffer(gpu, mesh.vertices, vk::BufferUsageFlagBits::eVertexBuffer, mesh.vertexBuffer);
        }

        static void createIndexBuffer(const gpu::vulkan::GPUResources& gpu, Mesh& mesh) {
            gpu::vulkan::MakeAndCopyBuffer(gpu, mesh.indices, vk::BufferUsageFlagBits::eIndexBuffer, mesh.indexBuffer);
        }

        void ensureMeshBuffers(Mesh* mesh) const {
            if (!mesh) return;
            if (mesh->hasGpuResources()) return;
            createVertexBuffer(*gpuResources, *mesh);
            createIndexBuffer(*gpuResources, *mesh);
        }


        void releaseResource(const ResourceID& id) {
            auto* ctx = getResourceContext(id);
            if (!ctx || !ctx->dataPtr) return;

            // notify & clear users
            for (ResourceUserBase* user : ctx->users) {
                const auto meshUser = dynamic_cast<MeshUser*>(user);
                if (user) unuseMesh(*meshUser);
            }
            ctx->users.clear();

            if (ctx->dataPtr) {
                ctx->dataPtr->releaseGpuResources();
                debug::log(debug::LogLevel::eInfo, "Released buffers for mesh: {}", ctx->dataPtr->name);
                ctx->dataPtr.reset();
            }

            container.erase(id);
        }

    };

    const ResourceID* CreateMeshContent(MeshManager& manager, const std::span<Vertex>& vertices, const std::span<uint16_t>& indices, const ResourceContextCreateInfo& info) {
        const ResourceID* id = manager.makeMesh(vertices, indices, info);
        return id;
    }

    const ResourceID* CreateQuad(MeshManager& manager) {
        ResourceContextCreateInfo info{};
        info.setName(DEFAULT_QUAD_MESH_NAME)
            .setSourceType(SourceType::eBuiltIn)
            .setCategory("Standard")
            .setID(BUILTIN_QUAD_ID);
        return CreateMeshContent(manager, QuadVertices, QuadIndices, info);
    }

    const ResourceID* CreateCube(MeshManager& manager) {
        ResourceContextCreateInfo info{};
        info.setName(DEFAULT_CUBE_MESH_NAME)
            .setSourceType(SourceType::eBuiltIn)
            .setCategory("Standard")
            .setID(BUILTIN_CUBE_ID);
        return CreateMeshContent(manager, CubeVertices, CubeIndices, info);
    }

    void LoadGLTF(std::string_view MODEL_PATH,std::vector<Vertex>& vertices, std::vector<uint16_t>& indices) {

        auto glbBytes = UFoxWindow::ReadFileBinary(MODEL_PATH.data());

        if (glbBytes.empty()){
            debug::log(debug::LogLevel::eError, "Failed to load glTF file: {}", MODEL_PATH.data() );
            return;
        }

        std::span<const std::byte> glbData = glbBytes;

        // ────────────────────────────────────────────────────────────────
        // Step 1: Basic validation of input data size
        // ────────────────────────────────────────────────────────────────
        if (glbData.size() < 20){
            debug::log(debug::LogLevel::eError, "glTF invalid size");
            return;
        }

        // ────────────────────────────────────────────────────────────────
        // Step 2: Validate GLB header (magic, version, total length)
        // ────────────────────────────────────────────────────────────────
        if (glbData.size() < sizeof(gltf::GlbHeader)){
            debug::log(debug::LogLevel::eError, "Data smaller than GLB header");
            return ;
        }

        auto header = reinterpret_cast<const gltf::GlbHeader*>(glbData.data());
        if (header->magic   != gltf::GLB_MAGIC   ||header->version != gltf::GLB_VERSION ||header->length  != glbData.size()){
            debug::log(debug::LogLevel::eError, "Invalid GLB header (magic/version/length mismatch)");
            return;
        }

        glbData = glbData.subspan(sizeof(gltf::GlbHeader));

        // ────────────────────────────────────────────────────────────────
        // Step 3: Extract JSON chunk
        // ────────────────────────────────────────────────────────────────
        if (glbData.size() < sizeof(gltf::GlbChunkHeader)){
            debug::log(debug::LogLevel::eError, "Data too short for JSON chunk header");
            return;
        }

        auto jsonChunk = reinterpret_cast<const gltf::GlbChunkHeader*>(glbData.data());
        if (jsonChunk->chunkType != gltf::CHUNK_TYPE_JSON){
            debug::log(debug::LogLevel::eError, "First chunk is not JSON");
            return;
        }

        std::string_view jsonView{
            reinterpret_cast<const char*>(glbData.data() + sizeof(gltf::GlbChunkHeader)),
            jsonChunk->chunkLength
        };

        glbData = glbData.subspan(sizeof(gltf::GlbChunkHeader) + jsonChunk->chunkLength);

        // ────────────────────────────────────────────────────────────────
        // Step 4: Extract BIN chunk (raw vertex/index data)
        // ────────────────────────────────────────────────────────────────
        if (glbData.size() < sizeof(gltf::GlbChunkHeader)){
            debug::log(debug::LogLevel::eError, "Data too short for BIN chunk header");
            return;
        }

        auto binChunk = reinterpret_cast<const gltf::GlbChunkHeader*>(glbData.data());
        if (binChunk->chunkType != gltf::CHUNK_TYPE_BIN){
            debug::log(debug::LogLevel::eError, "Second chunk is not BIN");
            return;
        }

        std::span binary{
            glbData.data() + sizeof(gltf::GlbChunkHeader),
            binChunk->chunkLength
        };

        // ────────────────────────────────────────────────────────────────
        // Step 5: Parse JSON using nlohmann::json
        // ────────────────────────────────────────────────────────────────
        nlohmann::json gltfJson;
        try{
            gltfJson = nlohmann::json::parse(jsonView);
        }
        catch (const nlohmann::json::parse_error& e){
            debug::log(debug::LogLevel::eError,"JSON parse error at byte {}: {}", e.byte, e.what());
            return ;
        }

        // ────────────────────────────────────────────────────────────────
        // Step 6: Navigate to first mesh → first primitive → attributes
        //         Extract POSITION and optional indices accessor indices
        // ────────────────────────────────────────────────────────────────
        uint32_t positionAccessorIndex = UINT32_MAX;
        std::optional<uint32_t> optIndicesAccessor;

        try{
            const auto& meshes = gltfJson.at("meshes");
            if (!meshes.is_array() || meshes.empty()){
                debug::log(debug::LogLevel::eWarning, "No meshes array or empty");
                return;
            }

            const auto& firstMesh = meshes[0];
            const auto& primitives = firstMesh.at("primitives");
            if (!primitives.is_array() || primitives.empty()){
                debug::log(debug::LogLevel::eWarning, "No primitives in first mesh");
                return;
            }

            const auto& firstPrim = primitives[0];
            const auto& attributes = firstPrim.at("attributes");

            if (!attributes.contains("POSITION")){
                debug::log(debug::LogLevel::eWarning, "First primitive has no POSITION attribute");
                return;
            }

            positionAccessorIndex = attributes["POSITION"].get<uint32_t>();

            if (firstPrim.contains("indices")){
                optIndicesAccessor = firstPrim["indices"].get<uint32_t>();
            }
        }
        catch (const std::exception& e){
            debug::log(debug::LogLevel::eError, "Failed to read mesh/primitive/attributes: {}", e.what());
            return;
        }

        // ────────────────────────────────────────────────────────────────
        // Step 7: Load POSITION attribute (vertices)
        // ────────────────────────────────────────────────────────────────
        if (!gltfJson.contains("accessors") ||!gltfJson["accessors"].is_array() ||positionAccessorIndex >= gltfJson["accessors"].size()){
            debug::log(debug::LogLevel::eError, "Invalid or missing accessors array for POSITION");
            return;
        }

        const auto& posAccessor = gltfJson["accessors"][positionAccessorIndex];

        // Required fields check
        if (!posAccessor.contains("bufferView") ||!posAccessor.contains("count") ||!posAccessor.contains("type") ||!posAccessor.contains("componentType")){
            debug::log(debug::LogLevel::eError, "POSITION accessor missing required fields");
            return;
        }

        uint32_t bufferViewIdx      = posAccessor["bufferView"].get<uint32_t>();
        uint32_t accessorByteOffset = posAccessor.value("byteOffset", 0u);
        uint32_t count              = posAccessor["count"].get<uint32_t>();
        std::string typeStr         = posAccessor["type"].get<std::string>();
        uint32_t compType           = posAccessor["componentType"].get<uint32_t>();

        // Only support FLOAT VEC3 right now
        if (typeStr != "VEC3" || compType != 5126){
            debug::log(debug::LogLevel::eError,"Unsupported POSITION format: type={}, componentType={}", typeStr, compType);
            return;
        }

        if (count == 0 || count > 1'000'000){
            debug::log(debug::LogLevel::eError, "Invalid POSITION count: {}", count);
            return;
        }

        // ─── Get bufferView info ───────────────────────────────────────
        if (!gltfJson.contains("bufferViews") ||bufferViewIdx >= gltfJson["bufferViews"].size()){
            debug::log(debug::LogLevel::eError, "Invalid bufferView index: {}", bufferViewIdx);
            return;
        }

        const auto& bufferView = gltfJson["bufferViews"][bufferViewIdx];

        uint32_t viewByteOffset = bufferView.value("byteOffset", 0u);
        uint32_t byteStride     = bufferView.value("byteStride", 12u); // default packed vec3

        if (byteStride != 12){
            debug::log(debug::LogLevel::eWarning,"Non-packed stride {} for POSITION — assuming interleaved (may need adjustment)",byteStride);
        }

        // Final memory range check
        size_t finalStart = viewByteOffset + accessorByteOffset;
        size_t finalSize  = count * byteStride;

        if (finalStart + finalSize > binary.size()){
            debug::log(debug::LogLevel::eError,"POSITION data out of binary bounds (start={}, size={})", finalStart, finalSize);
            return;
        }

        // ─── Copy real positions into vertices ─────────────────────────
        vertices.reserve(count);

        const std::byte* srcData = binary.data() + finalStart;

        for (uint32_t i = 0; i < count; ++i){
            const auto* posPtr = reinterpret_cast<const glm::vec3*>(srcData + i * byteStride);

            Vertex v{};
            v.pos      = *posPtr;
            v.color    = {1.0f, 1.0f, 1.0f};  // white default
            v.texCoord = {0.0f, 0.0f};        // zero default
            vertices.push_back(v);
        }

        // ────────────────────────────────────────────────────────────────
        // Step 8: Load indices (if present)
        // ────────────────────────────────────────────────────────────────

        if (optIndicesAccessor){
            uint32_t idxAccessorIdx = *optIndicesAccessor;

            if (!gltfJson.contains("accessors") ||idxAccessorIdx >= gltfJson["accessors"].size()){
                debug::log(debug::LogLevel::eWarning, "Invalid indices accessor index: {}", idxAccessorIdx);
            }
            else{
                const auto& idxAccessor = gltfJson["accessors"][idxAccessorIdx];

                if (!idxAccessor.contains("bufferView") ||!idxAccessor.contains("count") ||!idxAccessor.contains("componentType")){
                    debug::log(debug::LogLevel::eWarning, "Indices accessor missing required fields");
                }else{
                    uint32_t idxBufferView  = idxAccessor["bufferView"].get<uint32_t>();
                    uint32_t idxByteOffset  = idxAccessor.value("byteOffset", 0u);
                    uint32_t idxCount       = idxAccessor["count"].get<uint32_t>();
                    uint32_t idxCompType    = idxAccessor["componentType"].get<uint32_t>();

                    // Only UNSIGNED_SHORT (5123) supported for now
                    if (idxCompType != 5123){
                        debug::log(debug::LogLevel::eWarning,"Unsupported indices componentType: {} (only UNSIGNED_SHORT supported)",idxCompType);
                    }
                    else if (idxCount == 0 || idxCount > 100'000){
                        debug::log(debug::LogLevel::eError, "Invalid indices count: {}", idxCount);
                    }
                    else if (!gltfJson.contains("bufferViews") ||idxBufferView >= gltfJson["bufferViews"].size()){
                        debug::log(debug::LogLevel::eError, "Invalid indices bufferView: {}", idxBufferView);
                    }else{
                        const auto& idxView = gltfJson["bufferViews"][idxBufferView];
                        uint32_t viewOffset = idxView.value("byteOffset", 0u);

                        size_t finalIdxStart = viewOffset + idxByteOffset;
                        size_t finalIdxSize  = idxCount * sizeof(uint16_t);

                        if (finalIdxStart + finalIdxSize > binary.size()){
                            debug::log(debug::LogLevel::eError, "Indices data out of bounds");
                        }
                        else{
                            indices.resize(idxCount);
                            const auto* srcIdx = reinterpret_cast<const uint16_t*>(
                                binary.data() + finalIdxStart);

                            std::copy_n(srcIdx, idxCount, indices.begin());
                        }
                    }
                }
            }
        }else{
            debug::log(debug::LogLevel::eInfo, "No indices found — mesh will be rendered without indexing");
        }

        debug::log(debug::LogLevel::eInfo, "Loaded glTF ({}) : success", MODEL_PATH.data() );
    }

    void LoadOBJ(std::string_view MODEL_PATH, std::vector<Vertex>& vertices, std::vector<uint16_t>& indices) {
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

    bool LoadMeshFromFile(const std::filesystem::path& sourcePath,std::vector<Vertex>& outVertices,std::vector<uint16_t>& outIndices){
        if (!std::filesystem::exists(sourcePath))
        {
            debug::log(debug::LogLevel::eWarning,
                       "Mesh source file missing: {}", sourcePath.string());
            return false;
        }

        const auto ext = sourcePath.extension().string();

        if (ext == ".obj"){
            try{
                LoadOBJ(sourcePath.string(), outVertices, outIndices);
                return true;
            }
            catch (const std::exception& e){
                debug::log(debug::LogLevel::eError,
                           "Failed to load OBJ {} : {}", sourcePath.string(), e.what());
                return false;
            }
        }
        if (ext == ".glb" || ext == ".gltf") {
          try {
            LoadGLTF(sourcePath.string(), outVertices, outIndices);
            return true;
          } catch (const std::exception &e) {
            debug::log(debug::LogLevel::eError, "Failed to load GLTF {} : {}",
                       sourcePath.string(), e.what());
            return false;
          }
        }

        debug::log(debug::LogLevel::eWarning,
                   "Unsupported mesh format: {}", ext);
        return false;
    }
}