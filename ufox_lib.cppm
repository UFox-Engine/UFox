module;

#include <vector>
#include <optional>
#include <iostream>
#include <string>
#include <format>
#include <vulkan/vulkan_raii.hpp>
export module ufox_lib;

export namespace ufox {
    namespace debug {
        enum class LogLevel {
            INFO,
            WARNING,
            ERROR
        };

        // Moved source_location parameter to the beginning
        template<typename... Args>
        void log(LogLevel level,
                const std::string& format_str,
                Args&&... args) {
            std::string level_str;
            switch (level) {
                case LogLevel::INFO:    level_str = "INFO"; break;
                case LogLevel::WARNING: level_str = "WARNING"; break;
                case LogLevel::ERROR:   level_str = "ERROR"; break;
            }

            // Format the message using std::format
            std::string message = std::vformat(format_str, std::make_format_args(args...));

            std::cout << std::format("[{}] {}\n",
                level_str,
                message);
        }
    }




    struct Panel {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        std::vector<Panel*> rows;
        std::vector<Panel*> columns;

        void addRowChild(Panel* child) {
            rows.push_back(child);
        }

        void addColumnChild(Panel* child) {
            columns.push_back(child);
        }

        void print() const {
            debug::log(debug::LogLevel::INFO, "Panel: x={}, y={}, width={}, height={}", x, y, width, height);
        }
    };


}