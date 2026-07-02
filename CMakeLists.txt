cmake_minimum_required(VERSION 4.2.0)
project(modern-renderer)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED 23)

find_package(Vulkan REQUIRED)
find_package(SDL3 REQUIRED)
find_package(glm REQUIRED)
find_package(slang REQUIRED)
find_package(assimp REQUIRED)

add_executable(modern-renderer)
target_sources(modern-renderer PRIVATE
  main.cpp
  renderer.cpp
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

add_compile_definitions(modern-renderer PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
add_compile_definitions(modern-renderer PRIVATE STB_IMAGE_IMPLEMENTATION)

#target_compile_options(modern-renderer PRIVATE -fsanitize=address)
#target_compile_options(modern-renderer PRIVATE -fsanitize=undefined)
#target_link_options(modern-renderer PRIVATE -fsanitize=address)
#target_link_options(modern-renderer PRIVATE -fsanitize=undefined)

target_link_libraries(modern-renderer PRIVATE Vulkan::Vulkan)
target_link_libraries(modern-renderer PRIVATE SDL3::SDL3)
target_link_libraries(modern-renderer PRIVATE glm::glm)
target_link_libraries(modern-renderer PRIVATE assimp::assimp)
target_include_directories(modern-renderer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(modern-renderer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ext)

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

add_dependencies(modern-renderer ${NAME}ShaderTarget)
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

add_dependencies(modern-renderer ${NAME}ShaderTarget)
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
add_custom_command(TARGET modern-renderer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory 
    "${CMAKE_CURRENT_BINARY_DIR}/${PATH}"
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/${PATH}/${NAME}"
    "${CMAKE_CURRENT_BINARY_DIR}/${PATH}/${NAME}"
  COMMENT "Copying files"
)
endfunction()
