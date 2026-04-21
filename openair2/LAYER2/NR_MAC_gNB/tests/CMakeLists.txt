# SPDX-License-Identifier: LicenseRef-CSSL-1.0
#
# Unit tests for symbol-level beam allocation functions.
# This test is self-contained and does not depend on the full OAI build graph.

add_executable(test_beam_symbol_alloc test_beam_symbol_alloc.c)
target_compile_options(test_beam_symbol_alloc PRIVATE -Wall -Wextra)
add_test(NAME test_beam_symbol_alloc COMMAND test_beam_symbol_alloc)

add_executable(test_beam_selection test_beam_selection.c)
target_compile_options(test_beam_selection PRIVATE -Wall -Wextra)
add_test(NAME test_beam_selection COMMAND test_beam_selection)

add_executable(test_ssb_64beam test_ssb_64beam.c)
target_compile_options(test_ssb_64beam PRIVATE -Wall -Wextra)
add_test(NAME test_ssb_64beam COMMAND test_ssb_64beam)

add_executable(test_antenna_control test_antenna_control.c)
target_compile_options(test_antenna_control PRIVATE -Wall -Wextra)
add_test(NAME test_antenna_control COMMAND test_antenna_control)
