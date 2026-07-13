#pragma once

// v1.0 #162: single source of truth for the shipped version.
// Keep in sync with CMakeLists.txt project(VERSION) — checked by toy_content_test.

namespace toy {

constexpr int kVersionMajor = 1;
constexpr int kVersionMinor = 2;
constexpr int kVersionPatch = 0;
constexpr const char* kVersionString = "1.2.0";
constexpr const char* kVersionCodename = "Reinforcements";
// #166/#195: product page + support (fail-open — only opened on user click).
constexpr const char* kProjectUrl = "https://github.com/cagatay-softgineer/toy-soldiers";
constexpr const char* kSupportContact = "support: GitHub issues (placeholder — Discord invite TBD)";

} // namespace toy
