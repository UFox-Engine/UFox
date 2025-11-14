module;

#include <chrono>
#include <memory>
#include <SDL3/SDL_mouse.h>
#include <glm/glm.hpp>

#include <SDL3/SDL_events.h>

#include <vector>

#include "GLFW/glfw3.h"

export module ufox_input;

import ufox_lib;  // For GenerateUniqueID

export  namespace ufox::input {
    void RefreshResources(InputResource& res) {
        res.refresh();
    }

    void UpdateGlobalMousePosition(const windowing::WindowResource& window, InputResource& res) {
        switch (res.mouseMotionState) {
            case MouseMotionState::Moving: {
                res.onMouseMove();
                res.mouseMotionState = MouseMotionState::Stopping;
            }
            case MouseMotionState::Stopping: {
                res.onMouseStop();
                res.mouseDelta = glm::ivec2{0,0};
                res.mouseMotionState = MouseMotionState::Idle;
            }
            case MouseMotionState::Idle: {
#ifdef USE_SDL
                float x,y;
                SDL_GetGlobalMouseState(&x,&y);
                x = x - static_cast<float>(window.position.x);
                y = y - static_cast<float>(window.position.y);
#else
                double x,y;
                glfwGetCursorPos(window.getHandle(), &x, &y);
#endif
                glm::ivec2 pos = {static_cast<int>(x), static_cast<int>(y)};
                res.updateMouseDelta(pos);

                if (res.getMouseDeltaMagnitude() > 0.0f) {
                    res.mousePosition = pos;
                    res.mouseMotionState = MouseMotionState::Moving;
                }
            }
            default: ;
        }
    }

    void UpdateMouseButtonAction(InputResource& res) {
        if (res.leftMouseButtonAction.phase != ActionPhase::eSleep) {
            res.onLeftMouseButton();
        }

        if (res.rightMouseButtonAction.phase != ActionPhase::eSleep) {
            res.onRightMouseButton();
        }

        if (res.middleMouseButtonAction.phase != ActionPhase::eSleep) {
            res.onMiddleMouseButton();
        }
    }

    void CatchMouseButton(InputResource& res, MouseButton button, ActionPhase phase, const float& value1, const float& value2) {
        if (button == MouseButton::eLeft) {
            res.leftMouseButtonAction.phase = phase;
            res.leftMouseButtonAction.value1 = value1;
            res.leftMouseButtonAction.value2 = value2;
            res.leftMouseButtonAction.startTime = std::chrono::steady_clock::now();
        }else if (button == MouseButton::eRight) {
            res.rightMouseButtonAction.phase = phase;
            res.rightMouseButtonAction.value1 = value1;
            res.rightMouseButtonAction.value2 = value2;
            res.rightMouseButtonAction.startTime = std::chrono::steady_clock::now();
        }else if (button == MouseButton::eMiddle) {
            res.middleMouseButtonAction.phase = phase;
            res.middleMouseButtonAction.value1 = value1;
            res.middleMouseButtonAction.value2 = value2;
            res.middleMouseButtonAction.startTime = std::chrono::steady_clock::now();
        }
    }



#ifdef USE_SDL
    StandardCursorResource CreateStandardMouseCursor() {
        StandardCursorResource resource{};

        resource.defaultCursor      = {SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT), SDL_DestroyCursor};
        resource.ewResizeCursor     = {SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE), SDL_DestroyCursor};
        resource.nsResizeCursor     = {SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE), SDL_DestroyCursor};
        resource.neswResizeCursor   = {SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE), SDL_DestroyCursor};
        resource.nwseResizeCursor   = {SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE), SDL_DestroyCursor};

        return resource;
    }
    void SetStandardCursor(StandardCursorResource& resource, const CursorType& type) {
        resource.currentCursor = type;

        switch (type) {
            case CursorType::eDefault: {
                SDL_SetCursor(resource.defaultCursor.get());
                break;
            }
            case CursorType::eEWResize: {
                SDL_SetCursor(resource.ewResizeCursor.get());
                break;
            }
            case CursorType::eNSResize: {
                SDL_SetCursor(resource.nsResizeCursor.get());
                break;
            }
            case CursorType::eNESWResize: {
                SDL_SetCursor(resource.neswResizeCursor.get());
                break;
            }
            case CursorType::eNWSEResize: {
                SDL_SetCursor(resource.nwseResizeCursor.get());
                break;
            }
            default:{
                resource.currentCursor = CursorType::eDefault;
                SDL_SetCursor(resource.defaultCursor.get());
                break;
            }
        }
    }
#else
    StandardCursorResource CreateStandardMouseCursor() {
        StandardCursorResource resource{};

        resource.defaultCursor      = {glfwCreateStandardCursor(GLFW_ARROW_CURSOR), glfwDestroyCursor};
        resource.ewResizeCursor     = {glfwCreateStandardCursor(GLFW_RESIZE_EW_CURSOR), glfwDestroyCursor};
        resource.nsResizeCursor     = {glfwCreateStandardCursor(GLFW_RESIZE_NS_CURSOR), glfwDestroyCursor};
        resource.neswResizeCursor   = {glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR), glfwDestroyCursor};
        resource.nwseResizeCursor   = {glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR), glfwDestroyCursor};

        return resource;
    }

    void SetStandardCursor(StandardCursorResource& resource, const CursorType& type, const windowing::WindowResource& window) {
        resource.currentCursor = type;

        switch (type) {
            case CursorType::eDefault: {
                glfwSetCursor(window.getHandle(),resource.defaultCursor.get());
                break;
            }
            case CursorType::eEWResize: {
                glfwSetCursor(window.getHandle(),resource.ewResizeCursor.get());
                break;
            }
            case CursorType::eNSResize: {
                glfwSetCursor(window.getHandle(),resource.nsResizeCursor.get());
                break;
            }
            case CursorType::eNESWResize: {
                glfwSetCursor(window.getHandle(),resource.neswResizeCursor.get());
                break;
            }
            case CursorType::eNWSEResize: {
                glfwSetCursor(window.getHandle(),resource.nwseResizeCursor.get());
                break;
            }
            default:{
                resource.currentCursor = CursorType::eDefault;
                glfwSetCursor(window.getHandle(),resource.defaultCursor.get());
                break;
            }
        }
    }

#endif
}




