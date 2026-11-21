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

    // ─────────────────────────────────────────────────────────────
    // Free helpers (replace the three free functions that were in
    // the original ufox::geometry namespace)
    // ─────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr float CalculateAlign(
        const float childSize,
        const float parentSize,
        const AlignMode mode) noexcept
    {
        switch (mode) {
        case AlignMode::eStart:   return 0.0f;
        case AlignMode::eEnd:     return parentSize - childSize;
        case AlignMode::eCenter:  return (parentSize - childSize) * 0.5f;
        case AlignMode::eStretch: return 0.0f;
        case AlignMode::eAuto:    return parentSize - childSize;
        default:                  return 0.0f;
        }
    }

    [[nodiscard]] constexpr float CalculateJustifyContent(
        const float  currentOffset,
        const float  parentSize,
        const float  childSize,
        const float  usedSpace,
        const float  remainSpace,
        const int    itemCount,
        const bool   isFirst,
        const bool   isLast,
        JustifyContentMode mode) noexcept
    {
        switch (mode) {

        case JustifyContentMode::eStart:
            return currentOffset;

        case JustifyContentMode::eEnd:
            return isFirst ? currentOffset + parentSize - usedSpace
                           : currentOffset;

        case JustifyContentMode::eCenter:
            return isFirst ? currentOffset + (parentSize - usedSpace) * 0.5f
                           : currentOffset;

        case JustifyContentMode::eSpaceBetween: {
            if (itemCount <= 1) return currentOffset;
            const float gap = remainSpace / static_cast<float>(itemCount - 1);
            if (isFirst) return currentOffset;
            if (isLast)  return parentSize - childSize;
            return currentOffset + gap;
        }

        case JustifyContentMode::eSpaceAround: {
            const float half = remainSpace / static_cast<float>(itemCount) * 0.5f;
            if (isFirst) return currentOffset + half;
            if (isLast)  return parentSize - childSize - half;
            return currentOffset + half * 2.0f;
        }

        case JustifyContentMode::eSpaceEvenly: {
            const float gap = remainSpace / static_cast<float>(itemCount + 1);
            return currentOffset + gap;
        }

        default: return 0.0f;
        }
    }

    // Guard against both degenerate inputs in one place.
    [[nodiscard]] constexpr float Scaler(
        float distance,
        float totalCapacity,
        float ownCapacity) noexcept
    {
        if (distance <= 0.0f || totalCapacity <= 0.0f || ownCapacity <= 0.0f)
            return 0.0f;
        return distance / totalCapacity * ownCapacity;
    }

    template<typename Derived>
    class DiscadeltaRectLayoutBase {

        // ── Subclass hooks ────────────────────────────────────────
        virtual void onAdd(Derived* added)              = 0;
        virtual void onRemove(Derived* removed)         = 0;
        virtual void onTreeStructureChanged()           = 0;

    protected:

        // ── Construction ──────────────────────────────────────────

        explicit DiscadeltaRectLayoutBase(const engine::ResourceID& _id) {
            id = _id.IsValid() ? _id : engine::ResourceID{nanoid::generate(12)};
        }

        DiscadeltaRectLayoutBase()
            : id(nanoid::generate(12)) {}

        // ── Tree mutation ─────────────────────────────────────────

        void clear() noexcept {
            // Detach all children so they don't hold a stale parent pointer.
            for (auto& entry : container) {
                if (auto* child = getBaseChild(entry))
                    child->parent = nullptr;
            }
            container.clear();
            resourceIndex.clear();

            if (parent) {
                parent->resourceIndex.erase(this->id);
                parent = nullptr;
            }
        }

        // ── Metrics pipeline ─────────────────────────────────────
        //
        //  Phase 1 — preComputeMetrics:  walk children, accumulate
        //  Phase 2 — validateMetrics:    apply style constraints
        //  Phase 3 — updatePriorityLists: sort compress/expand order

        void preComputeMetrics() noexcept {
            // Reset accumulators
            accumulated = {};

            if (container.empty() || !baseStyle) return;

            const bool isRow = (baseStyle->direction == FlexDirection::eRow);

            for (const auto& entry : container) {
                const DiscadeltaRectLayoutBase* child = getBaseChild(entry);
                if (!child) continue;

                const AxisSlice childBase  = AxisSlice::from(child->validatedWidthBase,  child->validatedHeightBase,  isRow);
                const AxisSlice childMin   = AxisSlice::from(child->validatedWidthMin,   child->validatedHeightMin,   isRow);
                const AxisSlice childAccMin= AxisSlice::from(child->accumulated.widthMin, child->accumulated.heightMin, isRow);
                const float     childSolid = isRow ? child->widthCompressSolidify
                                                   : child->heightCompressSolidify;

                // Main axis: children stack
                accumulated.mainBase(isRow) += childBase.main;

                // Cross axis: children compete (take the max)
                float& accCrossBase = accumulated.crossBase(isRow);
                accCrossBase = ChooseGreaterDistance(accCrossBase, childBase.cross);

                // Main-axis minimum
                const float childMainMin = ChooseGreaterDistance(childMin.main, childSolid);
                accumulated.mainMin(isRow) += ChooseGreaterDistance(childMainMin, childAccMin.main);

                // Cross-axis minimum
                const float childCrossMin = ChooseGreaterDistance(childMin.cross, childAccMin.cross);
                float& accCrossMin = accumulated.crossMin(isRow);
                accCrossMin = ChooseGreaterDistance(accCrossMin, childCrossMin);

                accumulated.compressSolidify += childSolid;
                accumulated.expandRatio      += child->expandRatio;
            }
        }

        void validateMetrics() noexcept {
            if (!baseStyle) return;

            auto resolveMin = [](const Length& l, float accum) {
                return l.mode == LengthMode::eAuto
                    ? accum
                    : ChooseGreaterDistance(0.0f, l.value);
            };
            auto resolveMax = [](const Length& l, float minVal) {
                return l.mode == LengthMode::eAuto
                    ? std::numeric_limits<float>::max()
                    : ChooseGreaterDistance(l.value, minVal);
            };
            auto resolveBase = [](const Length& l, float accum) {
                return l.mode == LengthMode::eAuto ? accum : l.value;
            };

            validatedWidthMin  = resolveMin(baseStyle->minWidth,   accumulated.widthMin);
            validatedHeightMin = resolveMin(baseStyle->minHeight,  accumulated.heightMin);
            validatedWidthMax  = resolveMax(baseStyle->maxWidth,   validatedWidthMin);
            validatedHeightMax = resolveMax(baseStyle->maxHeight,  validatedHeightMin);
            validatedWidthBase = resolveBase(baseStyle->width,     accumulated.widthBase);
            validatedHeightBase= resolveBase(baseStyle->height,    accumulated.heightBase);

            compressRatio          = std::max(0.0f, baseStyle->flexCompress);
            widthCompressCapacity  = validatedWidthBase  * compressRatio;
            widthCompressSolidify  = std::max(0.0f, validatedWidthBase  - widthCompressCapacity);
            heightCompressCapacity = validatedHeightBase * compressRatio;
            heightCompressSolidify = std::max(0.0f, validatedHeightBase - heightCompressCapacity);
            expandRatio            = std::max(0.0f, baseStyle->flexExpand);
            order                  = baseStyle->order;
        }

        void updatePriorityLists() noexcept {
            compressCascadePriorities.clear();
            expandCascadePriorities.clear();

            if (container.empty() || !baseStyle) return;

            const bool  isRow      = (baseStyle->direction == FlexDirection::eRow);
            const size_t childCount = container.size();

            struct PriEntry { float room; size_t idx; };
            std::vector<PriEntry> cPri, ePri;
            cPri.reserve(childCount);
            ePri.reserve(childCount);

            for (size_t i = 0; i < childCount; ++i) {
                const DiscadeltaRectLayoutBase* child = getBaseChild(container[i]);
                if (!child) continue;
                if (child->baseStyle &&
                    child->baseStyle->positionMode == PositionMode::eAbsolute) continue;

                const AxisSlice base = AxisSlice::from(child->validatedWidthBase,  child->validatedHeightBase,  isRow);
                const AxisSlice vMin = AxisSlice::from(child->validatedWidthMin,   child->validatedHeightMin,   isRow);
                const AxisSlice vMax = AxisSlice::from(child->validatedWidthMax,   child->validatedHeightMax,   isRow);

                cPri.push_back({ std::max(0.0f, base.main - vMin.main), i });
                ePri.push_back({ std::max(0.0f, vMax.main - base.main), i });
            }

            // Compress: least room first (fairest squeeze)
            std::ranges::sort(cPri, [](const PriEntry& a, const PriEntry& b){
                return a.room < b.room;
            });
            compressCascadePriorities.reserve(cPri.size());
            for (const auto& e : cPri) compressCascadePriorities.push_back(e.idx);

            // Expand: most room first (prevents over-expansion of small items)
            std::ranges::sort(ePri, [](const PriEntry& a, const PriEntry& b){
                return a.room > b.room;
            });
            expandCascadePriorities.reserve(ePri.size());
            for (const auto& e : ePri) expandCascadePriorities.push_back(e.idx);
        }

        // ── Sizing helpers ────────────────────────────────────────

        [[nodiscard]] SizeMetrics makeSizeMetrics(
            float targetWidth, float targetHeight, bool round) const noexcept
        {
            const bool  isRow = baseStyle->direction == FlexDirection::eRow;
            const float accBase = isRow ? accumulated.widthBase : accumulated.heightBase;
            const float roundedAccBase = round ? std::lroundf(accBase) : accBase;
            const float dirInput       = isRow ? targetWidth : targetHeight;

            return { targetWidth, targetHeight, isRow, dirInput < roundedAccBase };
        }

        [[nodiscard]] CompressCascade makeCompressCascadeMetrics(
            float widthInput, float heightInput, bool isRow, bool round) const noexcept
        {
            const float input   = isRow ? widthInput : heightInput;
            const float accBase = isRow ? accumulated.widthBase : accumulated.heightBase;
            const float rBase   = round ? std::lroundf(accBase) : accBase;
            return { input - accumulated.compressSolidify,
                     rBase  - accumulated.compressSolidify,
                     0.0f };   // ownBase filled per-child below
        }

        // Called per child during compression pass.
        [[nodiscard]] CompressCascade makeChildCompressMetrics(
            const CompressCascade& cascade, bool isRow, bool round) const noexcept
        {
            const float childBase = isRow ? validatedWidthBase : validatedHeightBase;
            const float rBase     = round ? std::lroundf(childBase) : childBase;
            return { cascade.remainDistance,
                     cascade.remainCapacity,
                     rBase };
        }

        [[nodiscard]] ExpandCascade makeExpandCascadeMetrics(
            float widthInput, float heightInput, bool isRow, bool round) const noexcept
        {
            const float input   = isRow ? widthInput : heightInput;
            const float accBase = isRow ? accumulated.widthBase : accumulated.heightBase;
            const float rBase   = round ? std::lroundf(accBase) : accBase;
            const float delta   = std::max(0.0f, input - rBase);
            return { delta > 0.0f, delta, accumulated.expandRatio };
        }

        [[nodiscard]] ExpandSizeInfo makeExpandSizeInfo(bool isRow, bool round) const noexcept {
            const float base    = isRow ? validatedWidthBase  : validatedHeightBase;
            const float maxVal  = isRow ? validatedWidthMax   : validatedHeightMax;
            const float rBase   = round ? std::lroundf(base) : base;
            return { rBase, std::max(0.0f, maxVal - rBase) };
        }

        [[nodiscard]] float computeCrossSize(
            float widthInput, float heightInput, bool isRow) const noexcept
        {
            const float input      = isRow ? heightInput : widthInput;
            const float vBase      = isRow ? validatedHeightBase : validatedWidthBase;
            const float vMin       = isRow ? validatedHeightMin  : validatedWidthMin;
            const float vMax       = isRow ? validatedHeightMax  : validatedWidthMax;
            const LengthMode mode  = isRow ? baseStyle->height.mode : baseStyle->width.mode;

            const float resolved = (mode == LengthMode::eAuto) ? input : vBase;
            return std::clamp(resolved, vMin, vMax);
        }

        // ── Core layout passes ────────────────────────────────────

        void compressing(
            float mainInput, float crossInput,
            bool isRow, bool round,
            float rootWidth, float rootHeight) noexcept
        {
            CompressCascade cascade = makeCompressCascadeMetrics(
                mainInput, crossInput, isRow, round);

            for (const size_t idx : compressCascadePriorities) {
                DiscadeltaRectLayoutBase* child = getBaseChild(container[idx]);
                if (!child) continue;

                const float childSolid    = isRow ? child->widthCompressSolidify
                                                  : child->heightCompressSolidify;
                const float childCapacity = isRow ? child->widthCompressCapacity
                                                  : child->heightCompressCapacity;
                const float childMin      = isRow ? child->validatedWidthMin
                                                  : child->validatedHeightMin;
                const float childBase     = isRow ? child->validatedWidthBase
                                                  : child->validatedHeightBase;
                const float rChildBase    = round ? std::lroundf(childBase) : childBase;

                // How much this child's main size compresses.
                const float mainSize = std::max(
                    childMin,
                    Scaler(cascade.remainDistance - childSolid,
                           cascade.remainCapacity  - childSolid,
                           childCapacity)
                    + childSolid);
                const float roundedMain = round ? std::lroundf(mainSize) : mainSize;

                const float cross    = child->computeCrossSize(mainInput, crossInput, isRow);
                const float rCross   = round ? std::lroundf(cross) : cross;

                const auto [w, h] = AxisSlice{roundedMain, rCross}.toWH(isRow);
                child->sizing(w, h, 0.0f, 0.0f, round, rootWidth, rootHeight);

                cascade.remainDistance  -= roundedMain;
                cascade.remainCapacity  -= rChildBase;
                // solidify budget consumed by this child
            }
        }

        void expanding(
            float widthInput, float heightInput,
            bool isRow, bool round,
            float rootWidth, float rootHeight) noexcept
        {
            ExpandCascade cascade = makeExpandCascadeMetrics(
                widthInput, heightInput, isRow, round);

            if (!cascade.active) return;

            for (const size_t idx : expandCascadePriorities) {
                DiscadeltaRectLayoutBase* child = getBaseChild(container[idx]);
                if (!child) continue;

                const ExpandSizeInfo info = child->makeExpandSizeInfo(isRow, round);

                const float rawDelta    = Scaler(cascade.totalDelta, cascade.totalRatio, child->expandRatio);
                const float clampedDelta= std::min(rawDelta, info.maxDelta);
                const float roundedDelta= round ? std::lroundf(clampedDelta) : clampedDelta;

                const float cross  = child->computeCrossSize(widthInput, heightInput, isRow);
                const float rCross = round ? std::lroundf(cross) : cross;

                const auto [baseW, baseH] = AxisSlice{info.base, rCross}.toWH(isRow);
                const auto [dW,    dH]    = AxisSlice{roundedDelta, 0.0f}.toWH(isRow);
                child->sizing(baseW, baseH, dW, dH, round, rootWidth, rootHeight);

                cascade.totalDelta -= roundedDelta;
                cascade.totalRatio -= child->expandRatio;
            }
        }

        // Extracted from sizing() to keep it readable.
        void applyCrossStretch(float flexWidth, float flexHeight) noexcept {
                if (!parent || !parent->baseStyle) return;

                const bool pIsRow = parent->baseStyle->direction == FlexDirection::eRow;
                const bool stretch = baseStyle->alignSelf == AlignMode::eStretch ||
                                     parent->baseStyle->alignItems == AlignMode::eStretch;

                if (pIsRow) {
                    // Cross axis = height
                    if (stretch) {
                        height = flexHeight;                                // stretch to full cross size
                    } else {
                        height = std::clamp(validatedHeightBase, validatedHeightMin, validatedHeightMax);
                    }
                } else {
                    // Cross axis = width
                    if (stretch) {
                        width = flexWidth;                                  // stretch to full cross size
                    } else {
                        width = std::clamp(validatedWidthBase, validatedWidthMin, validatedWidthMax);
                    }
                }
    }

        void sizing(
            float widthInput, float heightInput,
            float widthDelta, float heightDelta,
            bool  round,
            float rootWidth, float rootHeight) noexcept
        {
            if (!baseStyle) return;

            // Absolute elements re-layout themselves against the root.
            if (baseStyle->positionMode == PositionMode::eAbsolute) {
                updateLayout(rootWidth, rootHeight, round);
                return;
            }

            const SizeMetrics sm = makeSizeMetrics(widthInput, heightInput, round);
            const float flexWidth  = widthInput  + widthDelta;
            const float flexHeight = heightInput + heightDelta;

            width  = flexWidth;
            height = flexHeight;
            usedMainSpace = 0.0f;

            applyCrossStretch(flexWidth, flexHeight);

            if (parent && parent->baseStyle) {
                const bool pIsRow = parent->baseStyle->direction == FlexDirection::eRow;
                const float childMain = pIsRow ? width : height;
                parent->usedMainSpace += childMain;
            }

            if (sm.isCompression)
                compressing(flexWidth, flexHeight, sm.isRow, round, rootWidth, rootHeight);
            else
                expanding  (flexWidth, flexHeight, sm.isRow, round, rootWidth, rootHeight);
        }

        // ── Placement pass ────────────────────────────────────────

        // Returns indices into container sorted by child order value.
        // Null children are filtered out here.
        [[nodiscard]] std::vector<size_t> getOrderedIndices() const noexcept {
            std::vector<size_t> indices;
            indices.reserve(container.size());

            for (size_t i = 0; i < container.size(); ++i) {
                if (getBaseChild(container[i]))
                    indices.push_back(i);
            }

            std::ranges::stable_sort(indices, [this](size_t a, size_t b) noexcept {
                return getBaseChild(container[a])->order
                     < getBaseChild(container[b])->order;
            });

            return indices;
        }

        void placing(float relativeX = 0.0f, float relativeY = 0.0f) noexcept {
            x = relativeX;
            y = relativeY;

            if (container.empty() || !baseStyle) return;

            const std::vector<size_t> ordered = getOrderedIndices();
            if (ordered.empty()) return;

            const bool  isRow         = baseStyle->direction == FlexDirection::eRow;
            const float usedMainSpace = this->usedMainSpace;
            const float containerMain = isRow ? width  : height;
            const float remainSpace   = std::max(0.0f, containerMain - usedMainSpace);

            float mainOffset = 0.0f;

            for (size_t ordIdx = 0; ordIdx < ordered.size(); ++ordIdx) {
                DiscadeltaRectLayoutBase* child = getBaseChild(container[ordered[ordIdx]]);
                // null already filtered in getOrderedIndices

                // Absolute children are placed at their own origin.
                if (child->baseStyle &&
                    child->baseStyle->positionMode == PositionMode::eAbsolute) {
                    child->placing();
                    continue;
                }

                const bool  isFirst     = ordIdx == 0;
                const bool  isLast      = ordIdx == ordered.size() - 1;
                const float childMain   = isRow ? child->width  : child->height;
                const float childCross  = isRow ? child->height : child->width;
                const float parentCross = isRow ? height : width;

                const float mainPos = CalculateJustifyContent(
                    mainOffset,
                    containerMain,
                    childMain,
                    usedMainSpace,
                    remainSpace,
                    static_cast<int>(ordered.size()),
                    isFirst,
                    isLast,
                    baseStyle->justifyContent);

                const AlignMode crossMode =
                    (child->baseStyle && child->baseStyle->alignSelf != AlignMode::eAuto)
                        ? child->baseStyle->alignSelf
                        : baseStyle->alignItems;

                const float crossPos = CalculateAlign(childCross, parentCross, crossMode);

                const float childX = isRow ? mainPos  : crossPos;
                const float childY = isRow ? crossPos : mainPos;

                child->placing(relativeX + childX, relativeY + childY);

                mainOffset = mainPos + childMain;
            }
        }

        // ── Internal accumulator — grouped to make clearing trivial ──
        struct Accumulated {
            float widthBase{};
            float heightBase{};
            float widthMin{};
            float heightMin{};
            float compressSolidify{};
            float expandRatio{};

            // Mutable references selected by axis direction.
            float& mainBase(bool isRow)  noexcept { return isRow ? widthBase  : heightBase; }
            float& crossBase(bool isRow) noexcept { return isRow ? heightBase : widthBase;  }
            float& mainMin(bool isRow)   noexcept { return isRow ? widthMin   : heightMin;  }
            float& crossMin(bool isRow)  noexcept { return isRow ? heightMin  : widthMin;   }
        };

        // ── Data members ──────────────────────────────────────────

        ResourceID              id{};

        // Resolved layout output (written by sizing/placing)
        float x{};     float y{};
        float width{}; float height{};
        uint32_t order{};

        // Phase-1 accumulators (reset each updateMetrics)
        Accumulated accumulated{};

        // Phase-2 validated constraints
        float validatedWidthMin{};    float validatedHeightMin{};
        float validatedWidthBase{};   float validatedHeightBase{};
        float validatedWidthMax{};    float validatedHeightMax{};

        // Derived compress/expand values
        float compressRatio{};
        float widthCompressCapacity{};  float widthCompressSolidify{};
        float heightCompressCapacity{}; float heightCompressSolidify{};
        float expandRatio{};

        // Transient: filled during sizing(), read during placing()
        float usedMainSpace{};

        // Phase-3 sorted child index lists
        std::vector<size_t> compressCascadePriorities{};
        std::vector<size_t> expandCascadePriorities{};

        DiscadeltaRectLayoutBase*       parent    = nullptr;
        DiscadeltaRectLayoutStyleBase*  baseStyle = nullptr;

    public:
        virtual ~DiscadeltaRectLayoutBase() { clear(); }

        using Owned    = std::unique_ptr<Derived>;
        using NonOwned = Derived*;
        using Entry    = std::variant<Owned, NonOwned>;

        std::vector<Entry>  container{};
        std::unordered_map<engine::ResourceID, size_t,
                           engine::ResourceIDHash,
                           engine::ResourceIDEq>  resourceIndex{};

        // ── Public queries ────────────────────────────────────────

        [[nodiscard]] bool isRoot() const noexcept { return parent == nullptr; }

        [[nodiscard]] bool isContainerEmpty() const noexcept {
            return container.empty();
        }

        [[nodiscard]] bool contains(const engine::ResourceID& id_) const {
            return resourceIndex.contains(id_);
        }

        [[nodiscard]] bool contains(std::string_view name) const {
            return contains(engine::ResourceID{name});
        }

        [[nodiscard]] glm::mat4 getTransformMat4() const {
            return MakeTransformModel(glm::vec3{x, y, 0.0f},
                                      IDENTITY_QUAT,
                                      glm::vec3{width, height, 1.0f});
        }

        // ── Metrics update (public entry point) ───────────────────
        //
        //  updateFamily=true  → bubble up to root so parent constraints
        //                        re-evaluate after a child change.
        //  updateFamily=false → update only this node (batch inserts).

        void updateMetrics(bool updateFamily = true) noexcept {
            preComputeMetrics();
            validateMetrics();
            updatePriorityLists();

            if (!updateFamily || !parent || parent == this) return;
            parent->updateMetrics();
        }

        // ── Layout entry point ────────────────────────────────────

        void updateLayout(float w, float h, bool round) noexcept {
            sizing(w, h, 0.0f, 0.0f, round, w, h);
            placing();
        }

        // ── Child access ──────────────────────────────────────────

        [[nodiscard]] DiscadeltaRectLayoutBase* getBaseChild(const Entry& entry) const {
            return std::visit([]<typename T>(T&& arg) -> DiscadeltaRectLayoutBase* {
                if constexpr (std::is_same_v<std::decay_t<T>, Owned>)
                    return arg.get();
                else
                    return arg;
            }, entry);
        }

        // ── Mutation ──────────────────────────────────────────────

        void add(Owned resource) {
            if (!resource) return;

            if (resource->parent && resource->parent != this)
                resource->parent->remove(resource.get());

            resource->parent = this;
            onAdd(resource.get());

            resourceIndex[resource->id] = container.size();
            container.emplace_back(std::move(resource));

            updateMetrics();
            onTreeStructureChanged();
        }

        void add(NonOwned resource) {
            if (!resource) return;

            if (resource->parent && resource->parent != this)
                resource->parent->remove(resource);

            resource->parent = this;
            onAdd(resource);

            resourceIndex[resource->id] = container.size();
            container.emplace_back(resource);

            updateMetrics();
            onTreeStructureChanged();
        }

        template<typename T = Derived, typename... Args>
        [[nodiscard]] T* createChild(Args&&... args) {
            auto child = std::make_unique<T>(std::forward<Args>(args)...);
            T* raw = child.get();
            add(std::move(child));
            return raw;
        }

        bool remove(NonOwned resource) {
            if (!resource || resource->parent != this) return false;

            auto it = resourceIndex.find(resource->id);
            if (it == resourceIndex.end()) return false;

            onRemove(resource);
            resource->parent = nullptr;

            const size_t idx = it->second;
            container.erase(container.begin() + static_cast<ptrdiff_t>(idx));
            resourceIndex.erase(it);

            // Re-index everything after the removed slot.
            for (size_t i = idx; i < container.size(); ++i) {
                if (auto* child = getBaseChild(container[i]))
                    resourceIndex[child->id] = i;
            }

            updateMetrics();
            onTreeStructureChanged();
            return true;
        }

        // ── Query ─────────────────────────────────────────────────

        template<typename T = Derived>
        [[nodiscard]] T* Q(const engine::ResourceID& rid) const {
            auto it = resourceIndex.find(rid);
            if (it == resourceIndex.end()) return nullptr;

            return std::visit([]<typename U>(U&& arg) -> T* {
                if constexpr (std::is_same_v<std::decay_t<U>, Owned>)
                    return dynamic_cast<T*>(arg.get());
                else
                    return dynamic_cast<T*>(arg);
            }, container[it->second]);
        }

        template<typename T = Derived>
        [[nodiscard]] T* Q(std::string_view name) const {
            return Q<T>(engine::ResourceID{name});
        }
    };
}

