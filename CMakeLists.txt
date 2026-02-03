cmake_minimum_required(VERSION 3.15)
# Allow older CMake policies for dependencies
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(PortalRendering VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM (use newer version for CMake compatibility)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# GLEW
FetchContent_Declare(
    glew
    GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
    GIT_TAG        glew-cmake-2.2.0
)
FetchContent_MakeAvailable(glew)

find_package(OpenGL REQUIRED)

set(SOURCES
    main_example.cpp
    PortalRenderer.cpp
)

set(HEADERS
    PortalMath.h
    PortalRenderer.h
    PortalTeleporter.h
)

add_executable(PortalDemo ${SOURCES} ${HEADERS})

target_link_libraries(PortalDemo PRIVATE
    OpenGL::GL
    glfw
    libglew_static
    glm::glm
)

add_custom_command(TARGET PortalDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:PortalDemo>/shaders
)

target_include_directories(PortalDemo PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${glew_SOURCE_DIR}/include
)

target_compile_definitions(PortalDemo PRIVATE
    GLM_FORCE_RADIANS
    GLEW_STATIC
)

if(WIN32)
    target_compile_definitions(PortalDemo PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
endif()

message(STATUS "Portal Rendering Demo configured!")
