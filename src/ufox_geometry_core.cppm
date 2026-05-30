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
#include <limits>


export module ufox_geometry_core;

import ufox_lib;
import ufox_engine_lib;
import ufox_engine_core;
import ufox_geometry_lib;
import ufox_input;
import ufox_gpu_lib;
import ufox_gpu_core;
import ufox_nanoid;

using namespace ufox::engine;

export namespace ufox::geometry {
    class MeshManager;
    const ResourceID* MakeQuad(MeshManager& manager);
    const ResourceID* MakeCube(MeshManager& manager);
    const ResourceID* MakeMeshContent(MeshManager& manager, const std::span<Vertex>& vertices, const std::span<uint16_t>& indices, const ResourceContextCreateInfo& info);

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

    BuiltInResources MakeBuiltInMeshResources(MeshManager &manager) {
        return {{QUAD, MakeQuad(manager)},
                {CUBE, MakeCube(manager)}};
    }

    [[nodiscard]] const ResourceID* LoadGLTF(MeshManager& manager,std::span<const std::byte> glbData,std::string_view name);

    void MakeVertexBuffer(const gpu::GPUResources& gpu, Mesh& mesh) {
        gpu::MakeAndCopyBuffer(gpu, mesh.vertices, vk::BufferUsageFlagBits::eVertexBuffer, mesh.vertexBuffer);
    }

    void MakeIndexBuffer(const gpu::GPUResources& gpu, Mesh& mesh) {
        gpu::MakeAndCopyBuffer(gpu, mesh.indices, vk::BufferUsageFlagBits::eIndexBuffer, mesh.indexBuffer);
    }

    class MeshManager final : ResourceManagerBase {
    public:
        explicit MeshManager(UFoxWindow& _window) :
        ResourceManagerBase(_window, MESH_RESOURCE_PATH, MESH_RESOURCE_EXTENSION) {
            window->registerCallbackEvent<EventType::eSystemInit>([](void* user){static_cast<MeshManager*>(user)->onSystemInit(); }, this);
            window->registerCallbackEvent<EventType::eGainsFocus>([](void* user){static_cast<MeshManager*>(user)->onGainsFocus(); }, this);
        }

        ~MeshManager() override {
            window->unregisterCallbackEvent(EventType::eSystemInit,this);
            window->unregisterCallbackEvent(EventType::eGainsFocus,this);
            clearAllGpuResources(*this);
        }

        void onMakeResource(ResourceContextCreateInfo &info) override {
            std::vector<Vertex> vertices;
            std::vector<uint16_t> indices;

            if (LoadMeshFromFile(info.sourcePath, vertices, indices)) {
                MakeMeshContent(*this, vertices, indices, info);
            }
        }

        void onSystemInit() override {
            ReadResourceContextMetaData(directory, sourceExtensions, this);
            builtInResources = MakeBuiltInMeshResources(*this);
            debug::log(debug::LogLevel::eInfo, "MeshManager: init: success");
        }

        void onPostSystemInit(const float &width, const float &height) override{}

        void onGainsFocus() override {
            refreshResource();
        }

        void onPreDraw() override {}

        const ResourceID* makeMesh(const std::span<Vertex>& vertices, const std::span<uint16_t>& indices, const ResourceContextCreateInfo& info) {
            ResourceID id = ResolveResourceID(info);
            if(!makeResourceContext(id, info.overwrite)) return nullptr;

            std::unique_ptr<ResourceBase> res = std::make_unique<Mesh>(info.name, vertices, indices, id);
            auto* mesh = dynamic_cast<Mesh*>(res.get());

            MakeVertexBuffer(window->gpuResource, *mesh);
            MakeIndexBuffer(window->gpuResource, *mesh);
            return bindResourceToContext(res, info);
        }


        [[nodiscard]] Mesh* getMesh(const ResourceID& id) {
            const auto* ctx = getResourceContext(id);
            if (ctx == nullptr || ctx->dataPtr == nullptr) return nullptr;

            auto* mesh = dynamic_cast<Mesh*>(ctx->dataPtr.get());
            return mesh;
        }
    };

    const ResourceID* MakeMeshContent(MeshManager& manager, const std::span<Vertex>& vertices, const std::span<uint16_t>& indices, const ResourceContextCreateInfo& info) {
        return manager.makeMesh(vertices, indices, info);
    }

    const ResourceID* MakeQuad(MeshManager& manager) {
        ResourceContextCreateInfo info{};
        info.setName(DEFAULT_QUAD_MESH_NAME)
            .setSourceType(SourceType::eBuiltIn)
            .setCategory("Standard")
            .setID(BUILTIN_QUAD_ID);
        return MakeMeshContent(manager, QuadVertices, QuadIndices, info);
    }

    const ResourceID* MakeCube(MeshManager& manager) {
        ResourceContextCreateInfo info{};
        info.setName(DEFAULT_CUBE_MESH_NAME)
            .setSourceType(SourceType::eBuiltIn)
            .setCategory("Standard")
            .setID(BUILTIN_CUBE_ID);
        return MakeMeshContent(manager, CubeVertices, CubeIndices, info);
    }

    [[nodiscard]] constexpr float ChooseGreaterDistance(const float& a, const float& b) noexcept {
        return std::max(a,b);
    }

    [[nodiscard]] constexpr float ChooseGreaterDistance(const float& a, const float& b, const float& c) noexcept {
        return std::max({a,b,c});
    }

    [[nodiscard]] constexpr float ChooseLowestDistance(const float& a, const float& b) noexcept {
        return std::min({a,b});
    }

    [[nodiscard]] constexpr float ChooseLowestDistance(const float& a, const float& b, const float& c) noexcept {
        return std::min({a,b,c});
    }
}

export namespace ufox::geometry::discadelta {

    [[nodiscard]] constexpr float CalculateJustifyContent(const float currentOffset, const float parentSize, const float size, const float usedSpace, const float remainSpace, const int sectionSize
                , const bool isFirst, const bool isLast, JustifyContentMode mode) {

        switch (mode) {
        case JustifyContentMode::eStart:
            return currentOffset;
        case JustifyContentMode::eEnd: {
            if (isFirst) {
                return currentOffset + parentSize - usedSpace;
            }
            return currentOffset;
        }
        case JustifyContentMode::eCenter: {
            if (isFirst) {
                return currentOffset + (parentSize - usedSpace) / 2.0f;
            }
            return currentOffset;
        }
        case JustifyContentMode::eSpaceBetween: {
            const float space = remainSpace / static_cast<float>(sectionSize - 1);
            if (isLast) {
                return parentSize - size;
            }
            if (isFirst) {
                return currentOffset;
            }
            return currentOffset + space;
        }
        case JustifyContentMode::eSpaceAround: {
            const float space = remainSpace / static_cast<float>(sectionSize);
            if (isLast) {
                return parentSize - size - space/2;
            }
            if (isFirst) {
                return currentOffset + space/2;
            }
            return currentOffset + space;
        }
        case JustifyContentMode::eSpaceEvenly: {
            const float space = remainSpace / static_cast<float>(sectionSize+1);
            return currentOffset + space;
        }

        default: return 0.0f;
        }
    }


    [[nodiscard]] constexpr float Scaler(float distance, float accumulateFactor, float factor) noexcept {
        if (distance <= 0.0f || accumulateFactor <= 0.0f || factor <= 0.0f) {
            return 0.0f;
        }
        return distance <= 0.0f || accumulateFactor <= 0.0f || factor <= 0.0f ? 0.0f : distance / accumulateFactor * factor;
    }

    template<typename Derived>
    class DiscadeltaRectLayoutBase {


        virtual void onAdd(Derived* added) =0;
        virtual void onRemove(Derived* removed) =0;
        virtual void onTreeStructureChanged() =0;

    protected:
        explicit DiscadeltaRectLayoutBase(const ResourceID &_id) {
            id = _id.IsValid() ? _id : ResourceID{nanoid::generate(12)};
        }

        DiscadeltaRectLayoutBase() : id(nanoid::generate(12)){}

        void clear() {
            if (parent) {
                parent->resourceIndex.erase(this->id);
                parent = nullptr;
            }
            container.clear();
            resourceIndex.clear();
        }

        void preComputeMetrics() noexcept {
            accumulatedWidthBase = 0.0f;
            accumulatedHeightBase = 0.0f;
            accumulatedWidthMin = 0.0f;
            accumulatedHeightMin = 0.0f;
            accumulatedExpandRatio = 0.0f;
            accumulatedCompressSolidify = 0.0f;

            if (container.empty()) return;

            for (const auto& entry : container) {
                DiscadeltaRectLayoutBase* child = getBaseChild(entry);
                if (child == nullptr) continue;

                float compressSolidify{0.0f};

                if (baseStyle->direction == FlexDirection::eRow) {
                    accumulatedWidthBase += child->validatedWidthBase;
                    accumulatedHeightBase = ChooseGreaterDistance(accumulatedHeightBase, child->validatedHeightBase);
                    compressSolidify = child->widthCompressSolidify;
                    const float& childWidthMin = ChooseGreaterDistance(child->validatedWidthMin, compressSolidify);
                    const float& childHeightMin = ChooseGreaterDistance(child->validatedHeightMin, child->accumulatedHeightMin);
                    accumulatedWidthMin += ChooseGreaterDistance(childWidthMin, child->accumulatedWidthMin);
                    accumulatedHeightMin = ChooseGreaterDistance(accumulatedHeightMin, childHeightMin);
                }
                else {
                    accumulatedWidthBase = ChooseGreaterDistance(accumulatedWidthBase, child->validatedWidthBase);
                    accumulatedHeightBase += child->validatedHeightBase;
                    compressSolidify = child->heightCompressSolidify;
                    const float& childHeightMin = ChooseGreaterDistance(child->validatedHeightMin, compressSolidify);
                    const float& childWidthMin = ChooseGreaterDistance(child->validatedWidthMin, child->accumulatedWidthMin);
                    accumulatedWidthMin = ChooseGreaterDistance(accumulatedWidthMin, childWidthMin);
                    accumulatedHeightMin += ChooseGreaterDistance(child->accumulatedHeightMin, childHeightMin);
                }

                accumulatedCompressSolidify += compressSolidify;
                accumulatedExpandRatio += child->expandRatio;
            }

        }

        void validateMetrics() noexcept {
            validatedWidthMin = baseStyle->minWidth.mode == LengthMode::eAuto ? accumulatedWidthMin : ChooseGreaterDistance(0.0f, baseStyle->minWidth.value);
            validatedWidthMax = baseStyle->maxWidth.mode == LengthMode::eAuto ? std::numeric_limits<float>::max(): ChooseGreaterDistance(baseStyle->maxWidth.value, validatedWidthMin);
            validatedHeightMin = baseStyle->minHeight.mode == LengthMode::eAuto ? accumulatedHeightMin : ChooseGreaterDistance(0.0f, baseStyle->minHeight.value);
            validatedHeightMax = baseStyle->maxHeight.mode == LengthMode::eAuto ? std::numeric_limits<float>::max(): ChooseGreaterDistance(baseStyle->maxHeight.value, validatedHeightMin);
            validatedWidthBase = baseStyle->width.mode == LengthMode::eAuto ? accumulatedWidthBase : baseStyle->width.value;
            validatedHeightBase = baseStyle->height.mode == LengthMode::eAuto ? accumulatedHeightBase : baseStyle->height.value;
            // pre clamp temporary off
            // validatedWidthBase = std::clamp(baseW, validatedWidthMin, validatedWidthMax);
            // validatedHeightBase = std::clamp(baseH, validatedHeightMin, validatedHeightMax);
            compressRatio = ChooseGreaterDistance(0.0f,baseStyle->flexCompress);
            widthCompressCapacity = validatedWidthBase * compressRatio;
            widthCompressSolidify = ChooseGreaterDistance(0.0f, validatedWidthBase  - widthCompressCapacity);
            heightCompressCapacity = validatedHeightBase * compressRatio;
            heightCompressSolidify = ChooseGreaterDistance(0.0f, validatedHeightBase - heightCompressCapacity);
            expandRatio = ChooseGreaterDistance(0.0f, baseStyle->flexExpand);
            order = ChooseGreaterDistance(0, baseStyle->order);
        }

        void updatePriorityLists() noexcept {
            compressCascadePriorities.clear();
            expandCascadePriorities.clear();

            if (container.empty()) {
                return;
            }

            const size_t childCount = container.size();
            compressCascadePriorities.reserve(childCount);
            expandCascadePriorities.reserve(childCount);

            std::vector<std::pair<float, size_t>> compressPriorities;
            std::vector<std::pair<float, size_t>> expandPriorities;
            compressPriorities.reserve(childCount);
            expandPriorities.reserve(childCount);

            for (size_t i = 0; i < container.size(); ++i) {
                DiscadeltaRectLayoutBase* child = getBaseChild(container[i]);
                if (!child) continue;
                if (child->baseStyle->positionMode == PositionMode::eAbsolute) continue;

                float compressRoom{0.0f};
                float expandRoom{0.0f};

                if (baseStyle->direction == FlexDirection::eRow) {
                    compressRoom = ChooseGreaterDistance(0.0f, child->validatedWidthBase - child->validatedWidthMin);
                    expandRoom   = ChooseGreaterDistance(0.0f, child->validatedWidthMax  - child->validatedWidthBase);
                } else {
                    compressRoom = ChooseGreaterDistance(0.0f, child->validatedHeightBase - child->validatedHeightMin);
                    expandRoom   = ChooseGreaterDistance(0.0f, child->validatedHeightMax  - child->validatedHeightBase);
                }

                compressPriorities.emplace_back(compressRoom, i);
                expandPriorities.emplace_back(expandRoom,   i);
            }

            std::ranges::sort(compressPriorities, [](const auto& a, const auto& b) {
                return a.first < b.first;
            });
            for (const auto &val: compressPriorities | std::views::values) {
                compressCascadePriorities.push_back(val);
            }

            std::ranges::sort(expandPriorities, [](const auto& a, const auto& b) {
                return a.first > b.first;
            });
            for (const auto &val: expandPriorities | std::views::values) {
                expandCascadePriorities.push_back(val);
            }
        }



        [[nodiscard]] std::tuple<float, float, bool, bool> makeSizeMetrics(const float& targetWidth, const float& targetHeight, const bool& round) const noexcept {
            const bool isRow = baseStyle->direction == FlexDirection::eRow;
            const float directionAccumulatedBase = isRow ? accumulatedWidthBase : accumulatedHeightBase;
            const float roundedBase = round? std::lroundf(directionAccumulatedBase) : directionAccumulatedBase;
            const float directionInput =  isRow ? targetWidth : targetHeight;
            const bool isCompressionMode = directionInput < roundedBase;

            return std::make_tuple(targetWidth, targetHeight, isRow, isCompressionMode);
        }

        [[nodiscard]] std::tuple<float,float,float>
        makeCompressCascadeMetrics(const float& widthInput, const float& heightInput, const bool& isRow, const bool& round ) const noexcept {
            const float& value = isRow ? widthInput : heightInput;
            const float& directionAccumulatedBase = isRow ? accumulatedWidthBase : accumulatedHeightBase;
            const float accumulatedBase = round? std::lroundf(directionAccumulatedBase) : directionAccumulatedBase;

            return std::make_tuple(value,accumulatedBase,accumulatedCompressSolidify);
        }

        [[nodiscard]] std::tuple<const float,const float,const float>
        makeCompressSizeMetrics(const float& cascadeCompressDistance, const float&cascadeBaseDistance, const float& cascadeCompressSolidify, const bool& isRow, const bool& round) const noexcept {
            const float childEffectiveBase = isRow ?validatedWidthBase : validatedHeightBase;
            const float roundedBase = round? std::lroundf(childEffectiveBase) : childEffectiveBase;

            return std::make_tuple(cascadeCompressDistance - cascadeCompressSolidify, cascadeBaseDistance - cascadeCompressSolidify, roundedBase);
        }

        [[nodiscard]] std::tuple<const bool,float,float>
        makeExpandCascadeMetrics(const float& widthInput, const float& heightInput, const bool& isRow, const bool& round) const noexcept{
            const float& value = isRow ? widthInput : heightInput;
            const float& directionAccumulatedBase = isRow? accumulatedWidthBase: accumulatedHeightBase;
            const float accumulatedBase = round? std::lroundf(directionAccumulatedBase) : directionAccumulatedBase;
            const float cascadeExpandDelta = std::max(value - accumulatedBase, 0.0f);

            return std::make_tuple(cascadeExpandDelta > 0.0f,cascadeExpandDelta, accumulatedExpandRatio);
        }

        [[nodiscard]] std::tuple<const float, const float>
        makeExpandSizeMetrics(const bool& isRow, const bool& round) const noexcept{
            const float& directionBase = isRow ?validatedWidthBase :validatedHeightBase;
            const float roundedBase = round? std::lroundf(directionBase) : directionBase;
            const float& directionMax = isRow ? validatedWidthMax : validatedHeightMax;

            const float expandCapacity = std::max(0.0f, directionMax - roundedBase);

            return std::make_tuple(roundedBase, expandCapacity);
        }

        [[nodiscard]] float computeCrossSize(const float widthInput, const float heightInput, const bool isRow) const noexcept {
            // Select cross-axis values based on direction
            const float input          = isRow ? heightInput : widthInput;
            const float validatedBase  = isRow ? validatedHeightBase : validatedWidthBase;
            const float minVal         = isRow ? validatedHeightMin  : validatedWidthMin;
            const float maxVal         = isRow ? validatedHeightMax  : validatedWidthMax;
            const LengthMode mode      = isRow ? baseStyle->height.mode : baseStyle->width.mode;

            // If auto mode, use input size as base, otherwise use validated value
            const float resolvedBase = mode == LengthMode::eAuto ? input : validatedBase;

            const float lowest = ChooseLowestDistance(maxVal, input, resolvedBase);
            return ChooseGreaterDistance(0.0f, lowest, minVal);
        }


        void compressing(const float& mainInput, const float& crossInput, const bool& isRow, const bool& round, const float& rootWidth, const float& rootHeight) noexcept {
            auto [cascadeCompressDistance, cascadeBaseDistance, cascadeCompressSolidify] = makeCompressCascadeMetrics(mainInput, crossInput, isRow, round);

            for (const auto index : compressCascadePriorities) {
                DiscadeltaRectLayoutBase* child = getBaseChild(container[index]);
                if (child == nullptr) continue;
                auto [remainDist, remainCap, validatedBase] = child->makeCompressSizeMetrics(cascadeCompressDistance, cascadeBaseDistance, cascadeCompressSolidify, isRow, round);
                const float& solidify = isRow ? widthCompressSolidify : heightCompressSolidify;
                const float& capacity = isRow ? widthCompressCapacity : heightCompressCapacity;
                const float& validatedMin = isRow ? validatedWidthMin : validatedHeightMin;
                const float compressBaseDistance = Scaler(remainDist, remainCap, capacity) + solidify;
                const float clampedDist = ChooseGreaterDistance(compressBaseDistance, validatedMin);
                const float roundedDist = round? std::lroundf(clampedDist) : clampedDist;

                const float oppositeBase = child->computeCrossSize(mainInput, crossInput, isRow);
                const float roundedOppositeBase = round? std::lroundf(oppositeBase) : oppositeBase;
                const float& widthDist = isRow ? roundedDist : roundedOppositeBase;
                const float& heightDist = isRow ? roundedOppositeBase : roundedDist;

                child->sizing(widthDist, heightDist, 0.0f, 0.0f, round, rootWidth, rootHeight);

                cascadeCompressDistance -= roundedDist;
                cascadeCompressSolidify -= solidify;
                cascadeBaseDistance -= validatedBase;
            }
        }

        void expanding(const float& widthInput, const float& heightInput, const bool& isRow, const bool& round, const float& rootWidth, const float& rootHeight) {
            auto [processingExpansion,cascadeExpandDelta, cascadeExpandRatio] = makeExpandCascadeMetrics(widthInput, heightInput, isRow, round);

            if (!processingExpansion) return;

            for (const auto index : expandCascadePriorities) {
                DiscadeltaRectLayoutBase* child = getBaseChild(container[index]);
                if (child == nullptr) continue;

                auto [validatedBase, maxDelta] = child->makeExpandSizeMetrics(isRow, round);
                const float& expandRatio = child->expandRatio;
                const float expandDelta = Scaler(cascadeExpandDelta, cascadeExpandRatio, expandRatio);
                const float clampedDelta = ChooseLowestDistance(expandDelta, maxDelta);
                const float roundedDelta = round? std::lroundf(clampedDelta) : clampedDelta;

                const float& widthDelta = isRow ? roundedDelta : 0;
                const float& heightDelta = isRow ? 0 : roundedDelta;

                const float oppositeBase = child->computeCrossSize(widthInput, heightInput, isRow);
                const float roundedOppositeBase = round? std::lroundf(oppositeBase) : oppositeBase;
                const float& widthDist = isRow ? validatedBase : roundedOppositeBase;
                const float& heightDist = isRow ? roundedOppositeBase : validatedBase;


                child->sizing(widthDist, heightDist, widthDelta, heightDelta , round, rootWidth, rootHeight);

                cascadeExpandDelta -= roundedDelta;
                cascadeExpandRatio -= expandRatio;
            }
        }

        void sizing( const float& widthInput, const float& heightInput, const float& widthDelta, const float& heightDelta, const bool& round, const float& rootWidth, const float& rootHeight) {
            if (baseStyle->positionMode == PositionMode::eAbsolute) {
                updateLayout(rootWidth, rootHeight, round);
            }
            else {
                const auto [validatedWidthInput, validatedHeightInput, isRow, processingCompression] = makeSizeMetrics(widthInput, heightInput, round);

                usedWidthSpace = 0.0f;
                usedHeightSpace = 0.0f;


                widthBase = validatedWidthInput;
                heightBase = validatedHeightInput;
                widthExpandDelta = widthDelta;
                heightExpandDelta = heightDelta;

                width = validatedWidthInput + widthDelta;
                height = validatedHeightInput + heightDelta;

                if (parent) {
                    bool pIsRow = parent->baseStyle->direction == FlexDirection::eRow;
                    parent->usedWidthSpace += pIsRow ?width : 0.0f;
                    parent->usedHeightSpace += pIsRow ? 0.0f : height;
                }

                if (processingCompression) {
                    compressing(width, height, isRow, round, rootWidth, rootHeight);
                }
                else {
                    expanding(width, height, isRow, round, rootWidth, rootHeight);
                }
            }
        }


        [[nodiscard]] std::vector<size_t> getOrderedIndices() const noexcept {
            std::vector<size_t> indices(container.size());
            std::iota(indices.begin(), indices.end(), size_t{0});

            std::ranges::sort(indices, [this](size_t a, size_t b) noexcept {
                const DiscadeltaRectLayoutBase* childA = getBaseChild(container[a]);
                const DiscadeltaRectLayoutBase* childB = getBaseChild(container[b]);

                if (!childA || !childB) return false;
                return childA->order < childB->order;
            });

            return indices;
        }



        void placing(const float relativeX = 0.0f, const float relativeY = 0.0f) noexcept {
            x = relativeX;
            y = relativeY;

            if (container.empty()) return;

            const auto orderedIndices = getOrderedIndices();
            const bool isRow = baseStyle->direction == FlexDirection::eRow;

            // Use the used space already accumulated during sizing()
            const float usedMainSpace = isRow ? usedWidthSpace : usedHeightSpace;
            const float containerMainSize = isRow ? width : height;
            const float remainSpace = std::max(0.0f, containerMainSize - usedMainSpace);

            float currentMainOffset = 0.0f;

            for (const size_t index : orderedIndices) {
                DiscadeltaRectLayoutBase* child = getBaseChild(container[index]);
                if (child == nullptr) continue;

                if (child->baseStyle->positionMode == PositionMode::eAbsolute) {
                    child->placing();
                    continue;
                }

                const bool isFirst = index == orderedIndices.front();
                const bool isLast  = index == orderedIndices.back();

                const float childMainSize = isRow ? child->width : child->height;

                const float mainPos = CalculateJustifyContent(
                    currentMainOffset,
                    containerMainSize,
                    childMainSize,
                    usedMainSpace,
                    remainSpace,
                    static_cast<int>(orderedIndices.size()),
                    isFirst,
                    isLast,
                    baseStyle->justifyContent
                );

                // Cross axis (simple start for now)
                const float crossPos = 0.0f;

                const float childX = isRow ? mainPos : crossPos;
                const float childY = isRow ? crossPos : mainPos;

                child->placing(relativeX + childX, relativeY + childY);

                // Advance for next sibling
                currentMainOffset = mainPos + childMainSize;
            }
        }

        ResourceID id{};
        float x{0.0f}, y{0.0f}, width{0.0f}, height{0.0f};
        uint32_t order{0};

        float                               accumulatedWidthBase = 0.0f;
        float                               accumulatedHeightBase = 0.0f;
        float                               accumulatedWidthMin  = 0.0f;
        float                               accumulatedHeightMin = 0.0f;
        float                               accumulatedCompressSolidify = 0.0f;
        float                               accumulatedExpandRatio = 0.0f;

        float                               validatedWidthMin   = 0.0f;
        float                               validatedHeightMin  = 0.0f;
        float                               validatedWidthBase  = 0.0f;
        float                               validatedHeightBase = 0.0f;
        float                               validatedWidthMax   = 0.0f;
        float                               validatedHeightMax  = 0.0f;

        float                               compressRatio = 0.0f;
        float                               widthCompressCapacity = 0.0f;
        float                               widthCompressSolidify = 0.0f;
        float                               heightCompressCapacity = 0.0f;
        float                               heightCompressSolidify = 0.0f;
        float                               expandRatio = 0.0f;

        float                               widthBase{0.0f};
        float                               heightBase{0.0f};
        float                               widthExpandDelta{0.0f};
        float                               heightExpandDelta{0.0f};

        float                               usedWidthSpace{0.0f};
        float                               usedHeightSpace{0.0f};

        std::vector<size_t>                 compressCascadePriorities{};
        std::vector<size_t>                 expandCascadePriorities{};

        DiscadeltaRectLayoutBase*           parent = nullptr;
        DiscadeltaRectLayoutStyleBase*      baseStyle = nullptr;

    public:
        virtual ~DiscadeltaRectLayoutBase() { clear(); }

        using Owned   = std::unique_ptr<Derived>;
        using NonOwned = Derived*;
        using Entry   = std::variant<Owned, NonOwned>;

        std::vector<Entry> container{};
        std::unordered_map<ResourceID, size_t, ResourceIDHash, ResourceIDEq> resourceIndex{};

        [[nodiscard]] bool isRoot() const noexcept { return parent == nullptr; }

        [[nodiscard]] glm::mat4 getTransformMat4() const {
            return MakeTransformModel(glm::vec3{x, y, 0.0f}, IDENTITY_QUAT,
                                    glm::vec3{width, height, 1.0f});
        }

        void updateMetrics(const bool updateFamily = true) noexcept{
            preComputeMetrics();
            validateMetrics();
            updatePriorityLists();

            if (parent == nullptr || parent == this || !updateFamily) return;

            parent->updateMetrics();
        }

        [[nodiscard]] DiscadeltaRectLayoutBase* getBaseChild(const Entry& entry) const {
            return std::visit([]<typename T0>(T0&& arg) -> DiscadeltaRectLayoutBase* {
                if constexpr (std::is_same_v<std::decay_t<T0>, Owned>) {
                    return arg.get();
                } else {
                    return arg;
                }
            }, entry);
        }

        void add(Owned resource) {
            if (!resource) return;

            if (resource->parent && resource->parent != this) {
                resource->parent->remove(resource.get());
            }

            resource->updateMetrics();

            resource->parent = this;
            onAdd(resource.get());

            const size_t idx = container.size();
            resourceIndex[resource->id] = idx;
            container.emplace_back(std::move(resource));

            updateMetrics();

            onTreeStructureChanged();
        }

        void add(NonOwned resource) {
            if (!resource) return;

            if (resource->parent && resource->parent != this) {
                resource->parent->remove(resource);
            }

            resource->parent = this;
            onAdd(resource);

            const size_t idx = container.size();
            resourceIndex[resource->id] = idx;
            container.emplace_back(resource);

            resource->updateMetrics();

            onTreeStructureChanged();
        }

        template<typename T = Derived, typename... Args>
        [[nodiscard]] T* createChild(Args&&... args) {
            auto newChild = std::make_unique<T>(std::forward<Args>(args)...);
            T* rawPtr = newChild.get();

            add(std::move(newChild));
            return rawPtr;
        }

        [[nodiscard]] bool isContainerEmpty() const noexcept {
            return container.empty();
        }

        // ====================== CONTAINS ======================
        [[nodiscard]] bool contains(const ResourceID& id) const {
            return resourceIndex.contains(id);
        }

        [[nodiscard]] bool contains(const std::string_view name) const {
            return contains(ResourceID{name});
        }

        // ====================== REMOVE ======================
        bool remove(NonOwned resource) {
            if (!resource || resource->parent != this) return false;

            auto it = resourceIndex.find(resource->id);
            if (it == resourceIndex.end()) return false;

            onRemove(resource);

            resource->parent = nullptr;

            size_t idx = it->second;
            container.erase(container.begin() + idx);
            resourceIndex.erase(it);

            for (size_t i = idx; i < container.size(); ++i) {
                std::visit([&](auto&& arg) {
                    if (arg) resourceIndex[arg->id] = i;
                }, container[i]);
            }

            updateMetrics();

            onTreeStructureChanged();
            return true;
        }

        void updateLayout(const float width, const float height, const bool round ) {
            sizing(width, height, 0.0f,0.0f, round, width, height);
            placing();
        }

        // ====================== QUERY ======================
        template<typename T = Derived>
        [[nodiscard]] T* Q(const ResourceID& id) const {
            auto it = resourceIndex.find(id);
            if (it == resourceIndex.end()) return nullptr;

            return std::visit([]<typename T0>(T0&& arg) -> T* {
                if constexpr (std::is_same_v<std::decay_t<T0>, Owned>)
                    return dynamic_cast<T*>(arg.get());
                else
                    return dynamic_cast<T*>(arg);
            }, container[it->second]);
        }

        template<typename T = Derived>
        [[nodiscard]] T* Q(std::string_view name) const {
            return Q<T>(ResourceID{name});
        }
    };
}

