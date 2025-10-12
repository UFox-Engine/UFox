
#include <exception>
#include <iostream>

import ufox_engine;


int main() {

    try {
        ufox::Engine ide;
        ide.Init();
        ide.Run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;


}
