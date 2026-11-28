#include <iostream>
#include <memory>

import ufox_engine_app;

int main() {
  try {
    std::unique_ptr<ufox::UFoxEngineApp> engine = std::make_unique<ufox::UFoxEngineApp>();
    if (engine == nullptr) throw std::runtime_error("Failed to create engine");
    engine->execution();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}