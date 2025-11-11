module;

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


    // auto GetGlobalMousePosition() {
    //     float x,y;
    //     SDL_GetGlobalMouseState(&x,&y);
    //     return std::make_pair(x,y);
    // }
    //
    // void updateMousePositionInsideWindow() {
    //     SDL_GetMouseState(&currentLocalMouseX, &currentLocalMouseY);
    //     //fmt::println("position inside x:{} y:{}", currentLocalMouseX, currentLocalMouseY);
    // }
    //
    // void updateMousePositionOutsideWindow(SDL_Window *window) {
    //     if (!mouseIsOutSideWindow) return;
    //
    //     SDL_GetGlobalMouseState(&currentGlobalMouseX, &currentGlobalMouseY);
    //     if (currentGlobalMouseX == previousGlobalMouseX && currentGlobalMouseY == previousGlobalMouseY) return;
    //
    //     int xPos,yPos;
    //     SDL_GetWindowPosition(window, &xPos, &yPos);
    //     currentLocalMouseX = currentGlobalMouseX - xPos;
    //     currentLocalMouseY = currentGlobalMouseY - yPos;
    //     previousGlobalMouseX = currentGlobalMouseX;
    //     previousGlobalMouseY = currentGlobalMouseY;
    //     //fmt::println("position outside x:{} y:{}", currentLocalMouseX, currentLocalMouseY);
    //
    // }
    //
    // void enabledMousePositionOutside(bool state) {
    //         if (mouseIsOutSideWindow != state)
    //             mouseIsOutSideWindow = state;
    // }
    //
    // void updateMouseEvents(const SDL_Event &event) {
    //     _isMouseButtonDown = false;
    //     _isMouseButtonUp = false;
    //
    //
    //     switch (event.type) {
    //         case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    //             _isMouseButtonDown = true;
    //
    //             break;
    //         }
    //         case SDL_EVENT_MOUSE_BUTTON_UP: {
    //             _isMouseButtonUp = true;
    //
    //             break;
    //         }
    //         case SDL_EVENT_MOUSE_MOTION: {
    //
    //             enabledMousePositionOutside(false);
    //             updateMousePositionInsideWindow();
    //             break;
    //         }
    //         case SDL_EVENT_WINDOW_FOCUS_LOST: {
    //             enabledMousePositionOutside(false);
    //             break;
    //         }
    //         case SDL_EVENT_WINDOW_FOCUS_GAINED: {
    //             enabledMousePositionOutside(true);
    //             break;
    //         }
    //         default: {
    //             if (event.motion.x == 0 || event.motion.y == 0) {
    //                 enabledMousePositionOutside(true);
    //             }
    //         }
    //     }
    // }


    // void setCursor(const CursorType type) {
    //     if (currentCursor != type) {
    //         switch (type) {
    //             case CursorType::eDefault: {
    //                 SDL_SetCursor(_defaultCursor.get());
    //                 break;
    //             }
    //             case CursorType::eEWResize: {
    //                 SDL_SetCursor(_ewResizeCursor.get());
    //                 break;
    //             }
    //             case CursorType::eNSResize: {
    //                 SDL_SetCursor(_nsResizeCursor.get());
    //                 break;
    //             }
    //             case CursorType::eNWResize: {
    //                 SDL_SetCursor(_nwResizeCursor.get());
    //                 break;
    //             }
    //             case CursorType::eNEResize: {
    //                 SDL_SetCursor(_neResizeCursor.get());
    //                 break;
    //             }
    //             case CursorType::eSWResize: {
    //                 SDL_SetCursor(_swResizeCursor.get());
    //                 break;
    //             }
    //                 case CursorType::eSEResize: {
    //                 SDL_SetCursor(_seResizeCursor.get());
    //                 break;
    //             }
    //             default: {
    //                 currentCursor = CursorType::eDefault;
    //                 SDL_SetCursor(_defaultCursor.get());
    //                 break;
    //             }
    //         }
    //
    //         currentCursor = type;
    //     }
    }




