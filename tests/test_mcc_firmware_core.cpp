#include "mcc_firmware_core.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

void expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  using mccfw::ComponentState;
  expect(std::strcmp(mccfw::state_name(ComponentState::Unknown), "unknown") == 0, "unknown state label");
  expect(std::strcmp(mccfw::state_name(ComponentState::Ready), "ready") == 0, "ready state label");
  expect(std::strcmp(mccfw::state_name(ComponentState::Unavailable), "unavailable") == 0, "unavailable state label");
  expect(std::strcmp(mccfw::state_name(ComponentState::Fault), "fault") == 0, "fault state label");
  expect(std::strcmp(mccfw::state_name(ComponentState::ReadOnly), "read-only") == 0, "read-only state label");
  expect(!mccfw::can_issue_output_command(ComponentState::Ready, true), "read-only mode blocks output command");
  expect(!mccfw::can_issue_output_command(ComponentState::Fault, false), "fault state blocks output command");
  expect(mccfw::can_issue_output_command(ComponentState::Ready, false), "ready writable state permits output command");
  expect(std::strcmp(mccfw::comparison_name(mccfw::ComparisonState::Awaiting), "awaiting") == 0, "awaiting comparison label");
  expect(std::strcmp(mccfw::comparison_name(mccfw::ComparisonState::Match), "match") == 0, "matching comparison label");
  expect(std::strcmp(mccfw::comparison_name(mccfw::ComparisonState::Mismatch), "mismatch") == 0, "mismatched comparison label");
  expect(std::strcmp(mccfw::comparison_name(mccfw::ComparisonState::Fault), "fault") == 0, "fault comparison label");
  expect(std::strcmp(mccfw::protocol_banner("0.1.0-diagnostic.1"), "0.1.0-diagnostic.1") == 0, "protocol banner version");
  std::cout << "MCC firmware core tests passed.\n";
  return 0;
}