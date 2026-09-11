#pragma once

#include <cstdint>

namespace mccfw {

enum class ComponentState : uint8_t {
  Unknown,
  Ready,
  Unavailable,
  Fault,
  ReadOnly,
};

struct ComponentStatus {
  const char *name;
  ComponentState state;
  const char *detail;
};

enum class ComparisonState : uint8_t {
  Awaiting,
  Match,
  Mismatch,
  Fault,
};

inline const char *state_name(ComponentState state) {
  switch (state) {
    case ComponentState::Unknown: return "unknown";
    case ComponentState::Ready: return "ready";
    case ComponentState::Unavailable: return "unavailable";
    case ComponentState::Fault: return "fault";
    case ComponentState::ReadOnly: return "read-only";
  }
  return "unknown";
}

inline bool can_issue_output_command(ComponentState state, bool read_only) {
  return !read_only && state == ComponentState::Ready;
}

inline const char *comparison_name(ComparisonState state) {
  switch (state) {
    case ComparisonState::Awaiting: return "awaiting";
    case ComparisonState::Match: return "match";
    case ComparisonState::Mismatch: return "mismatch";
    case ComparisonState::Fault: return "fault";
  }
  return "awaiting";
}

inline const char *protocol_banner(const char *version) {
  return version;
}

}  // namespace mccfw