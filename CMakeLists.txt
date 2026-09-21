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

# add_game(<game number> <executable> <core lib> <engine lib> <core sources> <engine sources> <rapid sources> <include dirs>)
function(add_game GAME EXE CORE ENGINE CORE_SRC ENGINE_SRC RAPID_SRC INCLUDES)
    add_library(${CORE} ${CORE_SRC})
    target_include_directories(${CORE} PUBLIC ${INCLUDES})
    # stb_image / stb_image_write (public domain, vendored)
    target_include_directories(${CORE} SYSTEM PRIVATE third_party)
    target_compile_definitions(${CORE} PUBLIC GRIMROCK_GAME=${GAME})
    target_compile_options(${CORE} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    target_link_libraries(${CORE} PUBLIC ZLIB::ZLIB SDL3::SDL3 PkgConfig::MINIZIP Threads::Threads)

    add_library(${ENGINE} ${ENGINE_SRC})
    target_link_libraries(${ENGINE} PUBLIC ${CORE} OpenGL::GL PkgConfig::GLEW
                          PkgConfig::FREETYPE PkgConfig::OPENAL PkgConfig::VORBISFILE)
    target_compile_options(${ENGINE} PRIVATE -Wall -Wextra -Wno-unused-parameter)

    if(LUAJIT_INCLUDE_DIR AND LUAJIT_LIBRARY)
        add_executable(${EXE} ${RAPID_SRC})
        target_include_directories(${EXE} PRIVATE ${LUAJIT_INCLUDE_DIR})
        # "Frame.h" style includes of the Lua layer resolve through the overlay as well
        foreach(dir ${INCLUDES})
            target_include_directories(${EXE} PRIVATE ${dir}/rapid)
        endforeach()
        target_link_libraries(${EXE} PRIVATE ${ENGINE} ${LUAJIT_LIBRARY} ${CMAKE_DL_LIBS})
        target_compile_options(${EXE} PRIVATE -Wall -Wextra -Wno-unused-parameter)
        # symbol names in the stall watchdog's backtraces (GRIMROCK_DEBUG_STALLS)
        target_link_options(${EXE} PRIVATE -rdynamic)
        # lib64/ next to the executable holds libraries the Steam Linux Runtime lacks (see
        # tools/deploy_steam.sh); a plain build only needs $ORIGIN for libsteam_api.so.
        set_target_properties(${EXE} PROPERTIES BUILD_RPATH "$ORIGIN/lib64:$ORIGIN"
                                                INSTALL_RPATH "$ORIGIN/lib64:$ORIGIN")
        if(GRIMROCK_HAVE_STEAM)
            # headers plus the redistributable libsteam_api.so, copied next to the executable
            # like the original's lib/ directory
            target_include_directories(${EXE} PRIVATE ${STEAMWORKS_DIR})
            target_link_libraries(${EXE} PRIVATE ${STEAMWORKS_DIR}/linux64/libsteam_api.so)
            add_custom_command(TARGET ${EXE} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${STEAMWORKS_DIR}/linux64/libsteam_api.so $<TARGET_FILE_DIR:${EXE}>)
        endif()
    endif()
endfunction()

add_game(1 grimrock grimrock_core grimrock_engine "${CORE_SOURCES}" "${ENGINE_SOURCES}" "${RAPID_SOURCES}" src)
game_sources(CORE2_SOURCES "${CORE_SOURCES}" core)
game_sources(ENGINE2_SOURCES "${ENGINE_SOURCES}" engine)
game_sources(RAPID2_SOURCES "${RAPID_SOURCES}" rapid)
add_game(2 grimrock2 grimrock2_core grimrock2_engine "${CORE2_SOURCES}" "${ENGINE2_SOURCES}" "${RAPID2_SOURCES}" "src2;src")

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
