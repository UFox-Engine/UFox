//
// Created by b-boy on 05.10.2025.
//
module;
#include <vector>  // For std::vector
#include <print>
export module ufox_lib;



export namespace ufox {

    struct Panel {
        int x = 0.0f;
        int y = 0.0f;
        int width = 0.0f;
        int height = 0.0f;

        std::vector<Panel*> rows;
        std::vector<Panel*> columns;

        // Optional: Method to add a row child panel
        void addRowChild(Panel* child) {
            rows.push_back(child);
        }

        // Optional: Method to add a column child panel
        void addColumnChild(Panel* child) {
            columns.push_back(child);
        }

        void print() {

            std::println("Panel: x: {}, y: {}, width: {}, height: {}\n", x, y, width, height);
        }
    };

}