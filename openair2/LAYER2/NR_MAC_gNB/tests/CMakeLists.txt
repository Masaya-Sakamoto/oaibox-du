# SPDX-License-Identifier: LicenseRef-CSSL-1.0
#
# Unit tests for symbol-level beam allocation functions.
# This test is self-contained and does not depend on the full OAI build graph.

add_executable(test_beam_symbol_alloc test_beam_symbol_alloc.c)
target_compile_options(test_beam_symbol_alloc PRIVATE -Wall -Wextra)
add_test(NAME test_beam_symbol_alloc COMMAND test_beam_symbol_alloc)
