cmake_minimum_required(VERSION 3.20)
project(grimrock_reconstruction LANGUAGES C CXX)

# The original was C++03 built with GCC 4.x; we compile as C++17 for 64-bit-safe
# library support but keep the original idioms in the source.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# the engines are shared modules of the launcher (see below), so everything linked into
# them is position independent
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
# libvpx decodes the cinematics of Legend of Grimrock 2 (IVF/VP8); without it the
# second game's video player skips the videos.
pkg_check_modules(VPX IMPORTED_TARGET vpx)
# the launcher finds its fonts through fontconfig (without it, a few common font files)
pkg_check_modules(FONTCONFIG IMPORTED_TARGET fontconfig)

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

# Legend of Grimrock 2 (grimrock2.exe) is built from the same tree with GRIMROCK_GAME=2.
# src2/ mirrors src/: a file there replaces the file of the same name in src/ (compilation
# units that diverged too far for #if) or adds a unit that only exists in the second game.
# Small differences inside shared units are guarded with GRIMROCK_GAME.
function(game_sources OUT SHARED SUBDIR)
    file(GLOB OVERRIDES CONFIGURE_DEPENDS src2/${SUBDIR}/*.cpp src2/${SUBDIR}/*.c)
    set(RESULT "")
    foreach(source ${SHARED})
        get_filename_component(name ${source} NAME)
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src2/${SUBDIR}/${name})
            continue()
        endif()
        list(APPEND RESULT ${source})
    endforeach()
    set(${OUT} ${RESULT} ${OVERRIDES} PARENT_SCOPE)
endfunction()

# The two games are one executable: grimrock (src/launcher) picks the game and loads its
# engine, a shared module next to it (libgrimrock1.so, libgrimrock2.so) whose only entry
# point is grimrock_game_main, the main() of the original. Each engine is built from its
# own core and engine libraries, compiled with its GRIMROCK_GAME.
#
# add_game(<game number> <module> <core lib> <engine lib> <core sources> <engine sources> <rapid sources> <include dirs>)
function(add_game GAME MODULE CORE ENGINE CORE_SRC ENGINE_SRC RAPID_SRC INCLUDES)
    add_library(${CORE} STATIC ${CORE_SRC})
    target_include_directories(${CORE} PUBLIC ${INCLUDES})
    # stb_image / stb_image_write (public domain, vendored)
    target_include_directories(${CORE} SYSTEM PRIVATE third_party)
    target_compile_definitions(${CORE} PUBLIC GRIMROCK_GAME=${GAME})
    target_compile_options(${CORE} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    target_link_libraries(${CORE} PUBLIC ZLIB::ZLIB SDL3::SDL3 PkgConfig::MINIZIP Threads::Threads)

    add_library(${ENGINE} STATIC ${ENGINE_SRC})
    target_link_libraries(${ENGINE} PUBLIC ${CORE} OpenGL::GL PkgConfig::GLEW
                          PkgConfig::FREETYPE PkgConfig::OPENAL PkgConfig::VORBISFILE)
    target_compile_options(${ENGINE} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    if(GAME EQUAL 2 AND VPX_FOUND)
        target_compile_definitions(${ENGINE} PRIVATE GRIMROCK_HAVE_VPX=1)
        target_link_libraries(${ENGINE} PUBLIC PkgConfig::VPX)
    endif()

    if(LUAJIT_INCLUDE_DIR AND LUAJIT_LIBRARY)
        add_library(${MODULE} SHARED ${RAPID_SRC})
        target_include_directories(${MODULE} PRIVATE ${LUAJIT_INCLUDE_DIR})
        # "Frame.h" style includes of the Lua layer resolve through the overlay as well
        foreach(dir ${INCLUDES})
            target_include_directories(${MODULE} PRIVATE ${dir}/rapid)
        endforeach()
        target_link_libraries(${MODULE} PRIVATE ${ENGINE} ${LUAJIT_LIBRARY} ${CMAKE_DL_LIBS})
        target_compile_options(${MODULE} PRIVATE -Wall -Wextra -Wno-unused-parameter)
        # a module with an undefined symbol would only fail when the launcher loads it
        target_link_options(${MODULE} PRIVATE -Wl,--no-undefined)
        # lib64/ next to the executable holds libraries the Steam Linux Runtime lacks (see
        # tools/deploy_steam.sh); a plain build only needs $ORIGIN for libsteam_api.so.
        # only relative paths: the build tree gets libsteam_api.so copied next to the module,
        # and a deployed module must not point into this machine's build
        set_target_properties(${MODULE} PROPERTIES INSTALL_RPATH "$ORIGIN/lib64:$ORIGIN"
                                                   BUILD_WITH_INSTALL_RPATH ON)
        if(GRIMROCK_HAVE_STEAM)
            # headers plus the redistributable libsteam_api.so, copied next to the module
            # like the original's lib/ directory
            target_include_directories(${MODULE} PRIVATE ${STEAMWORKS_DIR})
            target_link_libraries(${MODULE} PRIVATE ${STEAMWORKS_DIR}/linux64/libsteam_api.so)
            add_custom_command(TARGET ${MODULE} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${STEAMWORKS_DIR}/linux64/libsteam_api.so $<TARGET_FILE_DIR:${MODULE}>)
        endif()
    endif()
endfunction()

add_game(1 grimrock1 grimrock_core grimrock_engine "${CORE_SOURCES}" "${ENGINE_SOURCES}" "${RAPID_SOURCES}" src)
game_sources(CORE2_SOURCES "${CORE_SOURCES}" core)
game_sources(ENGINE2_SOURCES "${ENGINE_SOURCES}" engine)
game_sources(RAPID2_SOURCES "${RAPID_SOURCES}" rapid)
add_game(2 grimrock2 grimrock2_core grimrock2_engine "${CORE2_SOURCES}" "${ENGINE2_SOURCES}" "${RAPID2_SOURCES}" "src2;src")

# the launcher: one executable for both games; ./grimrock2 (a symlink to it) runs the
# second game
if(TARGET grimrock1 OR TARGET grimrock2)
    file(GLOB LAUNCHER_SOURCES CONFIGURE_DEPENDS src/launcher/*.cpp)
    add_executable(grimrock ${LAUNCHER_SOURCES})
    target_include_directories(grimrock SYSTEM PRIVATE third_party)
    target_link_libraries(grimrock PRIVATE SDL3::SDL3 PkgConfig::FREETYPE ${CMAKE_DL_LIBS})
    if(FONTCONFIG_FOUND)
        target_compile_definitions(grimrock PRIVATE LAUNCHER_HAVE_FONTCONFIG=1)
        target_link_libraries(grimrock PRIVATE PkgConfig::FONTCONFIG)
    endif()
    target_compile_options(grimrock PRIVATE -Wall -Wextra)
    set_target_properties(grimrock PROPERTIES INSTALL_RPATH "$ORIGIN/lib64:$ORIGIN"
                                              BUILD_WITH_INSTALL_RPATH ON)
    foreach(module grimrock1 grimrock2)
        if(TARGET ${module})
            add_dependencies(grimrock ${module})
        endif()
    endforeach()
    add_custom_command(TARGET grimrock POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink grimrock $<TARGET_FILE_DIR:grimrock>/grimrock2)
endif()

enable_testing()
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*_test.cpp)
foreach(test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    if(test_name STREQUAL "launcher_test")
        # the launcher's game library stands on its own
        target_sources(${test_name} PRIVATE src/launcher/GameLibrary.cpp)
    elseif(TARGET grimrock_engine)
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
