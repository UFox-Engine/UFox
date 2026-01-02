//
// #include <exception>
// #include <iostream>
// #include <thread>
//
// #include "vulkan/vulkan.hpp"
//
// import ufox_engine;
//
//
// int main() {
//
//     try {
//         ufox::UFoxEngine ide;
//         ide.Init();
//         ide.Run();
//
//      } catch (const std::exception& e) {
//         std::cerr << e.what() << std::endl;
//         return EXIT_FAILURE;
//     }
//     return EXIT_SUCCESS;
// }



// #include <iostream>
// #include <vector>
// #include <format>
// #include <iomanip>  // for std::setw, std::fixed, std::setprecision
// #include <ranges>
//
// struct DiscadeltaSegment {
//
//     float base;
//     float delta;
//     float value;
// };
//
// struct DiscadeltaSegmentConfig {
//     std::string name;
//     float base;
//     float reduceRatio;
//     float shareRatio;
//     float min;
//     float max;
// };
//
// struct DiscadeltaSegmentMetrics {
//     float baseDistance{0.0f};
//     float solidifyBaseDistance{0.0f};
//     float baseShareRatio{0.0f};
//     float shareRatio{0.0f};
//     float min{0.0f};
//     float max{0.0f};
//     DiscadeltaSegment* segmentPtr{nullptr};
// };
//
// struct DiscadeltaRedistributeMetrics {
//     float remainShareDistance{0.0f};
//     float accumulateBaseDistance{0.0f};
//     float accumulateSolidifyBase{0.0f};
//     float accumulateBaseShareRatio{0.0f};
//     float accumulateShareRatio{0.0f};
//
//     [[nodiscard]] bool IsRootDistanceReached() const { return remainShareDistance <= accumulateBaseDistance; }
// };
//
// constexpr auto PrepareDiscadeltaComputeContext = [](const std::vector<DiscadeltaSegmentConfig>& configs, std::vector<DiscadeltaSegment>& segmentDistances, const float& rootBase) {
//     segmentDistances.clear();
//     segmentDistances.reserve(configs.size());
//
//     std::vector<DiscadeltaSegmentMetrics> metrics;
//     metrics.reserve(configs.size());
//
//     DiscadeltaRedistributeMetrics redistributeMetrics;
//     redistributeMetrics.remainShareDistance = rootBase;
//
//
//     for (const auto& [name, base, reduceRatio, shareRatio, min, max] : configs) {
//         const float validatedMin = std::max(0.0f, min);  // never negative
//         const float validatedMax = std::max(validatedMin, max);  // max >= min
//         const float validatedBaseDistance = std::clamp(base ,validatedMin, validatedMax);  // base >= min && base <= max
//         const float solidifyDistance = validatedBaseDistance * (1.0f - reduceRatio);
//         const float baseShareDist = validatedBaseDistance * reduceRatio;
//         const float baseShareRatio = baseShareDist <= 0.0f? 0.0f: baseShareDist / 100.0f;
//
//         segmentDistances.push_back({validatedBaseDistance, 0.0f, validatedBaseDistance});  // base distance is always validated
//
//         redistributeMetrics.accumulateBaseDistance += validatedBaseDistance;
//         redistributeMetrics.accumulateSolidifyBase += solidifyDistance;
//         redistributeMetrics.accumulateBaseShareRatio += baseShareRatio;
//         redistributeMetrics.accumulateShareRatio += shareRatio;
//
//         metrics.push_back({validatedBaseDistance, solidifyDistance, baseShareRatio, shareRatio, validatedMin, validatedMax, &segmentDistances.back()});
//     }
//
//     return std::make_pair(metrics, redistributeMetrics);
// };
//
// void RedistributeDiscadeltaBaseDistance (DiscadeltaRedistributeMetrics& redistributeMetrics, const std::vector<DiscadeltaSegmentMetrics> &segmentMetrics) {
//
//     DiscadeltaRedistributeMetrics recursiveRedistributeMetrics(segmentMetrics.size());
//
//     for (const auto & segmentMetric : segmentMetrics) {
//         const float& currentShareRatio =  segmentMetric.baseShareRatio;
//         const float& currentSolidifyBaseDistance = segmentMetric.solidifyBaseDistance;
//         const float& min = segmentMetric.min;
//         const float currentShareDistance = redistributeMetrics.remainShareDistance - redistributeMetrics.accumulateSolidifyBase;
//         const float distributedShare = (currentShareDistance <= 0.0f || redistributeMetrics.accumulateBaseShareRatio <= 0.0f || currentShareRatio <= 0.0f ? 0.0f :
//         currentShareDistance / redistributeMetrics.accumulateBaseShareRatio * currentShareRatio) + currentSolidifyBaseDistance;
//
//
//         const float currentDistance = std::max(distributedShare, min);
//
//         DiscadeltaSegment& segment = *segmentMetric.segmentPtr;
//
//         segment.value = currentDistance;
//         segment.base = currentDistance;
//         redistributeMetrics.accumulateSolidifyBase -= currentSolidifyBaseDistance;
//         redistributeMetrics.accumulateBaseShareRatio -= currentShareRatio;
//         redistributeMetrics.remainShareDistance -= currentDistance;
//     }
// }
//
// int main()
// {
//     std::vector<DiscadeltaSegmentConfig> segmentConfigs{
//         {"1",200.0f, 0.7f, 0.1f ,0.0f, 100.0f},
//         {"2",200.0f, 1.0f, 1.0f ,300.0f, 800.0f},
//         {"3",150.0f, 0.0f, 2.0f, 0.0f, 200.0f},
//         {"4",350.0f, 0.3f, 0.5f, 50.0f, 300.0f}
//     };
//
//     float rootBase = 800.0f;
//     const size_t segmentCount = segmentConfigs.size();
//     std::vector<DiscadeltaSegment> segmentDistances;
//
//
//     auto [metrics, redistributeMetrics] = PrepareDiscadeltaComputeContext(segmentConfigs, segmentDistances, rootBase);
//
//     DiscadeltaRedistributeMetrics accumulators = redistributeMetrics;
//
//
//     if (redistributeMetrics.IsRootDistanceReached()) {
//         RedistributeDiscadeltaBaseDistance(redistributeMetrics, metrics);
//     }
//
//
//
// #pragma region //Compute Segment Delta
//
//     // if (redistributeMetrics.remainShareDistance > 0.0f) {
//     //     float remainShareRatio = accumulators.shareRatio;
//     //
//     //     for (size_t i = 0; i < segmentCount; ++i) {
//     //         const float& ratio = segmentMetrics[i].shareRatio;
//     //         const float shareDelta = remainShareRatio <= 0.0f || ratio <= 0.0f? 0.0f : redistributeMetrics.remainShareDistance / remainShareRatio * ratio;
//     //
//     //         segmentDistances[i].delta = shareDelta;
//     //         segmentDistances[i].value += shareDelta;
//     //
//     //         redistributeMetrics.remainShareDistance -= shareDelta;
//     //         remainShareRatio -= ratio;
//     //     }
//     // }
//
// #pragma endregion //Compute Segment Delta
//
// #pragma region //Print Result
//     std::cout << "\n=== Discadelta Layout: Metrics & Final Distribution ===\n";
//     std::cout << std::format("Root distance: {:.1f} px\n\n", rootBase);
//
//     // Header for metrics + results
//     std::cout << std::left
//               << std::setw(8)  << "Seg"
//               << std::setw(12) << "Config Base"
//               << std::setw(10) << "Validated"
//               << std::setw(10) << "Min"
//               << std::setw(10) << "Max"
//               << std::setw(14) << "SolidifyBase"
//               << std::setw(16) << "BaseShareDist"
//               << std::setw(16) << "BaseShareRatio"
//               << std::setw(12) << "Final Base"
//               << std::setw(12) << "Delta"
//               << std::setw(14) << "Final Value"
//               << '\n';
//
//     std::cout << std::string(140, '-') << '\n';
//
//     float total = 0.0f;
//
//     for (size_t i = 0; i < segmentCount; ++i) {
//         const auto& cfg = segmentConfigs[i];
//         const auto& met = metrics[i];    // from PrepareDiscadeltaComputeContext
//         const auto& res = segmentDistances[i];
//
//         const float reduceDist     = met.solidifyBaseDistance;
//         const float baseShareDist  = met.baseDistance;  // renamed for clarity
//         const float baseShareRatio = met.baseShareRatio;
//
//         total += res.value;
//
//         std::cout << std::fixed << std::setprecision(3)
//                   << std::setw(8)  << (i + 1)
//                   << std::setw(12) << cfg.base
//                   << std::setw(10) << met.baseDistance           // validated base
//                   << std::setw(10) << met.min
//                   << std::setw(10) << met.max
//                   << std::setw(14) << reduceDist
//                   << std::setw(16) << baseShareDist
//                   << std::setw(16) << baseShareRatio
//                   << std::setw(12) << res.base
//                   << std::setw(12) << res.delta
//                   << std::setw(14) << res.value
//                   << '\n';
//     }
//
//     std::cout << std::string(140, '-') << '\n';
//     std::cout << std::format("Total: {:.3f} px (expected {:.1f})\n", total, rootBase);
//     std::cout << std::format("Accumulators: Base={:.1f}, SolidifyBase ={:.1f}, BaseShareRatio={:.4f}, ShareRatio={:.1f}\n",
//                              accumulators.accumulateBaseDistance,
//                              accumulators.accumulateSolidifyBase,
//                              accumulators.accumulateBaseShareRatio,
//                              accumulators.accumulateShareRatio);
// #pragma endregion //Print Result
//
//     return 0;
// }

#include <format>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>

struct DiscadeltaSegment {
    std::string name{"None"};
    float base{0.0f};
    float expandDelta{0.0f};
    float distance{0.0f};
    float offset{0.0f};
    size_t order{0};
};

struct DiscadeltaSegmentConfig {
    std::string name{"None"};
    float base{0.0f};
    float compressRatio{0.0f};
    float expandRatio{0.0f};
    float min{0.0f};
    float max{0.0f};
    size_t order{0};
};

struct DiscadeltaPreComputeMetrics {
    float inputDistance{};
    std::vector<float> compressCapacities{};
    std::vector<float> compressSolidifies{};
    std::vector<float> baseDistances{};
    std::vector<float> expandRatios{};
    std::vector<float> minDistances{};
    std::vector<float> maxDistances{};

    float accumulateBaseDistance{0.0f};
    float accumulateCompressSolidify{0.0f};
    float accumulateExpandRatio{0.0f};

    std::vector<DiscadeltaSegment*> segments;

    std::vector<size_t> compressPriorityIndies;
    std::vector<size_t> expandPriorityIndies;

    DiscadeltaPreComputeMetrics() = default;
    ~DiscadeltaPreComputeMetrics() = default;

    explicit DiscadeltaPreComputeMetrics(const size_t segmentCount, const float& rootBase) : inputDistance(rootBase) {
        compressCapacities.reserve(segmentCount);
        compressSolidifies.reserve(segmentCount);
        baseDistances.reserve(segmentCount);
        expandRatios.reserve(segmentCount);
        minDistances.reserve(segmentCount);
        maxDistances.reserve(segmentCount);
        segments.reserve(segmentCount);

        compressPriorityIndies.reserve(segmentCount);
        expandPriorityIndies.reserve(segmentCount);
    }
};

using DiscadeltaSegmentsHandler = std::vector<std::unique_ptr<DiscadeltaSegment>>;

constexpr auto MakeDiscadeltaContext = [](const std::vector<DiscadeltaSegmentConfig>& configs, const float inputDistance) -> std::tuple<DiscadeltaSegmentsHandler, DiscadeltaPreComputeMetrics, bool>{
    const float validatedInputDistance = std::max(0.0f, inputDistance);
    const size_t segmentCount = configs.size();

    DiscadeltaSegmentsHandler segments;
    segments.reserve(segmentCount);

    DiscadeltaPreComputeMetrics preComputeMetrics(segmentCount, validatedInputDistance);

    float compressPriorityLowestValue = std::numeric_limits<float>::max();
    float expandPriorityLowestValue{0.0f};

    for (size_t i = 0; i < segmentCount; ++i) {
        const auto& [name, rawBase, rawCompressRatio, rawExpandRatio, rawMin, rawMax, rawOrder] = configs[i];

        // --- INPUT VALIDATION ---
        const float minVal = std::max(0.0f, rawMin);
        const float maxVal = std::max(minVal, rawMax);
        const float baseVal = std::clamp(rawBase, minVal, maxVal);

        const float compressRatio = std::max(0.0f, rawCompressRatio);
        const float expandRatio   = std::max(0.0f, rawExpandRatio);

        // --- COMPRESS METRICS ---
        const float compressCapacity = baseVal * compressRatio;
        const float compressSolidify = std::max (0.0f, baseVal - compressCapacity);

        // --- STORE PRE-COMPUTE ---
        preComputeMetrics.compressCapacities.push_back(compressCapacity);
        preComputeMetrics.compressSolidifies.push_back(compressSolidify);
        preComputeMetrics.baseDistances.push_back(baseVal);
        preComputeMetrics.expandRatios.push_back(expandRatio);
        preComputeMetrics.minDistances.push_back(minVal);
        preComputeMetrics.maxDistances.push_back(maxVal);

        preComputeMetrics.accumulateBaseDistance += baseVal;
        preComputeMetrics.accumulateCompressSolidify += compressSolidify;
        preComputeMetrics.accumulateExpandRatio += expandRatio;

        // --- CREATE OWNED SEGMENT ---
        auto seg = std::make_unique<DiscadeltaSegment>();
        seg->name = name;
        seg->order = rawOrder;
        seg->base = baseVal;
        seg->distance = baseVal;
        seg->expandDelta = 0.0f;

        preComputeMetrics.segments.push_back(seg.get());
        segments.push_back(std::move(seg));

        // --- COMPRESS PRIORITY ---
        const float greaterMin = std::max(compressSolidify, minVal);
        const float compressPriorityValue = std::max(0.0f, baseVal - greaterMin);

        if (compressPriorityValue <= compressPriorityLowestValue) {
            preComputeMetrics.compressPriorityIndies.insert(preComputeMetrics.compressPriorityIndies.begin(), i);
            compressPriorityLowestValue = compressPriorityValue;
        }
        else {
            preComputeMetrics.compressPriorityIndies.push_back(i);
        }

        // --- EXPAND PRIORITY ---
        const float expandPriorityValue = std::max(0.0f, maxVal - baseVal);
        if (expandPriorityValue >= expandPriorityLowestValue) {
            preComputeMetrics.expandPriorityIndies.push_back(i);
            expandPriorityLowestValue = expandPriorityValue;
        }
        else {
            preComputeMetrics.expandPriorityIndies.insert(preComputeMetrics.expandPriorityIndies.begin(), i);
        }
    }

    const bool processingCompression = validatedInputDistance < preComputeMetrics.accumulateBaseDistance;

    return { std::move(segments), std::move(preComputeMetrics), processingCompression };
};

constexpr float DiscadeltaScaler(const float& distance, const float& accumulateFactor, const float& factor) {
    return distance <= 0.0f || accumulateFactor <= 0.0f || factor <= 0.0f ? 0.0f : distance / accumulateFactor * factor;
}

constexpr void DiscadeltaCompressing(const DiscadeltaPreComputeMetrics& preComputeMetrics) {
    float cascadeCompressDistance = preComputeMetrics.inputDistance;
    float cascadeBaseDistance = preComputeMetrics.accumulateBaseDistance;
    float cascadeCompressSolidify = preComputeMetrics.accumulateCompressSolidify;

    for (size_t i = 0; i < preComputeMetrics.segments.size(); ++i) {
        const size_t index = preComputeMetrics.compressPriorityIndies[i];
        DiscadeltaSegment* seg = preComputeMetrics.segments[index];
        if (seg == nullptr) break;

        const float remainDist = cascadeCompressDistance - cascadeCompressSolidify;
        const float remainCap = cascadeBaseDistance - cascadeCompressSolidify;
        const float& solidify = preComputeMetrics.compressSolidifies[index];

        const float compressBaseDistance = DiscadeltaScaler(remainDist, remainCap, preComputeMetrics.compressCapacities[index]) + solidify;

        const float& min = preComputeMetrics.minDistances[index];
        const float clampedDist = std::max(compressBaseDistance, min);

        seg->base = seg->distance = clampedDist;
        cascadeCompressDistance -= clampedDist;
        cascadeCompressSolidify -= solidify;
        cascadeBaseDistance -= preComputeMetrics.baseDistances[index];
    }
}

constexpr void DiscadeltaExpanding(const DiscadeltaPreComputeMetrics& preComputeMetrics) {
    float cascadeExpandDelta = std::max(preComputeMetrics.inputDistance - preComputeMetrics.accumulateBaseDistance, 0.0f);
    float cascadeExpandRatio = preComputeMetrics.accumulateExpandRatio;

    if (cascadeExpandDelta <= 0.0f) return;

    for (size_t i = 0; i < preComputeMetrics.segments.size(); ++i) {
        const size_t index = preComputeMetrics.expandPriorityIndies[i];
        DiscadeltaSegment* seg = preComputeMetrics.segments[index];
        const float& base = preComputeMetrics.baseDistances[index];
        const float& ratio = preComputeMetrics.expandRatios[index];

        const float expandDelta = DiscadeltaScaler(cascadeExpandDelta, cascadeExpandRatio, ratio);
        // const float expandDelta = cascadeExpandRatio <= 0.0f || ratio <= 0.0f ? 0.0f :
        //                            cascadeExpandDelta / cascadeExpandRatio * ratio;

        // Apply MAX Constraint
        const float maxDelta = std::max(0.0f, preComputeMetrics.maxDistances[index] - base);
        const float clampedDelta = std::min(expandDelta, maxDelta);

        seg->expandDelta = clampedDelta;
        seg->distance = base + clampedDelta;
        cascadeExpandDelta -= clampedDelta;
        cascadeExpandRatio -= ratio;
    }
}

constexpr void DiscadeltaPlacing(DiscadeltaPreComputeMetrics& preComputeMetrics) {
    // 1. Sort segments by their desired visual 'order'
    std::sort(preComputeMetrics.segments.begin(), preComputeMetrics.segments.end(), [](const auto& a, const auto& b) {
        return a->order < b->order;
    });

    // 2. Linear accumulation of offsets
    float currentOffset = 0.0f;
    for (auto* seg : preComputeMetrics.segments) {
        if (!seg) continue;

        // The segment starts at the current-accumulated distance
        seg->offset = currentOffset;

        // Push the offset forward by the segment's resolved size
        currentOffset += seg->distance;
    }
}

void Debugger(const DiscadeltaSegmentsHandler& segmentDistances, const DiscadeltaPreComputeMetrics &preComputeMetrics) {
    std::cout <<"=== Discadelta Layout: Metrics & Final Distribution ===" << std::endl;
    std::cout << std::format("Input distance: {}", preComputeMetrics.inputDistance)<< std::endl;

    // Table header
    std::cout << std::left
              << "|"
              << std::setw(10) << "Segment"
              << "|"
              << std::setw(20) << "Base"
              << "|"
              << std::setw(15) << "Delta"
              << "|"
              << std::setw(15) << "Distance"
              << "|"
              << std::setw(15) << "Order"
              << "|"
              << std::setw(15) << "Offset"
              << "|"
              << std::endl;

    std::cout << std::left
                   << "|"
                   << std::string(10, '-')
                   << "|"
                   << std::string(20, '-')
                   << "|"
                   << std::string(15, '-')
                   << "|"
                   << std::string(15, '-')
                   << "|"
                   << std::string(15, '-')
                   << "|"
                   << std::string(15, '-')
                   << "|"
                   << std::endl;

    float total{0.0f};
    for (const auto & res : segmentDistances) {
        total += res->distance;

        std::cout << std::left<< "|"<< std::setw(10) << res->name<< "|"
                  << std::setw(20) << std::format("{:.3f}",res->base)
                  << "|"
                  << std::setw(15) << std::format("{:.3f}",res->expandDelta)
                  << "|"
                  << std::setw(15) << std::format("{:.3f}",res->distance)
                  << "|"
                  << std::setw(15) << std::format("{}",res->order)
                  << "|"
                  << std::setw(15) << std::format("{:.3f}",res->offset)
                  << "|"
                  << std::endl;
    }
}

constexpr void SetSegmentOrder(DiscadeltaPreComputeMetrics& preComputeMetrics, std::string_view name, const size_t order) {
  const auto it = std::ranges::find_if(
      preComputeMetrics.segments, [&](const auto& seg) { return seg->name == name; });
    if (it != preComputeMetrics.segments.end()) (*it)->order = order;
}

int main() {
    std::vector<DiscadeltaSegmentConfig> segmentConfigs{
          {"Segment_1", 200.0f, 0.7f, 0.1f, 0.0f, 100.0f, 2},
          {"Segment_2", 200.0f, 1.0f, 1.0f, 300.0f, 800.0f, 1},
          {"Segment_3", 150.0f, 0.0f, 2.0f, 0.0f, 200.0f, 3},
          {"Segment_4", 350.0f, 0.3f, 0.5f, 50.0f, 300.0f, 0}};

    constexpr float rootDistance = 800.0f;
    auto [segmentDistances, preComputeMetrics, processingCompression] = MakeDiscadeltaContext(segmentConfigs, rootDistance);

    if (processingCompression) {
        DiscadeltaCompressing(preComputeMetrics);
    }
    else {
        DiscadeltaExpanding(preComputeMetrics);
    }

    DiscadeltaPlacing(preComputeMetrics);

    Debugger(segmentDistances, preComputeMetrics);
    //the debugger is slow we sleep for 2 seconds before trigger another log
    std::this_thread::sleep_for(std::chrono::seconds(2));

    SetSegmentOrder(preComputeMetrics, "Segment_1", 3);
    SetSegmentOrder(preComputeMetrics, "Segment_3", 2);

    DiscadeltaPlacing(preComputeMetrics);

    Debugger(segmentDistances, preComputeMetrics);

    return 0;
}
