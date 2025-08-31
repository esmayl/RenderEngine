cmake_minimum_required(VERSION 3.28)
project(RenderEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Source Files ---
# Automatically discover all source files in the Source directory
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "Source/*.cpp" "Source/*.h")
add_executable(RenderEngine WIN32 ${SOURCE_FILES})

# --- Dependencies ---
# vcpkg will find these packages using the toolchain file configured in CLion
find_package(tinyxml2 CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)

# --- Include Directories ---
target_include_directories(RenderEngine PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
        )

# --- Set Unicode Character Set ---
# Define UNICODE and _UNICODE to ensure Windows APIs use wide-character strings
target_compile_definitions(RenderEngine PRIVATE
        UNICODE
        _UNICODE
        )

# --- Link Libraries ---
# Use modern CMake targets provided by vcpkg for linking
target_link_libraries(RenderEngine PRIVATE
        d3d11.lib
        dxgi.lib
        d3dcompiler.lib
        tinyxml2::tinyxml2
        Microsoft::DirectXTK
        )

# --- Shader Compilation Function ---
# A robust function to compile different types of shaders
function(compile_shader SHADER_NAME SHADER_TYPE)
    set(HLSL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Source/Shaders/${SHADER_NAME}.hlsl")
    set(CSO_FILE "${CMAKE_BINARY_DIR}/${SHADER_NAME}.cso")

    add_custom_command(
            OUTPUT ${CSO_FILE}
            COMMAND fxc /T ${SHADER_TYPE} /E main /Fo ${CSO_FILE} ${HLSL_FILE}
            DEPENDS ${HLSL_FILE}
            COMMENT "Compiling ${SHADER_NAME}.hlsl"
    )

    # Add the compiled shader to a global list
    set(COMPILED_SHADER_FILES ${COMPILED_SHADER_FILES} ${CSO_FILE} PARENT_SCOPE)
endfunction()

# Collect all HLSL files
file(GLOB ALL_HLSL "${CMAKE_CURRENT_SOURCE_DIR}/Source/Shaders/*.hlsl")

# Reset accumulator the function appends to
set(COMPILED_SHADER_FILES)

foreach(HLSL ${ALL_HLSL})
    get_filename_component(SHADER_NAME "${HLSL}" NAME_WE)

    # Infer profile from filename suffix
    if (SHADER_NAME MATCHES "ComputeShader$")
        set(SHADER_PROFILE cs_5_0)
    elseif (SHADER_NAME MATCHES "VertexShader$")
        set(SHADER_PROFILE vs_5_0)
    elseif (SHADER_NAME MATCHES "PixelShader$")
        set(SHADER_PROFILE ps_5_0)
    elseif (SHADER_NAME MATCHES "GeometryShader$")
        set(SHADER_PROFILE gs_5_0)
    else()
        message(STATUS "Skipping ${SHADER_NAME}.hlsl (unknown suffix)")
        continue()
    endif()

    compile_shader("${SHADER_NAME}" "${SHADER_PROFILE}")
endforeach()

# Make a build target and hook your exe to it
add_custom_target(Shaders ALL DEPENDS ${COMPILED_SHADER_FILES})
add_dependencies(RenderEngine Shaders)

# Copy fonts
add_custom_command(TARGET RenderEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Fonts"
        "$<TARGET_FILE_DIR:RenderEngine>"
        COMMENT "Copying fonts to output directory")

# --- Tests ---
include(CTest)
if (BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)

    add_executable(RenderEngineTests
            tests/RandomGeneratorTests.cpp
            Source/RandomGenerator.cpp)

    target_include_directories(RenderEngineTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source")

    target_compile_definitions(RenderEngineTests PRIVATE
            UNICODE
            _UNICODE)

    target_link_libraries(RenderEngineTests PRIVATE
            GTest::gtest
            GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(RenderEngineTests)
endif ()
