#include <exception>
#include <iostream>
#include <thread>

import ufox_engine_lib;
import ufox_engine_core;
import ufox_gui_lib;
import ufox_gui_core;
import ufox_geometry_core;
import ufox_geometry_lib;

int main() {
  try {
    auto window = ufox::engine::CreateUFoxWindow("UFox", 800, 800);
    ufox::geometry::MeshManager meshManager{window->gpuResource};
    ufox::gui::Document doc(window.get(), &meshManager);
    // ufox::geometry::DefaultMeshesResources default_meshes_resources = ufox::geometry::CreateDefaultMeshResources(meshManager);
    // auto quadIt = default_meshes_resources.find("quad");
    // if (quadIt != default_meshes_resources.end()) {
    //   const ufox::engine::ContentID * quadId = quadIt->second;
    //   meshManager.useMesh(*quadId, window->gpuResource);
    // }
    window->addStartEventHandlers([](void*){ std::cout << "STARTED"<< std::endl; }, nullptr);

    window->run();


  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
