module;

#include "glm/ext/matrix_transform.hpp"

#include <chrono>
#include <utility>

#include <vector>

#include <optional>

#include <iostream>

#include <string>

#include <format>

#include <source_location>
#include <filesystem>

#include <functional>
#include <ranges>

#include <glm/glm.hpp>

#ifdef USE_SDL

#include <SDL3/SDL.h>

#include <SDL3/SDL_vulkan.h>

#else

#include <GLFW/glfw3.h>

#endif

export module ufox_lib;



export namespace ufox {
    namespace utilities {
        template <typename TargetType, typename SourceType>
        constexpr  TargetType CheckedCast( SourceType value ) {
            static_assert( sizeof( TargetType ) <= sizeof( SourceType ), "No need to cast from smaller to larger type!" );
            static_assert( std::numeric_limits<SourceType>::is_integer, "Only integer types supported!" );
            static_assert( !std::numeric_limits<SourceType>::is_signed, "Only unsigned types supported!" );
            static_assert( std::numeric_limits<TargetType>::is_integer, "Only integer types supported!" );
            static_assert( !std::numeric_limits<TargetType>::is_signed, "Only unsigned types supported!" );
            assert( value <= ( std::numeric_limits<TargetType>::max )() );
            return static_cast<TargetType>( value );
        }

        constexpr size_t GenerateUniqueID(const std::vector<glm::vec4>& fields) {
            std::hash<float> floatHasher;
            size_t hash = 0;

            auto combineHash = [&hash](size_t value) {
                hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            };

            auto combineFloat = [&combineHash, &floatHasher](float value) {
                float normalized = std::round(value * 1000000.0f) / 1000000.0f;
                combineHash(floatHasher(normalized));
            };

            for (const auto& vec : fields) {
                combineFloat(vec.r);
                combineFloat(vec.g);
                combineFloat(vec.b);
                combineFloat(vec.a);
            }

            return hash;
        }

        // Hash a vector of floats

        constexpr size_t GenerateUniqueID(const std::vector<float>& fields) {
            std::hash<float> floatHasher;
            size_t hash = 0;

            auto combineHash = [&hash](size_t value) {
                hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            };

            auto combineFloat = [&combineHash, &floatHasher](float value) {
                float normalized = std::round(value * 1000000.0f) / 1000000.0f;
                combineHash(floatHasher(normalized));
            };

            for (float value : fields) {
                combineFloat(value);
            }
            return hash;
        }

        constexpr  size_t GenerateUniqueID(std::string_view sv) {
            std::vector<float> chars;
            chars.reserve(sv.size());
            for (char c : sv) {
                chars.push_back(static_cast<unsigned char>(c));
            }
            return GenerateUniqueID(chars);
        }

        constexpr  size_t GenerateUniqueID(const char* str) {
            return GenerateUniqueID(std::string_view(str));
        }

        constexpr size_t GenerateUniqueID(const std::string& str) {
            return GenerateUniqueID(std::string_view(str));
        }

        template <typename T>
        [[nodiscard]] constexpr std::optional<std::size_t> Index_Of(std::span<T> container, const T& value) noexcept {
            auto it = std::ranges::find(container, value);
            if (it == container.end())
                return std::nullopt;

            return static_cast<std::size_t>(std::ranges::distance(container.begin(), it));
        }
    }

    namespace debug {
    enum class LogLevel {
        eInfo,
        eWarning,
        eError
    };

    struct FormatWithLocation {
        std::string_view format;
        std::source_location location;

        template<typename T>
        FormatWithLocation(const T& fmt,
                           std::source_location loc = std::source_location::current())
            : format(fmt)
            , location(loc)
        {}
    };


    template<typename... Args>
        void log(LogLevel level, FormatWithLocation fmt, Args&&... args){
            // Extract only the filename (no full path)
            const std::string fileName = std::filesystem::path(fmt.location.file_name()).filename().string();

            std::string level_str;
            switch (level) {
            case LogLevel::eInfo:    level_str = "INFO";    break;
            case LogLevel::eWarning: level_str = "WARNING"; break;
            case LogLevel::eError:   level_str = "ERROR";   break;
            }

            std::string message = std::vformat(fmt.format, std::make_format_args(args...));

            // Clickable format: filename(line): [LEVEL] message
            // Double-click in the Output window / console jumps to the source line.
            std::cout << std::format("{}({}): [{}] {}\n",fileName,fmt.location.line(),level_str,message);
        }
    }


    namespace input {
        struct InputResource;
        struct EventCallbackPool;

        enum class CursorType {
            eDefault,
            eEWResize,
            eNSResize,
            eNWSEResize,
            eNESWResize
        };

        enum class ActionPhase {
            eWait,
            eStart,
            ePerform,
            eEnd,
            eReset,
            eRepeat,
            eSleep
        };

        enum class MouseButtonState {
            eNone,
            eDown,
            eUp,
            eHold,
        };

        enum class MouseButton {
            eNone,eLeft, eRight, eMiddle
        };


        struct StandardCursorResource {
#ifdef USE_SDL
            using CursorHandle = SDL_Cursor;
            using DestroyFunc = decltype(&SDL_DestroyCursor);

#else
            using CursorHandle = GLFWcursor;
            using DestroyFunc = decltype(&glfwDestroyCursor);
#endif



            CursorType                                  currentCursor{CursorType::eDefault};
            std::unique_ptr<CursorHandle, DestroyFunc>  defaultCursor{nullptr,nullptr};
            std::unique_ptr<CursorHandle, DestroyFunc>  ewResizeCursor{nullptr,nullptr};
            std::unique_ptr<CursorHandle, DestroyFunc>  nsResizeCursor{nullptr,nullptr};
            std::unique_ptr<CursorHandle, DestroyFunc>  nwseResizeCursor{nullptr,nullptr};
            std::unique_ptr<CursorHandle, DestroyFunc>  neswResizeCursor{nullptr,nullptr};

            void ResetAll() {
                defaultCursor.reset();
                ewResizeCursor.reset();
                nsResizeCursor.reset();
                nwseResizeCursor.reset();
                neswResizeCursor.reset();
            }
        };

        struct EventCallbackPool {
            struct Handler {
                Handler(const Handler&) = delete;
                Handler(EventCallbackPool* p, std::size_t i) : pool(p), index(i) {}
                Handler() = default;
                Handler(Handler&& o) noexcept : pool(o.pool), index(o.index) { o.pool = nullptr; o.index = SIZE_MAX; }
                ~Handler() { disconnect(); }
                Handler& operator=(const Handler&) = delete;
                Handler& operator=(Handler&& o) noexcept { disconnect(); pool = o.pool; index = o.index; o.pool = nullptr; o.index = SIZE_MAX; return *this; }

                EventCallbackPool* pool = nullptr;
                std::size_t index = SIZE_MAX;

                void disconnect() { if (pool && index != SIZE_MAX) pool->unbind(index); pool = nullptr; index = SIZE_MAX; }
                [[nodiscard]] bool is_connected() const { return pool != nullptr; }
            };

            using Callback = std::function<void(InputResource&)>;

            [[nodiscard]] Handler bind(Callback cb) {
                std::size_t idx = free_list.empty() ? callbacks.size() : free_list.back();
                if (free_list.empty()) {
                    callbacks.emplace_back();
                    active.push_back(false);
                } else {
                    free_list.pop_back();
                }
                callbacks[idx] = std::move(cb);
                active[idx] = true;
                return {this, idx};
            }

            void unbind(std::size_t idx) {
                if (idx < active.size() && active[idx]) {
                    active[idx] = false;
                    free_list.push_back(idx);
                    callbacks[idx] = nullptr;
                }
            }

            void clear() {
                for (std::size_t i = 0; i < active.size(); ++i) {
                    unbind(i);
                }
            }

            void invoke(InputResource& input) const {
                for (std::size_t i = 0; i < active.size(); ++i) {
                    if (active[i] && callbacks[i]) callbacks[i](input);
                }
            }

        private:
            std::vector<Callback> callbacks;
            std::vector<bool> active;
            std::vector<std::size_t> free_list;
        };

        struct Action {
            explicit Action(const std::string &name_, const std::chrono::milliseconds& timeout_) : id(utilities::GenerateUniqueID(name_)) , name(name_), timeout(timeout_) {
                debug::log(debug::LogLevel::eInfo, "InputAction CREATED: {} [id: {:#x}]", name, id);
            }
            ~Action() = default;

            const uint32_t                          id;
            const std::string                       name;
            ActionPhase                             phase = ActionPhase::eSleep;
            float                                   value1 = 0.0f;
            float                                   value2 = 0.0f;
            bool                                    isStarted{false};
            bool                                    isPerformed{false};
            bool                                    isEnded{false};
            uint32_t                                triggerCount{0};

            std::chrono::steady_clock::time_point   startTime = std::chrono::steady_clock::now();
            std::chrono::milliseconds               timeout{1000};

            void refresh() noexcept {
                if (phase == ActionPhase::eStart && isStarted) {
                    phase = ActionPhase::ePerform;
                }else if (phase == ActionPhase::eEnd && isEnded) {
                    phase = std::chrono::steady_clock::now() - startTime > timeout? ActionPhase::eReset : ActionPhase::eRepeat;
                }else if (phase == ActionPhase::eRepeat) {
                    isStarted = false;
                    isPerformed = false;
                    isEnded = false;
                    phase = ActionPhase::eWait;
                }
                else if (phase == ActionPhase::eReset) {
                    isStarted = false;
                    isPerformed = false;
                    isEnded = false;
                    triggerCount = 0;
                    phase = ActionPhase::eSleep;
                }
            }

            void perform() noexcept {
                if (phase == ActionPhase::eStart) {
                    triggerCount++;
                    isStarted = true;
                }else if (phase == ActionPhase::ePerform && !isPerformed) {
                    isPerformed = true;
                }else if (phase == ActionPhase::eEnd) {
                    isEnded = true;
                }else if (phase == ActionPhase::eWait) {
                    phase = std::chrono::steady_clock::now() - startTime > timeout? ActionPhase::eReset : ActionPhase::eWait;
                }
            }
        };

        enum class MouseMotionState : uint8_t {
            Idle,     // No movement
            Moving,   // Delta > 0
            Stopping  // Delta == 0, was Moving
        };

        constexpr  std::string MouseMotionStateToString(const MouseMotionState& state) {
            switch (state) {
                case MouseMotionState::Moving: return "Moving";
                case MouseMotionState::Stopping: return "Stopping";
                case MouseMotionState::Idle: return "Idle";
                default: return "Unknown";
            }
        }

        struct InputResource {

            glm::ivec2 mousePosition{0,0};
            glm::ivec2 mouseDelta{0, 0};
            glm::ivec2 mouseWheel{0,0};

            MouseMotionState mouseMotionState{MouseMotionState::Idle};
            Action leftMouseButtonAction{"left-mouse-button", std::chrono::milliseconds{500}};
            Action rightMouseButtonAction{"right-mouse-button", std::chrono::milliseconds{500}};
            Action middleMouseButtonAction{"middle-mouse-button", std::chrono::milliseconds{500}};

            EventCallbackPool onMouseMoveCallbackPool{};
            EventCallbackPool onMouseStopCallbackPool{};
            EventCallbackPool onMouseWheelCallbackPool{};

            EventCallbackPool onLeftMouseButtonCallbackPool{};
            EventCallbackPool onRightMouseButtonCallbackPool{};
            EventCallbackPool onMiddleMouseButtonCallbackPool{};

            bool mouseLeftButton{false}, mouseRightButton{false}, mouseMiddleButton{false};

            void refresh() noexcept {
                mouseWheel = {0,0};
                leftMouseButtonAction.refresh();
            }
            void updateMouseDelta(const glm::ivec2& pos) noexcept { mouseDelta = pos - mousePosition; }
            [[nodiscard]] float getMouseDeltaMagnitude() const noexcept { return glm::length(glm::vec2(mouseDelta)); }

            void onMouseMove() {
                onMouseMoveCallbackPool.invoke(*this);
            }

            void onMouseStop() {
                onMouseStopCallbackPool.invoke(*this);
            }

            void onLeftMouseButton() {

                leftMouseButtonAction.perform();

                onLeftMouseButtonCallbackPool.invoke(*this);
            }

            void onRightMouseButton() {
                onRightMouseButtonCallbackPool.invoke(*this);
            }

            void onMiddleMouseButton() {
                onMiddleMouseButtonCallbackPool.invoke(*this);
            }

            void onMouseWheel() {
                onMouseWheelCallbackPool.invoke(*this);
            }
        };
    }
}
