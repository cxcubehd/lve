cmake_minimum_required(VERSION 4.4.2)
project(lve)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(lve)
set_target_properties(lve PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(
    lve PUBLIC 
    ${PROJECT_SOURCE_DIR}/src
)

file(
    GLOB_RECURSE LVE_SOUCES 
    CONFIGURE_DEPENDS 
    "src/lve/*.cc" 
)
file(
    GLOB_RECURSE LVE_HEADERS
    CONFIGURE_DEPENDS 
    "src/lve/*.hh"
)
target_sources(lve PUBLIC ${LVE_SOUCES} ${LVE_HEADERS})

include(cmake/CPM.cmake)

# Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(lve PUBLIC Vulkan::Vulkan)

# SDL3
CPMAddPackage(
    NAME SDL
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.14
    GIT_SHALLOW
    OPTIONS
    "SDL_TEST_LIBRARY OFF"
    "SDL_STATIC ON"
    "SDL_X11_XSCRNSAVER OFF"
)
target_link_libraries(lve PUBLIC SDL3::SDL3-static)

# glm
CPMAddPackage("gh:g-truc/glm#1.0.3")
target_link_libraries(lve PUBLIC glm::glm)

# EnTT
CPMAddPackage("gh:skypjack/entt@4.0.0")
target_link_libraries(lve PUBLIC EnTT::EnTT)

# VulkanHpp
CPMAddPackage(
    NAME VulkanHpp
    GITHUB_REPOSITORY KhronosGroup/Vulkan-Hpp
    GIT_TAG v1.4.360
    GIT_SHALLOW
    OPTIONS
    "VULKAN_HPP_NO_CONSTRUCTORS ON"
)
target_link_libraries(lve PUBLIC VulkanHpp)
target_compile_definitions(lve PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC)

# Box3D
CPMAddPackage(
    NAME box3d
    GITHUB_REPOSITORY erincatto/box3d
    GIT_TAG 30c67b5e6d0a3a66f0f506c69ce9e9e0587e3b7c
    GIT_SHALLOW
)
target_link_libraries(lve PUBLIC box3d::box3d)

# GameNetworkingSockets
CPMAddPackage("gh:ValveSoftware/GameNetworkingSockets@1.6.0")
target_link_libraries(lve PUBLIC GameNetworkingSockets::static)


target_compile_definitions(lve PUBLIC L_RENDER)
target_compile_definitions(lve PUBLIC L_STEAM)

target_compile_definitions(lve PRIVATE $<$<CONFIG:Debug>:L_DEBUG>)
