import hello_uFox;
import ufox_lib;
import ufox_windowing;

#include <SDL3/SDL.h>
#include <iostream>


int main() {


    try {
        using ufox::windowing::sdl::WindowFlag;
        ufox::windowing::sdl::UFoxWindow window("UFoxEngine Test",
            WindowFlag::Vulkan | WindowFlag::Resizable);

        window.Show();

        SDL_Event event;
        bool running = true;

        while (running) {
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_EVENT_QUIT: {
                        running = false;
                        break;
                    }
                    default: {}
                }
            }
        }

        return 0;
    } catch (const ufox::windowing::sdl::SDLException& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
