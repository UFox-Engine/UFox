#include <exception>
#include <iostream>
#include <thread>
#include <filesystem>

import ufox_engine_lib;
import ufox_engine_core;
import ufox_gui_lib;
import ufox_gui_core;
import ufox_geometry_core;
import ufox_geometry_lib;
import ufox_lib;

int main() {
  try {
    auto window = ufox::engine::CreateUFoxWindow("UFox", 800, 800);
    ufox::geometry::MeshManager meshManager{window->gpuResource};
    meshManager.Init();

    ufox::gui::Document doc(window.get(), &meshManager);
    ufox::engine::Camera mainCamera{};


    window->addStartEventHandlers([](void*){ std::cout << "STARTED"<< std::endl; }, nullptr);
    window->addResizeEventHandlers([](const float& w, const float& h, void* user) {
      auto* camera = static_cast<ufox::engine::Camera*>(user);
      ufox::engine::CalculateAspectRatio(w,h, camera->aspectRatio);
    }, &mainCamera);

    std::filesystem::path modelPath = "res/meshes/boxu.glb";

    auto glbBytes = window->ReadFileBinary(modelPath.string());

    if (glbBytes.empty())
    {
      ufox::debug::log(ufox::debug::LogLevel::eError, "Failed to load glTF file: {}", modelPath.string());
      return EXIT_FAILURE;
    }

    ufox::debug::log(ufox::debug::LogLevel::eInfo, "Loaded glTF file ({} bytes): {}", glbBytes.size(), modelPath.string());

    if (auto* cid = CreateMeshFromFirstGlbPrimitive(
                meshManager,
                std::span<const std::byte>{glbBytes},
                "BoxFromGlTF", modelPath))
    {
      ufox::debug::log(ufox::debug::LogLevel::eInfo, "Successfully parsed glTF mesh! ID = {}", cid->index);





      // ... later in your rendering loop, use boxMesh.mesh->vertexBuffer etc.
    }
    else
    {
      ufox::debug::log(ufox::debug::LogLevel::eWarning, "glTF parsing failed (likely missing data or unsupported format)");
    }

    window->run();


  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
