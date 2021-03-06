include_directories(
  ${PROJECT_SOURCE_DIR}/include/cppunit
  ${PROJECT_SOURCE_DIR}/api
)

add_executable (test_functiondef01   test_functiondef01.cpp)
add_executable (test_uuid            test_uuid.cpp)
add_executable (test_basictypes01    test_basictypes01.cpp)
add_executable (test_attpdu01        test_attpdu01.cpp)
add_executable (test_lfringbuffer01  test_lfringbuffer01.cpp)
add_executable (test_lfringbuffer11  test_lfringbuffer11.cpp)

set_target_properties(test_functiondef01
    PROPERTIES
    CXX_STANDARD 11
    COMPILE_FLAGS "-Wall -Wextra -Werror"
)
set_target_properties(test_uuid
    PROPERTIES
    CXX_STANDARD 11
    COMPILE_FLAGS "-Wall -Wextra -Werror"
)
set_target_properties(test_basictypes01
    PROPERTIES
    CXX_STANDARD 11
    COMPILE_FLAGS "-Wall -Wextra -Werror"
)
set_target_properties(test_attpdu01
    PROPERTIES
    CXX_STANDARD 11
    COMPILE_FLAGS "-Wall -Wextra -Werror"
)
set_target_properties(test_lfringbuffer01
    PROPERTIES
    CXX_STANDARD 11
    COMPILE_FLAGS "-Wall -Wextra -Werror"
)
set_target_properties(test_lfringbuffer11
    PROPERTIES
    CXX_STANDARD 11
    COMPILE_FLAGS "-Wall -Wextra -Werror"
)

target_link_libraries (test_functiondef01 direct_bt)
target_link_libraries (test_basictypes01 direct_bt)
target_link_libraries (test_uuid direct_bt)
target_link_libraries (test_attpdu01 direct_bt)
target_link_libraries (test_lfringbuffer01 direct_bt)
target_link_libraries (test_lfringbuffer11 direct_bt)

add_test (NAME functiondef01  COMMAND test_functiondef01)
add_test (NAME basictypes01   COMMAND test_basictypes01)
add_test (NAME uuid           COMMAND test_uuid)
add_test (NAME attpdu01       COMMAND test_attpdu01)
add_test (NAME lfringbuffer01 COMMAND test_lfringbuffer01)
add_test (NAME lfringbuffer11 COMMAND test_lfringbuffer11)

