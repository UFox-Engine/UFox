module;

#include <nanoid/nanoid.h>     // the real header from thirdparty/nanoid/inc/nanoid/nanoid.h
#include <string>               // std::string
#include <cstddef>              // std::size_t
#include <future>               // std::future, std::async
#include <ufox_engine_export.h>

export module ufox_nanoid;

namespace ufox::nanoid{
  // Default (21 characters, URL-safe alphabet)
  export UFOX_ENGINE_EXPORT std::string generate(){return ::nanoid::generate();}

  // Custom length, default alphabet
  export UFOX_ENGINE_EXPORT std::string generate(std::size_t size){return ::nanoid::generate(size);}

  // Custom alphabet + default length (21)
  export UFOX_ENGINE_EXPORT std::string generate(const std::string& alphabet){return ::nanoid::generate(alphabet);}

  // Full control: custom alphabet + custom length
  export UFOX_ENGINE_EXPORT std::string generate(const std::string& alphabet, std::size_t size){return ::nanoid::generate(alphabet, size);}

  // ── Async versions ──────────────────────────────────────────────────────────
  // (useful if you want to generate IDs off the main thread)

  export UFOX_ENGINE_EXPORT std::future<std::string> generate_async(){return ::nanoid::generate_async();}

  export UFOX_ENGINE_EXPORT std::future<std::string> generate_async(std::size_t size){return ::nanoid::generate_async(size);}

  export UFOX_ENGINE_EXPORT std::future<std::string> generate_async(const std::string& alphabet){return ::nanoid::generate_async(alphabet);}

  export UFOX_ENGINE_EXPORT std::future<std::string> generate_async(const std::string& alphabet, std::size_t size){return ::nanoid::generate_async(alphabet, size);}

  // ── Optional: version with custom random engine ─────────────────────────────
  // (only if you ever want to pass your own random provider)

  export UFOX_ENGINE_EXPORT std::string generate_custom_random(::nanoid::crypto_random_base& rnd,const std::string& alphabet,std::size_t size){
    return ::nanoid::generate(rnd, alphabet, size);
  }
}