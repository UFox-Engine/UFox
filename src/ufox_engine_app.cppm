//
// Created by Puwiwad on 04.10.2025.
//
module;

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <iostream>
#include <memory>
#include <ranges>
#include <vulkan/vulkan_raii.hpp>

export module ufox_engine_app;

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
import ufox_beta;
import ufox_key;
import ufox_core;

using namespace ufox::engine;
export namespace ufox {

inline const auto UFOX_ENGINE_ID = KeyID{"UFox"};
inline const auto UFOX_ENGINE_VERSION = KeyID{"0.1.0"};

inline const auto UFOX_MAIN_WINDOW_ID = KeyID{"UFox_Main_Window"};

    class UFoxEngineApp {
    public:
        explicit UFoxEngineApp() = default;

        void init() {
            appData.emplace(UFOX_ENGINE_ID, UFOX_ENGINE_VERSION);
            mainWindow = CreateUFoxWindow(appData->getEngineTitle(), appData->screenSize.x, appData->screenSize.y);
            assert(appData->addSystem(UFOX_MAIN_WINDOW_ID, mainWindow.get()) && "Failed to add main window system");


            meshManager.emplace(*mainWindow);
            assert(appData->addSystem(*meshManager->getId(), &meshManager.value()) && "Failed to add mesh manager system");
            textureManager.emplace(*mainWindow);
            assert(appData->addSystem(*textureManager->getId(), &textureManager.value()) && "Failed to add texture manager system");
            glyphManager.emplace(*mainWindow, *textureManager);
            assert(appData->addSystem(*glyphManager->getId(), &glyphManager.value()) && "Failed to add glyph manager system");
            //document.emplace(*mainWindow, *meshManager, *glyphManager, &*textureManager);


            // ePostSystemInit → build test UI
            // mainWindow->registerCallbackEvent<EventType::ePostSystemInit>(
            //     [](const float& w, const float& h, void* user) {
            //         auto* doc = static_cast<gui::UIDocument*>(user);
            //         if (!doc || !doc->rootElement) return;
            //
            //         auto* v11 = doc->rootElement->createChild<gui::VisualElement>(
            //             KeyID{"v11"}, "v11");
            //
            //         v11->style.backgroundColor = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
            //         v11->style.flexCompress    = 1.0f;
            //         v11->style.flexExpand      = 1.0f;
            //         v11->style.maxWidth        = geometry::discadelta::Length{
            //             geometry::discadelta::LengthMode::eFlat, 150};
            //         v11->style.alignSelf       = geometry::discadelta::AlignMode::eStretch;
            //         v11->style.marginLeft = v11->style.marginRight =
            //             v11->style.marginTop = v11->style.marginBottom = 0;
            //
            //         v11->style.borderBottomColor = {0.0f, 0.0f, 1.0f, 1.0f};
            //         v11->style.borderBottomWidth = 0;
            //         v11->style.borderBottomLeftRadius  = 25;
            //         v11->style.borderBottomRightRadius = 17;
            //
            //         v11->style.borderTopColor = {1.0f, 0.0f, 0.0f, 1.0f};
            //         v11->style.borderTopWidth = 0;
            //         v11->style.borderTopLeftRadius  = 25;
            //         v11->style.borderTopRightRadius = 20;
            //
            //         v11->style.borderLeftColor  = {1.0f, 0.0f, 1.0f, 1.0f};
            //         v11->style.borderLeftWidth  = 0;
            //         v11->style.borderRightColor = {1.0f, 1.0f, 0.0f, 1.0f};
            //         v11->style.borderRightWidth = 0;
            //
            //         v11->registerCallbackEvent<gui::ElementEventType::PointerEnter>(
            //             [](gui::ElementPointerEvent&, void* user) {
            //                 auto* el = static_cast<gui::VisualElement*>(user);
            //                 el->style.backgroundColor = {1, 0, 0, 1};
            //                 el->markStyleChanged();
            //             }, v11);
            //
            //         v11->registerCallbackEvent<gui::ElementEventType::PointerLeave>(
            //             [](gui::ElementPointerEvent&, void* user) {
            //                 auto* el = static_cast<gui::VisualElement*>(user);
            //                 el->style.backgroundColor = {1, 1, 1, 1};
            //                 el->markStyleChanged();
            //             }, v11);
            //
            //         v11->updateMetrics();
            //
            //         auto* title = doc->rootElement->createChild<gui::Label>(
            //             KeyID{"title"}, "title", ufox_beta::GetHallo(), 24.0f);
            //         title->setColor({1, 1, 1, 1});
            //
            //         auto v3 = std::make_unique<gui::VisualElement>("v3");
            //         doc->rootElement->add(std::move(v3));
            //     }, &document);
        }

        void awake() {
            executeAwake();
        }

        void run() {
            RunTimeContext context{};
            context.reset();
            auto et = mainWindow->getExtent();
            context.screenSize = glm::ivec2{et.width, et.height};

            while (context.isRunning) {
                onEventLoop(context);
                onUpdateLoop(context);
                onRenderLoop(context);
            }
        }

        void onEventLoop(RunTimeContext& ctx) {
            while (SDL_PollEvent(&ctx.event)) {
                if (ctx.event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
                    mainWindow->waitGPUIdle();
                    executeFocusGained();
                    mainWindow->requestPreDraw();
                }

                executeEventsLoop(ctx);
            }
        }

        void onUpdateLoop(RunTimeContext& ctx){
            executeUpdateLoop(ctx);
        }

        void onRenderLoop(RunTimeContext& ctx) {
            if (!ctx.isRendering) return;
            mainWindow->prepareRenderStart();
            executePrepareRender();
            mainWindow->prepareRenderEnd();
            if (const auto cmb = mainWindow->render()) mainWindow->postRender(cmb);
        }

        void close() {
            appData->systemBaseContainer.clear();

            // if (document) {
            //     mainWindow->unregisterCallbackEvent(EventType::eResize, &mainCamera);
            //     mainWindow->unregisterCallbackEvent(EventType::ePostSystemInit, &*document);
            // }
            // optionals destroy themselves in reverse order

        }


        void execution() {
            init();
            awake();
            run();
            close();
        }

        // accessors that return optional references / pointers
        [[nodiscard]] UFoxWindow& getWindow() { return *mainWindow; }
        [[nodiscard]] std::optional<geometry::MeshManager>& getMeshManager()     { return meshManager; }
        [[nodiscard]] std::optional<render::TextureManager>& getTextureManager() { return textureManager; }
        [[nodiscard]] std::optional<font::GlyphManager>& getGlyphManager()       { return glyphManager; }
        //[[nodiscard]] std::optional<gui::UIDocument>& getUIDocument()                { return document; }
        [[nodiscard]] Camera& getMainCamera()                                    { return mainCamera; }


    private:
        std::unique_ptr<UFoxWindow>                  mainWindow{};
        std::optional<geometry::MeshManager>         meshManager{};
        std::optional<render::TextureManager>        textureManager{};
        std::optional<font::GlyphManager>            glyphManager{};
        //std::optional<gui::UIDocument>               document{};
        Camera                                       mainCamera{};

        std::optional<ApplicationDataBase>           appData{};

        void executeAwake() {
            for (const auto &system : appData->systemBaseContainer | std::views::values) {
                system->awake();
            }
        }

        void executeEventsLoop(RunTimeContext& ctx) {
            for (const auto &system : appData->systemBaseContainer | std::views::values) {
                system->events(ctx);
            }
        }

        void executeUpdateLoop(RunTimeContext& ctx) {
            if (!ctx.isStarted) {
                for (const auto &system : appData->systemBaseContainer | std::views::values) {
                    system->start(ctx);
                    ctx.isStarted = true;
                }
            }

            for (const auto &system : appData->systemBaseContainer | std::views::values) {
                system->update(ctx);
            }

            for (const auto &system : appData->systemBaseContainer | std::views::values) {
                system->fixedUpdate(ctx);
            }

            for (const auto &system : appData->systemBaseContainer | std::views::values) {
                system->lateUpdate(ctx);
            }
        }

        void executeFocusGained() {
            for (const auto &system : appData->systemBaseContainer | std::views::values) {
                system->focusGained();
            }
        }

        void executePrepareRender() {
            for (const auto &system : appData->systemBaseContainer | std::views::values) {
                system->prepareRender();
            }
        }
    };

}
