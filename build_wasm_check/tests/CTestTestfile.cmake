# CMake generated Testfile for 
# Source directory: /Users/will/dev/BCHLight/tests
# Build directory: /Users/will/dev/BCHLight/build_wasm_check/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unit_tests "/opt/homebrew/opt/node/bin/node" "/Users/will/dev/BCHLight/build_wasm_check/tests/unit_tests.js")
set_tests_properties(unit_tests PROPERTIES  _BACKTRACE_TRIPLES "/Users/will/dev/BCHLight/tests/CMakeLists.txt;4;add_test;/Users/will/dev/BCHLight/tests/CMakeLists.txt;0;")
