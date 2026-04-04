#include <exception>
#include <iostream>
#include <thread>
#include <filesystem>
#include <string>

import ufox_engine_lib;
import ufox_engine_core;
import ufox_gui_lib;
import ufox_gui_core;
import ufox_geometry_lib;
import ufox_geometry_core;
import ufox_render_lib;
import ufox_render_core;
import ufox_lib;



int main() {
  try {
    auto window = ufox::engine::CreateUFoxWindow("UFox", 800, 800);
    ufox::geometry::MeshManager meshManager{window->gpuResource};
    meshManager.init();
    ufox::render::TextureManager textureManager{window->gpuResource};
    textureManager.init();


    ufox::gui::Document doc(window.get(), &meshManager, &textureManager);
    ufox::engine::Camera mainCamera{};


    window->registerStartEventHandlers([](void*){ std::cout << "STARTED"<< std::endl; }, nullptr);
    window->registerResizeEventHandlers([](const float& w, const float& h, void* user) {
      auto* camera = static_cast<ufox::engine::Camera*>(user);
      ufox::engine::CalculateAspectRatio(w,h, camera->aspectRatio);
    }, &mainCamera);



    window->run();


  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
