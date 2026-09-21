cmake_minimum_required(VERSION 3.20)
project(grimrock_reconstruction LANGUAGES C CXX)

# The original was C++03 built with GCC 4.x; we compile as C++17 for 64-bit-safe
# library support but keep the original idioms in the source.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ZLIB REQUIRED)
find_package(SDL3 REQUIRED CONFIG)
find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLEW REQUIRED IMPORTED_TARGET glew)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(OPENAL REQUIRED IMPORTED_TARGET openal)
pkg_check_modules(VORBISFILE REQUIRED IMPORTED_TARGET vorbisfile)
pkg_check_modules(MINIZIP REQUIRED IMPORTED_TARGET minizip)

# LuaJIT 2.0.0-rc3 (tools/fetch_deps.sh builds it into third_party/luajit); a system LuaJIT
# 2.0.x also loads the game's bytecode, 2.1 does not.
find_path(LUAJIT_INCLUDE_DIR lua.h
          HINTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/luajit/src
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/LuaJIT-original/src)
foreach(dir third_party/luajit tools/LuaJIT-original)
    if(NOT LUAJIT_LIBRARY AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/src/libluajit.a)
        set(LUAJIT_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/src/libluajit.a)
    endif()
endforeach()
if(NOT LUAJIT_LIBRARY)
    find_library(LUAJIT_LIBRARY NAMES luajit-5.1 luajit)
endif()

# Steamworks SDK (not redistributable, see tools/fetch_deps.sh). Without it the Steam
# module is a stub and the game runs in its non-Steam mode.
set(STEAMWORKS_DIR ${CMAKE_SOURCE_DIR}/third_party/steamworks)
if(EXISTS ${STEAMWORKS_DIR}/steam/steam_api.h AND EXISTS ${STEAMWORKS_DIR}/linux64/libsteam_api.so)
    set(GRIMROCK_HAVE_STEAM ON)
else()
    set(GRIMROCK_HAVE_STEAM OFF)
    message(STATUS "Steamworks SDK not found in third_party/steamworks: building the Steam stub")
endif()

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp)
file(GLOB ENGINE_SOURCES CONFIGURE_DEPENDS src/engine/*.cpp)
file(GLOB RAPID_SOURCES CONFIGURE_DEPENDS src/rapid/*.cpp src/rapid/*.c)
if(GRIMROCK_HAVE_STEAM)
    list(REMOVE_ITEM RAPID_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/rapid/SteamStub.cpp)
else()
    list(REMOVE_ITEM RAPID_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/rapid/Steam.cpp)
endif()

add_library(grimrock_core ${CORE_SOURCES})
target_include_directories(grimrock_core PUBLIC src)
# stb_image / stb_image_write (public domain, vendored)
target_include_directories(grimrock_core SYSTEM PRIVATE third_party)
target_compile_options(grimrock_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(grimrock_core PUBLIC ZLIB::ZLIB SDL3::SDL3 PkgConfig::MINIZIP Threads::Threads)
target_compile_options(grimrock_core PRIVATE -Wall -Wextra -Wno-unused-parameter)

if(ENGINE_SOURCES)
    add_library(grimrock_engine ${ENGINE_SOURCES})
    target_link_libraries(grimrock_engine PUBLIC grimrock_core OpenGL::GL PkgConfig::GLEW
                          PkgConfig::FREETYPE PkgConfig::OPENAL PkgConfig::VORBISFILE)
    target_compile_options(grimrock_engine PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(RAPID_SOURCES AND LUAJIT_INCLUDE_DIR AND LUAJIT_LIBRARY)
    add_executable(grimrock ${RAPID_SOURCES})
    target_include_directories(grimrock PRIVATE ${LUAJIT_INCLUDE_DIR})
    target_link_libraries(grimrock PRIVATE grimrock_engine ${LUAJIT_LIBRARY} ${CMAKE_DL_LIBS})
    target_compile_options(grimrock PRIVATE -Wall -Wextra -Wno-unused-parameter)
    # symbol names in the stall watchdog's backtraces (GRIMROCK_DEBUG_STALLS)
    target_link_options(grimrock PRIVATE -rdynamic)
    # lib64/ next to the executable holds libraries the Steam Linux Runtime lacks (see
    # tools/deploy_steam.sh); a plain build only needs $ORIGIN for libsteam_api.so.
    set_target_properties(grimrock PROPERTIES BUILD_RPATH "$ORIGIN/lib64:$ORIGIN"
                                              INSTALL_RPATH "$ORIGIN/lib64:$ORIGIN")
    if(GRIMROCK_HAVE_STEAM)
        # headers plus the redistributable libsteam_api.so, copied next to the executable
        # like the original's lib/ directory
        target_include_directories(grimrock PRIVATE ${STEAMWORKS_DIR})
        target_link_libraries(grimrock PRIVATE ${STEAMWORKS_DIR}/linux64/libsteam_api.so)
        add_custom_command(TARGET grimrock POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${STEAMWORKS_DIR}/linux64/libsteam_api.so $<TARGET_FILE_DIR:grimrock>)
    endif()
endif()


enable_testing()
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*_test.cpp)
foreach(test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    if(TARGET grimrock_engine)
        target_link_libraries(${test_name} PRIVATE grimrock_engine)
    else()
        target_link_libraries(${test_name} PRIVATE grimrock_core)
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

add_executable(grimrock_archive tools/archive.cpp)
target_link_libraries(grimrock_archive PRIVATE grimrock_core)

# Debugging aid: compile the listed sources with pattern-initialised locals.
if(GRIMROCK_PATTERN_INIT_FILES)
    set_source_files_properties(${GRIMROCK_PATTERN_INIT_FILES} PROPERTIES COMPILE_OPTIONS "-ftrivial-auto-var-init=pattern")
endif()
