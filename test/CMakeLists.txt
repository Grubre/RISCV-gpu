set(TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/full_system_tests")

function(create_test test_name source_file)
  add_executable(${test_name} ${source_file})
  target_link_libraries(${test_name} doctest)
  foreach(lib IN LISTS ARGN)
    target_link_libraries(${test_name} ${lib})
  endforeach()
  target_compile_definitions(${test_name} PRIVATE TESTS_DIR="${TESTS_DIR}")
  add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

create_test(test_instructions test.cpp Sim GPU)
create_test(generic_tests test2.cpp Sim GPU)

create_test(number_parsing_test assembler/number_parsing.cpp AsLib)
create_test(directive_parsing_test assembler/directive_parsing.cpp AsLib)
create_test(instruction_parsing_test assembler/instruction_parsing.cpp AsLib)
create_test(generic_parsing_test assembler/generic_parsing.cpp AsLib)

create_test(full_system_test full_system_test.cpp AsLib Sim GPU)
