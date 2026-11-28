//
// Created by b-boy on 27.11.2026.
//
module;

#include "_deps/sdl3-src/include/SDL3/SDL_events.h"

#include <ranges>
#include <string>
#include <string_view>
#include <ufox_engine_export.h>
#include <unordered_map>
#include <utility>


export module ufox_core;

import ufox_key;
import ufox_nanoid;
import glm;

namespace ufox {
  export struct UFOX_ENGINE_EXPORT ApplicationDataBase;

  export struct UFOX_ENGINE_EXPORT RunTimeContext {
    bool                  isRunning{};
    bool                  isRendering{};
    bool                  isStarted{};
    bool                  isResized{};
    double                deltaTime{};
    SDL_Event             event{};
    ApplicationDataBase*  appData{};

    void reset() {
      isRunning = true;
      isStarted = false;
      isRendering = true;
      deltaTime = 0;
    }
  };


  export struct UFOX_ENGINE_EXPORT SystemBase {

    explicit SystemBase(const std::string_view id_) : id(id_) {}
    explicit SystemBase(KeyID id_ = {}) : id(id_.empty() ? KeyID{nanoid::generate(12)} : std::move(id_)) {}

    virtual ~SystemBase() = default;

    virtual int awake() { return 0; }
    virtual int events(SDL_Event& event, bool& _isRunning) {return 0;}
    virtual int start(const double& deltaTime) { return 0; }
    virtual int update(const double& deltaTime) { return 0; }
    virtual int lateUpdate(const double& deltaTime) { return 0; }
    virtual int fixedUpdate(const double& deltaTime) { return 0; }
    virtual int close() { return 0; }

    [[nodiscard]] const KeyID* getId() const noexcept { return &id; }


  protected:
    const KeyID id{};

  };

  export struct UFOX_ENGINE_EXPORT ApplicationDataBase {
    ApplicationDataBase(const KeyID &engineTitleID,const KeyID& engineVersionID) : engineTitleID(engineTitleID), engineVersionID(engineVersionID) {}
    ApplicationDataBase(const std::string_view engineTitle,const std::string_view engineVersion) : engineTitleID(engineTitle), engineVersionID(engineVersion) {}
    virtual ~ApplicationDataBase() = default;

    const KeyID                                                       engineTitleID{};
    const KeyID                                                       engineVersionID{};
    glm::ivec2                                                        screenSize{800,800};
    std::unordered_map<const KeyID, SystemBase*, KeyIDHash, KeyIDEq>  systemBaseContainer{};

    bool addSystem(const KeyID& id, SystemBase* system) {
      if (systemBaseContainer.contains(id)) return false;
      systemBaseContainer.emplace(id, system);
      return true;
    }
    bool removeSystem(const KeyID& id) {
      if (!systemBaseContainer.contains(id)) return false;
      systemBaseContainer.erase(id);
      return true;
    }

    SystemBase* getSystem(const KeyID& id) {
      return systemBaseContainer.contains(id) ? systemBaseContainer.at(id) : nullptr;
    }

    std::string getEngineTitle() const noexcept {return  engineTitleID.data + " " + engineVersionID.data;}

  };
}