module;

#include <nanoid/nanoid.h>     // the real header from thirdparty/nanoid/inc/nanoid/nanoid.h
#include <string>               // std::string
#include <cstddef>              // std::size_t
#include <future>               // std::future, std::async

export module ufox_nanoid;

export namespace ufox::nanoid{
  // Default (21 characters, URL-safe alphabet)
  std::string generate(){return ::nanoid::generate();}

  // Custom length, default alphabet
  std::string generate(std::size_t size){return ::nanoid::generate(size);}

  // Custom alphabet + default length (21)
  std::string generate(const std::string& alphabet){return ::nanoid::generate(alphabet);}

  // Full control: custom alphabet + custom length
  std::string generate(const std::string& alphabet, std::size_t size){return ::nanoid::generate(alphabet, size);}

  // ── Async versions ──────────────────────────────────────────────────────────
  // (useful if you want to generate IDs off the main thread)

  std::future<std::string> generate_async(){return ::nanoid::generate_async();}

  std::future<std::string> generate_async(std::size_t size){return ::nanoid::generate_async(size);}

  std::future<std::string> generate_async(const std::string& alphabet){return ::nanoid::generate_async(alphabet);}

  std::future<std::string> generate_async(const std::string& alphabet, std::size_t size){return ::nanoid::generate_async(alphabet, size);}

  // ── Optional: version with custom random engine ─────────────────────────────
  // (only if you ever want to pass your own random provider)

  std::string generate_custom_random(::nanoid::crypto_random_base& rnd,const std::string& alphabet,std::size_t size){
    return ::nanoid::generate(rnd, alphabet, size);
  }
}