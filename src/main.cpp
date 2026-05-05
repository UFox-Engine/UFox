#include <exception>
#include <iostream>
#include <thread>
#include <filesystem>
#include <string>
#include <vulkan/vulkan_raii.hpp>
#include <msdf-atlas-gen/msdf-atlas-gen.h>

import ufox_engine_lib;
import ufox_engine_core;
import ufox_gui_lib;
import ufox_gui_core;
import ufox_geometry_lib;
import ufox_geometry_core;
import ufox_render_lib;
import ufox_render_core;
import ufox_font_lib;
import ufox_font_core;
import ufox_lib;


  void SaveGlyphTypeUnicodeRangesToFile(const std::string& filename = "glyph_unicode_ranges.txt") noexcept {
    std::ofstream file(filename);
    if (!file.is_open()) {
      std::cerr << "Failed to open file for writing: " << filename << std::endl;
      return;
    }

    auto writeRange = [&](const char* name, uint32_t start, uint32_t end) {
      file << name << ":\n";
      for (uint32_t c = start; c <= end; ++c) {
        file << "0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << c;
        if (c != end) file << ",";
      }
      file << "\n\n";
    };

    file << "=== GlyphType Unicode Ranges ===\n\n";

    file << "Latin:\n";
    writeRange("Basic Latin", 0x0000, 0x007F);
    writeRange("Latin-1 Supplement", 0x0080, 0x00FF);

    file << "Cyrillic:\n";
    writeRange("Cyrillic", 0x0400, 0x04FF);
    writeRange("Cyrillic Supplement", 0x0500, 0x052F);

    file << "Greek:\n";
    writeRange("Greek and Coptic", 0x0370, 0x03FF);

    file << "Hanzi (Chinese):\n";
    writeRange("CJK Unified Ideographs", 0x4E00, 0x9FFF);
    writeRange("CJK Extension A", 0x3400, 0x4DBF);

    file << "Kanji (Japanese):\n";
    writeRange("Hiragana + Katakana", 0x3040, 0x30FF);
    writeRange("CJK Unified Ideographs", 0x4E00, 0x9FFF);
    writeRange("CJK Extension A", 0x3400, 0x4DBF);

    file << "Hangul (Korean):\n";
    writeRange("Hangul Syllables", 0xAC00, 0xD7AF);
    writeRange("Hangul Jamo", 0x1100, 0x11FF);

    file << "Thai:\n";
    writeRange("Thai", 0x0E01, 0x0E5B);

    file << "BuiltIn (ASCII fallback):\n";
    writeRange("ASCII", 0x0020, 0x007F);

    file << "=== End of GlyphType Unicode Ranges ===\n";

    file.close();
    std::cout << "Unicode ranges saved to: " << filename << std::endl;
  }

int main() {
  try {
    auto window = ufox::engine::CreateUFoxWindow("UFox", 800, 800);
     ufox::geometry::MeshManager meshManager{*window,window->gpuResource};
     ufox::render::TextureManager textureManager{*window,window->gpuResource};
     ufox::font::GlyphManager glyph_manager{*window,textureManager};

     ufox::gui::Document doc(window.get(), &meshManager, &textureManager);
     ufox::engine::Camera mainCamera{};

     window->registerCallbackEvent<ufox::engine::EventType::eStart>([](void*){ std::cout << "STARTED"<< std::endl; }, nullptr);
     window->registerCallbackEvent<ufox::engine::EventType::eResize>([](const float& w, const float& h, void* user) {
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
