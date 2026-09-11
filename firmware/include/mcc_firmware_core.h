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

inline const char *protocol_banner(const char *version) {
  return version;
}

}  // namespace mccfw