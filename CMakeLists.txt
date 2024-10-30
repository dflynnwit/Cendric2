cmake_minimum_required(VERSION 3.15)

project(Cendric)

option(CENDRIC_BUILD_SQLITE_SHELL "Build SQLite shell on Windows platform?" OFF)
option(CENDRIC_BUILD_DIALOGUE_TOOL "Build Dialogue Tool on Windows platform?" ON)
option(CENDRIC_STEAM "Include steamworks API?" OFF)
option(CENDRIC_EXTERNAL_DOCUMENT_FOLDER "Use external documents folder?" OFF)
option(CENDRIC_GERMAN "Use German as default language?" OFF)
option(USE_SYSTEM_SFML "Use system SFML lib instead of internal" OFF)
option(USE_SYSTEM_PATHS "Use system paths for loading resources instead of local ones" OFF)

# Check SFML submodule
if (NOT IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ext/sfml/src AND NOT USE_SYSTEM_SFML)
    message(FATAL_ERROR
            "Some dependencies are missing. Clone the project with the --recursive flag or run 'git submodule update --init --recursive'."
    )
endif()

include(CheckCXXCompilerFlag)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# macOS settings
if (APPLE)
    option(SFML_BUILD_FRAMEWORKS "" ON)
    if (NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "macOS architecture to build; 64-bit is expected" FORCE)
    endif()
    if (NOT CMAKE_OSX_SYSROOT)
        execute_process(COMMAND xcodebuild -sdk macosx -version Path
                COMMAND head -n 1
                COMMAND tr -d '\n'
                OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
                ERROR_QUIET)
    endif()
endif()

# SFML setup
if (NOT USE_SYSTEM_SFML)
    add_subdirectory("${PROJECT_SOURCE_DIR}/ext/sfml" EXCLUDE_FROM_ALL)
else ()
    find_package(SFML 2.5 COMPONENTS graphics window audio system)
    if (NOT SFML_FOUND)
        message(FATAL_ERROR "System SFML package not found, set USE_SYSTEM_SFML to off")
    endif()
endif()

# Compiler-specific configurations
if (USE_SYSTEM_PATHS)
    add_definitions ("-DUSE_SYSTEM_PATHS")
endif()

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    add_definitions (/D "_CRT_SECURE_NO_WARNINGS")
endif()

# Enable modern C++ standards on GCC/Clang
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif()
    CHECK_CXX_COMPILER_FLAG("-std=c++14" HAS_CPP14_FLAG)
    CHECK_CXX_COMPILER_FLAG("-std=c++11" HAS_CPP11_FLAG)
    if (HAS_CPP14_FLAG)
        set(CMAKE_CXX_STANDARD 14)
    elseif (HAS_CPP11_FLAG)
        set(CMAKE_CXX_STANDARD 11)
    else()
        message(FATAL_ERROR "Unsupported compiler. At least C++11 support is required.")
    endif()
endif()

# Collect source files
file(GLOB_RECURSE Cendric_FILES
        "${PROJECT_SOURCE_DIR}/include/*.h"
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/ext/Particles/*.cpp"
        "${PROJECT_SOURCE_DIR}/ext/lua/src/*.c"
        "${PROJECT_SOURCE_DIR}/ext/sqlite/sqlite3.c"
        "${PROJECT_SOURCE_DIR}/ext/tinyxml2/*.cpp"
        if (CENDRIC_STEAM)
        "${PROJECT_SOURCE_DIR}/ext/steam-sdk/public/steam/*.h"
        "${PROJECT_SOURCE_DIR}/ext/steam-sdk/public/steam/*.cpp"
        endif()
)

# Windows-specific executable setup
if (WIN32)
    add_executable(Cendric
            ${Cendric_FILES}
            ${PROJECT_SOURCE_DIR}/res/info.rc
    )

    if (CENDRIC_STEAM)
        target_compile_definitions(Cendric PRIVATE STEAM)
        target_link_libraries(Cendric ${PROJECT_SOURCE_DIR}/ext/steam-sdk/redistributable_bin/win64/steam_api64.lib)
    endif()
elseif (APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")
    add_definitions("-DAPPLE_APP_BUILD")

    add_executable(Cendric MACOSX_BUNDLE
            ${Cendric_FILES}
            ${PROJECT_SOURCE_DIR}/res/macOS/icon.icns
    )
    if (CENDRIC_STEAM)
        add_definitions("-DSTEAM")
        target_link_libraries(Cendric ${PROJECT_SOURCE_DIR}/ext/steam-sdk/redistributable_bin/osx32/libsteam_api.dylib)
        install(FILES ${PROJECT_SOURCE_DIR}/ext/steam-sdk/redistributable_bin/osx32/libsteam_api.dylib
                DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Cendric.app/Contents/MacOS)
        install(FILES ${PROJECT_SOURCE_DIR}/res/macOS/steam_appid.txt
                DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Cendric.app/Contents/MacOS)
    endif()
    add_library(CendricApple STATIC "${PROJECT_SOURCE_DIR}/src/Platform/CendricApple.mm")
    target_include_directories(CendricApple PRIVATE "${PROJECT_SOURCE_DIR}/include")
    set_target_properties(CendricApple PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(Cendric CendricApple "-framework Cocoa" "-framework Foundation")
else()
    add_executable(Cendric ${Cendric_FILES})
    if (CENDRIC_STEAM)
        target_compile_definitions(Cendric PRIVATE STEAM)
        target_link_libraries(Cendric ${PROJECT_SOURCE_DIR}/ext/steam-sdk/redistributable_bin/linux64/libsteam_api.so)
    endif()
endif()

if (CENDRIC_EXTERNAL_DOCUMENT_FOLDER)
    add_definitions("-DEXTERNAL_DOCUMENTS_FOLDER")
endif()

if (CENDRIC_GERMAN)
    add_definitions("-DGERMAN_DEFAULT_LANGUAGE")
endif()

# Linking pthread and dl libraries
if (NOT WIN32)
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -lpthread -ldl")
else()
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -lpthread")
endif()

target_link_libraries(Cendric sfml-graphics sfml-window sfml-audio sfml-system)

target_include_directories(Cendric PRIVATE
        "${PROJECT_SOURCE_DIR}/ext/sfml/include"
        "${PROJECT_SOURCE_DIR}/ext"
        "${PROJECT_SOURCE_DIR}/include"
)
find_program(GPP_PATH NAMES g++.exe)
if(GPP_PATH)
    message("g++ found at: ${GPP_PATH}")
endif()

if (WIN32)
    # Define paths for resources and database folders
    set(RESOURCE_DIR "${PROJECT_SOURCE_DIR}/res")
    set(DB_DIR "${PROJECT_SOURCE_DIR}/db")

    # Define the destination directory in the build output
    set(DEST_DIR "${CMAKE_BINARY_DIR}")

    # Copy the resources directory
    add_custom_command(TARGET Cendric POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${RESOURCE_DIR}" "${DEST_DIR}/res"
            COMMENT "Copying resources to build directory."
    )

    # Copy the database directory
    add_custom_command(TARGET Cendric POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${DB_DIR}" "${DEST_DIR}/db"
            COMMENT "Copying database files to build directory."
    )
endif()


# Set paths for SFML and GCC DLLs
if (WIN32)

    # Define the path for OpenAL DLL
    set(OPENAL_DLL "${PROJECT_SOURCE_DIR}/ext/openal/openal32.dll")

    # Check if the OpenAL DLL exists at the expected path
    #if (EXISTS "${OPENAL_DLL}")
        message(STATUS "Found openal32.dll at ${OPENAL_DLL}")

    get_filename_component(CLION_MINGW_PATH ${CMAKE_CXX_COMPILER} DIRECTORY)
    message("CLion MinGW path: ${CLION_MINGW_PATH}  ")
    message("resources at: ${PROJECT_SOURCE_DIR}")


    set(SFML_DLL_DIR "${CMAKE_BINARY_DIR}/ext/sfml/lib")
    set(GCC_DLL_DIR "${CLION_MINGW_PATH}")

    # List of SFML DLLs required by the project
    set(DLLS
            "${SFML_DLL_DIR}/sfml-graphics-2.dll"
            "${SFML_DLL_DIR}/sfml-window-2.dll"
            "${SFML_DLL_DIR}/sfml-audio-2.dll"
            "${SFML_DLL_DIR}/sfml-system-2.dll"
            "${GCC_DLL_DIR}/libgcc_s_seh-1.dll"
            "${GCC_DLL_DIR}/libstdc++-6.dll"
            "${GCC_DLL_DIR}/libwinpthread-1.dll"
            "${OPENAL_DLL}"
    )

    # Add a custom command to copy each SFML DLL individually
    foreach(DLL ${DLLS})
        add_custom_command(TARGET Cendric POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL}" "${CMAKE_BINARY_DIR}"
                COMMENT "Copying ${DLL} to output directory."
        )
    endforeach()

    # Separate custom commands to copy the GCC runtime DLLs
    add_custom_command(TARGET Cendric POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GCC_DLL_DIR}/libgcc_s_seh-1.dll" "${CMAKE_BINARY_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GCC_DLL_DIR}/libstdc++-6.dll" "${CMAKE_BINARY_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GCC_DLL_DIR}/libwinpthread-1.dll" "${CMAKE_BINARY_DIR}"
            COMMENT "Copying GCC runtime DLLs to output directory."
    )
endif()




# Remaining original content...


if (WIN32 AND CENDRIC_BUILD_DIALOGUE_TOOL)


    file(GLOB_RECURSE DialogueTool_FILES
        "${PROJECT_SOURCE_DIR}/tools/DialogueTool/include/*.h"
        "${PROJECT_SOURCE_DIR}/tools/DialogueTool/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/ext/imgui/imgui.cpp"
        "${PROJECT_SOURCE_DIR}/ext/imgui/imgui_draw.cpp"
        "${PROJECT_SOURCE_DIR}/ext/imgui/imgui_demo.cpp"
        "${PROJECT_SOURCE_DIR}/ext/imgui-sfml/imgui-SFML.cpp"
    )

    add_executable(DialogueTool ${DialogueTool_FILES})

    find_package(OpenGL)

    target_link_libraries(DialogueTool sfml-graphics sfml-window sfml-system OpenGL::GL)

    target_include_directories(DialogueTool PRIVATE
        "${PROJECT_SOURCE_DIR}/ext/sfml/include"
        "${PROJECT_SOURCE_DIR}/ext/imgui"
        "${PROJECT_SOURCE_DIR}/ext/imgui-sfml"
        "${PROJECT_SOURCE_DIR}/ext"
        "${PROJECT_SOURCE_DIR}/tools/DialogueTool/include"
    )

endif()

if (APPLE)
    set_target_properties(Cendric PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/res/macOS/info.plist)
    set_source_files_properties(res/macOS/icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    install(FILES ${PROJECT_SOURCE_DIR}/db/game_data.db DESTINATION ./Cendric.app/Contents/Resources/db)
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/res DESTINATION ./Cendric.app/Contents/Resources)
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/saves DESTINATION ./Cendric.app/Contents/Resources)

    install(TARGETS Cendric BUNDLE DESTINATION .)
    install(TARGETS sfml-graphics sfml-window sfml-audio sfml-system FRAMEWORK DESTINATION ./Cendric.app/Contents/Frameworks)
    install(DIRECTORY ${FREETYPE_LIBRARY} ${FLAC_LIBRARY} ${VORBISENC_LIBRARY} ${VORBISFILE_LIBRARY} ${VORBIS_LIBRARY} ${OGG_LIBRARY} DESTINATION ./Cendric.app/Contents/Frameworks)
else()
    set(RESDIR "share/Cendric" CACHE STRING "Directory Game Ressources are installed to")
    set(BINDIR "bin" CACHE STRING "Directory Game Binary is installed to")

    if (IS_ABSOLUTE "${RESDIR}")
        add_definitions(-DRESDIR="${RESDIR}/")
    else()
        add_definitions(-DRESDIR="${CMAKE_INSTALL_PREFIX}/${RESDIR}/")
    endif()

    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/res" DESTINATION ${RESDIR})
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/db" DESTINATION ${RESDIR})
    install(TARGETS Cendric RUNTIME DESTINATION ${BINDIR})
endif()
