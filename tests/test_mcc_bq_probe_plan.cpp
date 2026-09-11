#include "mcc_bq_probe_plan.h"

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
  using namespace mccfw;

  expect(std::strcmp(kBqC01IdentityProbe.slot, "C01") == 0, "probe is restricted to C01");
  expect(kBqC01IdentityProbe.tca_address == 0x70, "probe uses first TCA only");
  expect(kBqC01IdentityProbe.tca_channel_mask == 0x01, "probe uses TCA channel zero only");
  expect(kBqC01IdentityProbe.bq_address == 0x6B, "probe uses BQ24195 address");
  expect(kBqC01IdentityProbe.register_address == 0x0A, "probe reads BQ identity register");
  expect(tca_channel_mask_is_single_channel(0x01), "channel zero selection is singular");
  expect(tca_channel_mask_is_single_channel(0x80), "channel seven selection is singular");
  expect(!tca_channel_mask_is_single_channel(0x00), "no selection is not a selected channel");
  expect(!tca_channel_mask_is_single_channel(0x03), "multiple channel selection is rejected");
  expect(bq_register_is_in_snapshot_scope(0x00), "BQ snapshot starts at REG00");
  expect(bq_register_is_in_snapshot_scope(0x0A), "BQ snapshot includes REG0A");
  expect(!bq_register_is_in_snapshot_scope(0x0B), "BQ snapshot excludes registers after REG0A");
  std::cout << "MCC BQ identity probe plan tests passed.\n";
  return 0;
}