cmake_minimum_required(VERSION 4.2.0)
project(renderer)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED 23)

include(GNUInstallDirs)
set(SVET_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR}/svet/renderer)
set(SVET_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/svet-renderer)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

find_package(VulkanLoader REQUIRED)
find_package(VulkanHeaders REQUIRED)
find_package(SDL3 REQUIRED)
find_package(glm REQUIRED)
find_package(slang REQUIRED)
find_package(assimp REQUIRED)

# Actual library
add_library(renderer SHARED)
target_sources(renderer
PRIVATE
  svet/renderer/staging.cpp
  svet/renderer/vulkan/buffer.cpp
  svet/renderer/vulkan/context.cpp
  svet/renderer/vulkan/image.cpp
  svet/renderer/vulkan/sampler.cpp
  svet/renderer/vulkan/shader.cpp
  svet/renderer/vulkan/pipeline.cpp
  svet/renderer/vulkan/descriptor.cpp
  svet/renderer/vulkan/render-pass.cpp
  svet/renderer/vulkan/swapchain.cpp
  svet/renderer/vulkan/draw-processor.cpp
  svet/renderer/vulkan/memory.cpp
PRIVATE FILE_SET privHeaders TYPE HEADERS
  BASE_DIRS svet/renderer
PUBLIC FILE_SET HEADERS
  FILES
    svet/renderer/buffer.h
    svet/renderer/context.h
    svet/renderer/descriptor.h
    svet/renderer/draw-processor.h
    svet/renderer/enums.h
    svet/renderer/image.h
    svet/renderer/memory.h
    svet/renderer/pipeline.h
    svet/renderer/render-pass.h
    svet/renderer/sampler.h
    svet/renderer/shader.h
    svet/renderer/staging.h
    svet/renderer/swapchain.h
)

if (ASAN)
  target_compile_options(renderer PRIVATE -fsanitize=address)
  target_link_options(renderer PRIVATE -fsanitize=address)
endif()

target_link_libraries(renderer 
  PRIVATE Vulkan::Loader Vulkan::Headers
  PUBLIC SDL3::SDL3
)

install(TARGETS renderer
  EXPORT svet-renderer-targets
  LIBRARY DESTINATION ${SVET_INSTALL_LIBDIR}
  FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT svet-renderer-targets
  DESTINATION ${SVET_INSTALL_CMAKEDIR}
  NAMESPACE svet::
)

install(FILES
  cmake/svet-renderer-config.cmake
  DESTINATION ${SVET_INSTALL_CMAKEDIR}
)

# Testing executable
add_executable(renderer-exe)
target_sources(renderer-exe PRIVATE
  main.cpp
  image-gen.cpp
  flycam.cpp
  file.cpp
  passes/deferred-base.cpp
  passes/deferred-shading.cpp
  passes/geometry-pass.cpp
  passes/overlay-pass.cpp
  passes/shadow-pass.cpp
  passes/weighted-oit.cpp
  scene.cpp
  job-manager.cpp
  pipelines/deferred-pipeline.cpp
  pipelines/geometry-pipeline.cpp
  jobs/assimp-load-job.cpp
  jobs/job-system.cpp
  jobs/gpu-upload-job.cpp
)

target_compile_definitions(renderer-exe PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(renderer-exe PRIVATE STB_IMAGE_IMPLEMENTATION)

if (ASAN)
  target_compile_options(renderer-exe PRIVATE -fsanitize=address)
  #target_compile_options(renderer-exe PRIVATE -fsanitize=undefined)
  #target_compile_options(renderer-exe PRIVATE -fsanitize=alignment)
  target_link_options(renderer-exe PRIVATE -fsanitize=address)
  #target_link_options(renderer-exe PRIVATE -fsanitize=undefined)
  #target_link_options(renderer-exe PRIVATE -fsanitize=alignment)
endif()

target_link_libraries(renderer-exe PRIVATE glm::glm)
target_link_libraries(renderer-exe PRIVATE assimp::assimp)
target_link_libraries(renderer-exe PRIVATE renderer)

target_include_directories(renderer-exe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(renderer-exe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ext)

function(compileGraphicsShader NAME)
set(FILE_NAME ${NAME}.slang)
set(VERT_NAME ${NAME}.vert.spv)
set(FRAG_NAME ${NAME}.frag.spv)

set(SLANG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${FILE_NAME})
set(VERT_FILE_SPV ${CMAKE_CURRENT_BINARY_DIR}/shaders/${VERT_NAME})
set(FRAG_FILE_SPV ${CMAKE_CURRENT_BINARY_DIR}/shaders/${FRAG_NAME})
file(GLOB SHARED_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shared/*")

add_custom_command(OUTPUT 
  ${VERT_FILE_SPV}
  ${FRAG_FILE_SPV}
  COMMAND slang::slangc ${SLANG_FILE} -entry vertexMain -target spirv -stage vertex -o ${VERT_FILE_SPV}
  COMMAND slang::slangc ${SLANG_FILE} -entry fragmentMain -target spirv -stage pixel -o ${FRAG_FILE_SPV}
  DEPENDS ${SLANG_FILE} ${SHARED_SHADERS}
  COMMENT "Compiling ${SLANG_FILE} shaders"
)

add_custom_target(${NAME}ShaderTarget ALL DEPENDS 
  ${VERT_FILE_SPV}
  ${FRAG_FILE_SPV}
)

add_dependencies(renderer-exe ${NAME}ShaderTarget)
endfunction()

function(compileComputeShader NAME)
set(FILE_NAME ${NAME}.slang)
set(COMP_NAME ${NAME}.comp.spv)

set(SLANG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${FILE_NAME})
set(COMP_FILE_SPV ${CMAKE_CURRENT_BINARY_DIR}/shaders/${COMP_NAME})
file(GLOB SHARED_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shared/*")

add_custom_command(OUTPUT 
  ${COMP_FILE_SPV}
  COMMAND slang::slangc ${SLANG_FILE} -entry main -target spirv -stage compute -o ${COMP_FILE_SPV}
  DEPENDS ${SLANG_FILE} ${SHARED_SHADERS}
  COMMENT "Compiling ${SLANG_FILE} shaders"
)

add_custom_target(${NAME}ShaderTarget ALL DEPENDS 
  ${COMP_FILE_SPV}
)

add_dependencies(renderer-exe ${NAME}ShaderTarget)
endfunction()

compileGraphicsShader(geometry)
compileGraphicsShader(deferred-base)
compileComputeShader(deferred-shade)
compileGraphicsShader(shadow)
compileGraphicsShader(weighted-oit)
compileComputeShader(weighted-oit-resolve)
compileComputeShader(overlay)
compileComputeShader(blur)

function(copyFilePath PATH NAME)
add_custom_command(TARGET renderer-exe POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory 
    "${CMAKE_CURRENT_BINARY_DIR}/${PATH}"
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/${PATH}/${NAME}"
    "${CMAKE_CURRENT_BINARY_DIR}/${PATH}/${NAME}"
  COMMENT "Copying files"
)
endfunction()
