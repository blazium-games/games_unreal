#!/bin/sh
# Fails while the default player session store leaves the account bearer in a
# file another local user can read. Passes once that store is fixed.
set -eu
cd "$(dirname "$0")/.."
g++ -std=c++17 -Wall -Wextra -Wno-comment \
  -I tests/session_theft/stub -I Source/BlaziumGames/Public \
  -o /tmp/session_file_exposure_test \
  tests/session_theft/session_file_exposure_test.cpp
/tmp/session_file_exposure_test
