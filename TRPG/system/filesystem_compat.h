#pragma once

// Provide fallbacks for <filesystem> on older toolchains that only have <experimental/filesystem>.
#if defined(__has_include)
#  if __has_include(<filesystem>)
#    include <filesystem>
#  elif __has_include(<experimental/filesystem>)
#    include <experimental/filesystem>
namespace std { namespace filesystem = std::experimental::filesystem; }
#  else
#    error "<filesystem> or <experimental/filesystem> required"
#  endif
#else
#  include <filesystem>
#endif
