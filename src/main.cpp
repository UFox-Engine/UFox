#include <iostream>

import ufox_engine;

int main() {
  try {
    auto engine = ufox::CreateUFoxEngine("UFox", 800, 800);   // or ufox::engine::CreateUFoxEngine
    engine->run();          // preRun → onRun → postRun
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}