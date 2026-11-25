//
// Created by Puwiwad on 04.10.2025.
//
module;

#include <glm/glm.hpp>
#include <iostream>
#include <memory>


export module ufox_engine;

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

using namespace ufox::engine;
export namespace ufox {

    class ufox_engine {
    public:
        explicit ufox_engine(const std::string& title, uint32_t width, uint32_t height): window(CreateUFoxWindow(title, width, height)){}

        void preRun() {
            // create only what the test needs
            meshManager.emplace(*window);
            textureManager.emplace(*window);
            glyphManager.emplace(*window, *textureManager);
            document.emplace(*window, *meshManager, *glyphManager, &*textureManager);

            // eStart
            window->registerCallbackEvent<EventType::eStart>(
                [](void*) { std::cout << "STARTED" << std::endl; }, nullptr);

            // eResize → update camera aspect
            window->registerCallbackEvent<EventType::eResize>(
                [](const float& w, const float& h, void* user) {
                    auto* camera = static_cast<Camera*>(user);
                    CalculateAspectRatio(w, h, camera->aspectRatio);
                }, &mainCamera);

            // ePostSystemInit → build test UI
            window->registerCallbackEvent<EventType::ePostSystemInit>(
                [](const float& w, const float& h, void* user) {
                    auto* doc = static_cast<gui::UIDocument*>(user);
                    if (!doc || !doc->rootElement) return;

                    auto* v11 = doc->rootElement->createChild<gui::VisualElement>(
                        KeyID{"v11"}, "v11");

                    v11->style.backgroundColor = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
                    v11->style.flexCompress    = 1.0f;
                    v11->style.flexExpand      = 1.0f;
                    v11->style.maxWidth        = geometry::discadelta::Length{
                        geometry::discadelta::LengthMode::eFlat, 150};
                    v11->style.alignSelf       = geometry::discadelta::AlignMode::eStretch;
                    v11->style.marginLeft = v11->style.marginRight =
                        v11->style.marginTop = v11->style.marginBottom = 5;

                    v11->style.borderBottomColor = {0.0f, 0.0f, 1.0f, 1.0f};
                    v11->style.borderBottomWidth = 5;
                    v11->style.borderBottomLeftRadius  = 25;
                    v11->style.borderBottomRightRadius = 17;

                    v11->style.borderTopColor = {1.0f, 0.0f, 0.0f, 1.0f};
                    v11->style.borderTopWidth = 18;
                    v11->style.borderTopLeftRadius  = 25;
                    v11->style.borderTopRightRadius = 20;

                    v11->style.borderLeftColor  = {1.0f, 0.0f, 1.0f, 1.0f};
                    v11->style.borderLeftWidth  = 12;
                    v11->style.borderRightColor = {1.0f, 1.0f, 0.0f, 1.0f};
                    v11->style.borderRightWidth = 8;

                    v11->registerCallbackEvent<gui::ElementEventType::PointerEnter>(
                        [](gui::ElementPointerEvent&, void* user) {
                            auto* el = static_cast<gui::VisualElement*>(user);
                            el->style.backgroundColor = {1, 0, 0, 1};
                            el->markStyleChanged();
                        }, v11);

                    v11->registerCallbackEvent<gui::ElementEventType::PointerLeave>(
                        [](gui::ElementPointerEvent&, void* user) {
                            auto* el = static_cast<gui::VisualElement*>(user);
                            el->style.backgroundColor = {1, 1, 1, 1};
                            el->markStyleChanged();
                        }, v11);

                    v11->updateMetrics();

                    auto* title = doc->rootElement->createChild<gui::Label>(
                        KeyID{"title"}, "title", "Hallo, gg Welt!", 24.0f);
                    title->setColor({1, 1, 1, 1});

                    auto v3 = std::make_unique<gui::VisualElement>("v3");
                    doc->rootElement->add(std::move(v3));
                }, &document);
        }

        void onRun() {
            window->run();
        }

        void postRun() {
            if (document) {
                window->unregisterCallbackEvent(EventType::eResize, &mainCamera);
                window->unregisterCallbackEvent(EventType::ePostSystemInit, &*document);
            }
            // optionals destroy themselves in reverse order
        }

        void run() {
            preRun();
            onRun();
            postRun();
        }

        // accessors that return optional references / pointers
        [[nodiscard]] UFoxWindow& getWindow() { return *window; }

        [[nodiscard]] std::optional<geometry::MeshManager>& getMeshManager()     { return meshManager; }
        [[nodiscard]] std::optional<render::TextureManager>& getTextureManager() { return textureManager; }
        [[nodiscard]] std::optional<font::GlyphManager>& getGlyphManager()       { return glyphManager; }
        [[nodiscard]] std::optional<gui::UIDocument>& getUIDocument()                { return document; }
        [[nodiscard]] Camera& getMainCamera()                                    { return mainCamera; }

    private:
        std::unique_ptr<UFoxWindow>                  window{};
        std::optional<geometry::MeshManager>         meshManager{};
        std::optional<render::TextureManager>        textureManager{};
        std::optional<font::GlyphManager>            glyphManager{};
        std::optional<gui::UIDocument>               document{};
        Camera                                       mainCamera{};   // cheap, can stay always present
    };

    [[nodiscard]] std::unique_ptr<ufox_engine> CreateUFoxEngine(const std::string& title,uint32_t width,uint32_t height){
    return std::make_unique<ufox_engine>(title, width, height);
}
}