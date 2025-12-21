
#include <exception>
#include <iostream>
#include <thread>

#include "vulkan/vulkan.hpp"

import ufox_engine;


int main() {

    try {
        ufox::UFoxEngine ide;
        ide.Init();
        ide.Run();

     } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}



// #include <iostream>
// #include <vector>
// #include <format>
// #include <iomanip>  // for std::setw, std::fixed, std::setprecision
//
// struct DiscadeltaSegment {
//     float base;
//     float delta;
//     float value;
// };
//
// struct DiscadeltaSegmentConfig {
//     float base;
//     float reduceRatio;
//     float shareRatio;
// };
//
// int main()
// {
//     std::vector<DiscadeltaSegmentConfig> segmentConfigs{
//         {200.0f, 0.7f, 0.1f},
//         {300.0f, 1.0f, 1.0f},
//         {150.0f, 1.0f, 2.0f},
//         {250.0f, 0.3f, 0.5f}
//     };
//
//     const size_t segmentCount = segmentConfigs.size();
//
//     std::vector<DiscadeltaSegment> segmentDistances{};
//     segmentDistances.reserve(segmentCount);
//
// #pragma region // Prepare Compute Context
//     constexpr float rootBase = 800.0f;
//
//     std::vector<float> reduceDistances{};
//     reduceDistances.reserve(segmentCount);
//
//     std::vector<float> baseShareDistances{};
//     baseShareDistances.reserve(segmentCount);
//
//     std::vector<float> baseShareRatios{};
//     baseShareRatios.reserve(segmentCount);
//
//     std::vector<float> shareRatios{};
//     shareRatios.reserve(segmentCount);
//
//     float accumulateReduceDistance{0.0f};
//     float accumulateBaseShareRatio{0.0f};
//
//     float accumulateBaseDistance{0.0f};
//     float accumulateShareRatio{0.0f};
//
//     for (size_t i = 0; i < segmentCount; ++i) {
//         const auto &[base, reduceRatio, shareRatio] = segmentConfigs[i];
//
//         const float reduceDistance = base * (1.0f - reduceRatio);
//         const float baseShareDistance = base * reduceRatio;
//         const float baseShareRatio = baseShareDistance / 100.0f;
//
//         accumulateReduceDistance += reduceDistance;
//         accumulateBaseShareRatio += baseShareRatio;
//
//         accumulateBaseDistance += base;
//         accumulateShareRatio += shareRatio;
//
//         reduceDistances.push_back(reduceDistance);
//         baseShareDistances.push_back(baseShareDistance);
//         baseShareRatios.push_back(baseShareRatio);
//
//         shareRatios.push_back(shareRatio);
//     }
// #pragma endregion //Prepare Compute Context
//
// #pragma region //Compute Segment Base Distance
//
//     float remainShareDistance = rootBase;
//     float total{0.0f};
//
//     for (size_t i = 0; i < segmentCount; ++i) {
//         DiscadeltaSegment segment{};
//
//         if (rootBase <= accumulateBaseDistance) {
//             const float remainReduceDistance = remainShareDistance - accumulateReduceDistance;
//             const float shareRatio = baseShareRatios[i];
//             const float baseDistance = remainReduceDistance <= 0.0f || accumulateBaseShareRatio <= 0.0f || shareRatio <= 0.0f ? 0.0f :
//                 remainReduceDistance / accumulateBaseShareRatio * shareRatio + reduceDistances[i];
//
//             accumulateReduceDistance -= reduceDistances[i];
//             remainShareDistance -= baseDistance;
//             accumulateBaseShareRatio -= shareRatio;
//
//             segment.base = baseDistance;
//             segment.value = baseDistance;
//             total += baseDistance;
//         }
//         else {
//             const float baseDistance = segmentConfigs[i].base;
//             segment.base = baseDistance;
//             segment.value = baseDistance;
//             remainShareDistance -= baseDistance;
//         }
//
//         segmentDistances.push_back(segment);
//     }
//
// #pragma endregion //Compute Segment Base Distance
//
// #pragma region //Compute Segment Delta
//
//     if (remainShareDistance > 0.0f) {
//         float remainShareRatio = accumulateShareRatio;
//
//         for (size_t i = 0; i < segmentCount; ++i) {
//             const float shareDelta = remainShareDistance / remainShareRatio * shareRatios[i];
//
//             segmentDistances[i].delta = shareDelta;
//             segmentDistances[i].value += shareDelta;
//
//             remainShareDistance -= shareDelta;
//             remainShareRatio -= shareRatios[i];
//         }
//     }
//
// #pragma endregion //Compute Segment Delta
//
// #pragma region //Print Result
//     std::cout << "\n=== Dynamic Base Segment (Underflow Handling) ===\n";
//     std::cout << std::format("Root distance: {:.1f}\n\n", rootBase);
//
//     // Table header
//     std::cout << std::left
//               << std::setw(8) << "Segment"
//               << std::setw(14) << "reduceDist"
//               << std::setw(16) << "baseShareDist"
//               << std::setw(16) << "baseShareRatio"
//               << std::setw(14) << "base"
//               << std::setw(14) << "delta"
//               << std::setw(14) << "distance"
//               << '\n';
//
//     std::cout << std::string(100, '-') << '\n';
//
//     for (size_t i = 0; i < segmentCount; ++i) {
//         const auto& seg = segmentConfigs[i];
//         const auto& res = segmentDistances[i];
//
//         const float reduceDistance = seg.base * (1.0f - seg.reduceRatio);
//         const float baseShareDistance = seg.base * seg.reduceRatio;
//         const float baseShareRatio = baseShareDistance / 100.0f;
//
//         std::cout << std::fixed << std::setprecision(3)
//                   << std::setw(8) << (i + 1)
//                   << std::setw(14) << reduceDistance
//                   << std::setw(16) << baseShareDistance
//                   << std::setw(16) << baseShareRatio
//                   << std::setw(14) << res.base
//                   << std::setw(14) << res.delta
//                   << std::setw(14) << res.value
//                   << '\n';
//     }
//
//     std::cout << std::string(100, '-') << '\n';
//     std::cout << std::format("Total: {:.3f} (expected 800.0)\n\n", total);
// #pragma endregion //Print Result
//
//     return 0;
// }