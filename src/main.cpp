
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
