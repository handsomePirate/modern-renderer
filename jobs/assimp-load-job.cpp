#include "assimp-load-job.h"

#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <stb/stb_image.h>

#include <filesystem>
#include <print>

using namespace svet::renderer;

namespace {

// Helper function to align a value to the next multiple of alignment
inline uint32_t alignUp(uint32_t value, uint32_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

// Get alignment for a given attribute size
uint32_t getAttributeAlignment(uint32_t elementSize) {
  // Align to element size, but typically GPU prefer 4 or 16 byte alignment
  // You can also use device.properties.limits.minStorageBufferOffsetAlignment
  return std::min(16u, alignUp(elementSize, 4u));
}

void determineSingleAttributeSizes(uint32_t elementSize, uint32_t elementCount,
                                   uint32_t &totalSize) {
  uint32_t size = elementSize * elementCount;
  uint32_t alignment = getAttributeAlignment(elementSize);
  totalSize = alignUp(totalSize, alignment);

  totalSize += size;
}

void determineBufferSize(const aiScene *scene, uint32_t &totalSize) {
  totalSize = 0;
  for (int i = 0; i < scene->mNumMeshes; ++i) {
    aiMesh *mesh = scene->mMeshes[i];
    uint32_t vertexCount = mesh->mNumVertices;
    uint32_t indexCount = mesh->mNumFaces * 3;
    if (mesh->HasPositions()) {
      determineSingleAttributeSizes(3 * sizeof(float), vertexCount, totalSize);
    }
    if (mesh->HasNormals()) {
      determineSingleAttributeSizes(3 * sizeof(float), vertexCount, totalSize);
    }
    if (mesh->mTangents) {
      determineSingleAttributeSizes(3 * sizeof(float), vertexCount, totalSize);
    }
    for (int j = 0; j < 8; ++j) {
      if (mesh->HasTextureCoords(j)) {
        determineSingleAttributeSizes(2 * sizeof(float), vertexCount,
                                      totalSize);
      }
    }
    if (mesh->HasFaces()) {
      determineSingleAttributeSizes(sizeof(uint32_t), indexCount, totalSize);
    }

    // TODO: bones indexes and weights
  }
}

void uploadMeshData(LContext context, uint32_t elementSize,
                    uint32_t elementCount, StagingBuffer stagingBuffer,
                    Buffer buffer, uint32_t attributeSignature, void *data,
                    uint32_t bufferIndex, uint32_t &bufferOffset,
                    SceneMesh &result) {
  uint32_t size = elementSize * elementCount;
  uint32_t alignment = getAttributeAlignment(elementSize);
  bufferOffset = alignUp(bufferOffset, alignment);

  StagedUploadSpecification stagedUploadSpec{};
  stagedUploadSpec.data = data;
  stagedUploadSpec.offset = bufferOffset;
  stagedUploadSpec.size = size;
  stagedUploadSpec.stagingBuffer = stagingBuffer;
  stagedUploadSpec.destination = buffer;
  stagedUploadSpec.destinationMeta.stage = PipelineStage::TRANSFER;
  stagedUploadSpec.destinationMeta.access = ResourceAccess::TRANSFER_WRITE;
  stagedUploadSpec.destinationMeta.ownership = QueueOwnershipState::TRANSFER;
  stagedUploadSpec.desiredOwnership = QueueOwnership::TRANSFER;
  stagedUploadData(context, stagedUploadSpec);

  SceneVertexAttribute attr;
  attr.signature = attributeSignature;
  attr.buffer = bufferIndex;
  attr.offset = bufferOffset;
  attr.size = size;
  result.vertexAttributes.push_back(std::move(attr));

  bufferOffset += size;
}

void loadMesh(LContext context, const aiMesh *mesh, bool rotateZUp, float scale,
              const glm::vec3 &move, StagingBuffer stagingBuffer, Buffer buffer,
              uint32_t bufferIndex, uint32_t &bufferOffset, SceneMesh &result) {
  uint32_t vertexCount = mesh->mNumVertices;
  uint32_t indexCount = mesh->mNumFaces * 3;
  if (mesh->HasPositions()) {
    uint32_t elementSize = 3 * sizeof(float);
    if (rotateZUp) {
      uint32_t size = 3 * vertexCount;
      float *transformedPositions = new float[size];
      for (int i = 0; i < vertexCount; ++i) {
        transformedPositions[i * 3 + 0] = mesh->mVertices[i].x * scale + move.x;
        transformedPositions[i * 3 + 1] =
            -mesh->mVertices[i].z * scale + move.y;
        transformedPositions[i * 3 + 2] = mesh->mVertices[i].y * scale + move.z;
      }
      uploadMeshData(context, elementSize, vertexCount, stagingBuffer, buffer,
                     positionsVASignature, transformedPositions, bufferIndex,
                     bufferOffset, result);
      delete[] transformedPositions;
    } else {
      uint32_t size = 3 * vertexCount;
      float *transformedPositions = new float[size];
      for (int i = 0; i < vertexCount; ++i) {
        transformedPositions[i * 3 + 0] = mesh->mVertices[i].x * scale + move.x;
        transformedPositions[i * 3 + 1] = mesh->mVertices[i].y * scale + move.y;
        transformedPositions[i * 3 + 2] = mesh->mVertices[i].z * scale + move.z;
      }
      uploadMeshData(context, elementSize, vertexCount, stagingBuffer, buffer,
                     positionsVASignature, transformedPositions, bufferIndex,
                     bufferOffset, result);
      delete[] transformedPositions;
    }
  }
  if (mesh->HasNormals()) {
    uint32_t elementSize = 3 * sizeof(float);
    if (rotateZUp) {
      uint32_t size = 3 * vertexCount;
      float *rotatedNormals = new float[size];
      for (int i = 0; i < vertexCount; ++i) {
        rotatedNormals[i * 3 + 0] = mesh->mNormals[i].x;
        rotatedNormals[i * 3 + 1] = -mesh->mNormals[i].z;
        rotatedNormals[i * 3 + 2] = mesh->mNormals[i].y;
      }
      uploadMeshData(context, elementSize, vertexCount, stagingBuffer, buffer,
                     normalsVASignature, rotatedNormals, bufferIndex,
                     bufferOffset, result);
      delete[] rotatedNormals;
    } else {
      uploadMeshData(context, elementSize, vertexCount, stagingBuffer, buffer,
                     normalsVASignature, mesh->mNormals, bufferIndex,
                     bufferOffset, result);
    }
  }
  if (mesh->mTangents) {
    uint32_t elementSize = 3 * sizeof(float);
    if (rotateZUp) {
      uint32_t size = 3 * vertexCount;
      float *rotatedTangents = new float[size];
      for (int i = 0; i < vertexCount; ++i) {
        rotatedTangents[i * 3 + 0] = mesh->mTangents[i].x;
        rotatedTangents[i * 3 + 1] = -mesh->mTangents[i].z;
        rotatedTangents[i * 3 + 2] = mesh->mTangents[i].y;
      }
      uploadMeshData(context, elementSize, vertexCount, stagingBuffer, buffer,
                     tangentsVASignature, rotatedTangents, bufferIndex,
                     bufferOffset, result);
      delete[] rotatedTangents;
    } else {
      uploadMeshData(context, elementSize, vertexCount, stagingBuffer, buffer,
                     tangentsVASignature, mesh->mTangents, bufferIndex,
                     bufferOffset, result);
    }
  }
  for (int i = 0; i < 8; ++i) {
    if (mesh->HasTextureCoords(i)) {
      uint32_t size = 2 * vertexCount;
      float *compressedUVs = new float[size];
      assert(mesh->mNumUVComponents[i] == 2);
      for (int j = 0; j < vertexCount; ++j) {
        compressedUVs[j * 2 + 0] = mesh->mTextureCoords[i][j].x;
        compressedUVs[j * 2 + 1] = mesh->mTextureCoords[i][j].y;
      }
      uploadMeshData(context, 2 * sizeof(float), vertexCount, stagingBuffer,
                     buffer, texCoordsVASignature, compressedUVs, bufferIndex,
                     bufferOffset, result);
      delete[] compressedUVs;
    }
  }
  if (mesh->HasFaces()) {
    uint32_t *indexes = new uint32_t[indexCount];
    for (unsigned int fi = 0; fi < mesh->mNumFaces; fi++) {
      aiFace face = mesh->mFaces[fi];
      assert(face.mNumIndices == 3);
      indexes[fi * 3 + 0] = face.mIndices[0];
      indexes[fi * 3 + 1] = face.mIndices[1];
      indexes[fi * 3 + 2] = face.mIndices[2];
    }
    uploadMeshData(context, sizeof(uint32_t), indexCount, stagingBuffer, buffer,
                   indexesVASignature, indexes, bufferIndex, bufferOffset,
                   result);
    delete[] indexes;
    result.elementCount = indexCount;
  } else {
    result.elementCount = vertexCount;
  }

  // TODO: bone indexes and weights
  result.material = mesh->mMaterialIndex;
  result.renderFlags = 0;
  result.instanceCount = 1;
}

void loadMeshes(LContext context, const aiScene *scene, bool rotateZUp,
                float scale, const glm::vec3 &move, StagingBuffer stagingBuffer,
                Buffer buffer, uint32_t bufferIndex, uint32_t &bufferOffset,
                SceneAddition &result) {
  for (int i = 0; i < scene->mNumMeshes; ++i) {
    const aiMesh *mesh = scene->mMeshes[i];
    SceneMesh sceneMesh{};
    loadMesh(context, mesh, rotateZUp, scale, move, stagingBuffer, buffer,
             bufferIndex, bufferOffset, sceneMesh);
    result.sceneMeshes.emplace_back(std::move(sceneMesh));
  }
}

void loadMaterial(LContext context, DescriptorPool descriptorPool,
                  MemoryPool imagePool, Sampler sampler,
                  const aiMaterial *material,
                  const std::filesystem::path &basePath,
                  StagingBuffer stagingBuffer, SceneMaterial &sceneMaterial,
                  std::vector<Image> &images,
                  std::vector<DescriptorSet> &descriptorSets) {
  // Helper lambda to load a texture of a specific type
  auto loadTextureType = [&](aiTextureType textureType,
                             uint32_t signature) -> uint32_t {
    aiString texturePath;
    if (material->GetTexture(textureType, 0, &texturePath) != AI_SUCCESS) {
      return UINT32_MAX;
    }

    // NOTE: On Windows, std::filesystem::path::c_str() returns const wchar_t*,
    // thus we need to convert to string to then be able to provide const char*
    // to STB
    auto path = (basePath / texturePath.C_Str()).string();
    int width, height, channels;
    unsigned char *imageData =
        stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!imageData) {
      std::println("STB failed to load image: {}", stbi_failure_reason());
      return UINT32_MAX; // Failed to load texture
    }

    size_t imageSize = width * height * 4; // RGBA8

    // Create the image
    ImageMetadata imageMeta{};
    ImageSpecification imageSpec{};
    imageSpec.width = width;
    imageSpec.height = height;
    imageSpec.pixelFormat = PixelFormat::UNORM8_RGBA;
    imageSpec.usage = ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST;
    imageSpec.tiling = ImageTiling::OPTIMAL;
    imageSpec.initialLayout = ImageLayout::UNDEFINED;
    imageSpec.outMeta = &imageMeta;
    imageSpec.memoryPool = imagePool;
    Image image = createImage(context, imageSpec);
    imageMeta.ownership = QueueOwnershipState::TRANSFER;

    StagedUploadImageSpecification stagedUploadSpec{};
    stagedUploadSpec.width = width;
    stagedUploadSpec.height = height;
    stagedUploadSpec.data = imageData;
    stagedUploadSpec.pixelSize = 4;
    stagedUploadSpec.stagingBuffer = stagingBuffer;
    stagedUploadSpec.destination = image;
    stagedUploadSpec.destinationMeta = imageMeta;
    stagedUploadSpec.desiredLayout = ImageLayout::SHADER_READ_ONLY;
    stagedUploadSpec.desiredOwnership = QueueOwnership::GRAPHICS;
    stagedUploadImageData(context, stagedUploadSpec);

    stbi_image_free(imageData);

    uint32_t imageIndex = images.size();
    images.push_back(image);
    uint32_t descriptorSetIndex = descriptorSets.size();

    // Create descriptor set entry for this texture
    SceneDescriptor descriptorSet{};
    descriptorSet.signature = signature;
    descriptorSet.asset = imageIndex;
    descriptorSet.descriptorSet = descriptorSetIndex;
    sceneMaterial.descriptors.push_back(descriptorSet);

    return imageIndex;
  };

  // Load standard PBR textures
  uint32_t albedoIndex =
      loadTextureType(aiTextureType_DIFFUSE, albedoTexDASignature);
  uint32_t normalIndex =
      loadTextureType(aiTextureType_NORMALS, normalTexDASignature);
  uint32_t metalroughIndex = loadTextureType(
      aiTextureType_GLTF_METALLIC_ROUGHNESS, metalroughTexDASignature);

  std::vector<ResourceType> types;
  types.reserve(4);
  std::vector<ShaderVisibility> visibilities;
  visibilities.reserve(4);
  std::vector<DescriptorUpdateSpecification> updateSpecs;
  updateSpecs.reserve(4);
  uint32_t offset = 0;
  if (albedoIndex != UINT32_MAX) {
    types.push_back(ResourceType::COMBINED_SAMPLER);
    visibilities.push_back(ShaderVisibility::FRAGMENT);
    updateSpecs.emplace_back(offset++, ResourceType::COMBINED_SAMPLER, nullptr,
                             images[albedoIndex], sampler);
  }
  if (normalIndex != UINT32_MAX) {
    types.push_back(ResourceType::COMBINED_SAMPLER);
    visibilities.push_back(ShaderVisibility::FRAGMENT);
    updateSpecs.emplace_back(offset++, ResourceType::COMBINED_SAMPLER, nullptr,
                             images[normalIndex], sampler);
  }
  if (metalroughIndex != UINT32_MAX) {
    types.push_back(ResourceType::COMBINED_SAMPLER);
    visibilities.push_back(ShaderVisibility::FRAGMENT);
    updateSpecs.emplace_back(offset++, ResourceType::COMBINED_SAMPLER, nullptr,
                             images[metalroughIndex], sampler);
  }

  if (types.size() > 0) {
    DescriptorSetLayoutSpecification setLayoutSpec{};
    setLayoutSpec.types = types.data();
    setLayoutSpec.visibilities = visibilities.data();
    setLayoutSpec.typeCount = types.size();
    auto setLayout = createDescriptorSetLayout(context, setLayoutSpec);

    auto set = createDescriptorSet(context, descriptorPool, setLayout);
    descriptorSets.push_back(set);

    DescriptorSetUpdateSpecification setUpdateSpec{};
    setUpdateSpec.descriptorSet = set;
    setUpdateSpec.specs = updateSpecs.data();
    setUpdateSpec.specCount = updateSpecs.size();
    updateDescriptorSet(context, setUpdateSpec);

    destroyDescriptorSetLayout(context, setLayout);
  }

  sceneMaterial.renderFlags = 0;

  aiString alphaMode;
  if (AI_SUCCESS == material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode)) {
    if (std::strcmp(alphaMode.C_Str(), "BLEND") == 0 |
        std::strcmp(alphaMode.C_Str(), "MASK") == 0) {
      sceneMaterial.renderFlags = OITMatFlag;
    }
  }

  float opacity = 1.0f;
  material->Get(AI_MATKEY_OPACITY, opacity);
  if (opacity < 1.0f) {
    sceneMaterial.renderFlags = OITMatFlag;
  }
}

void loadMaterials(LContext context, DescriptorPool descriptorPool,
                   MemoryPool imagePool, Sampler sampler, const aiScene *scene,
                   const std::filesystem::path &basePath,
                   StagingBuffer stagingBuffer, SceneAddition &result) {
  for (int i = 0; i < scene->mNumMaterials; ++i) {
    const aiMaterial *material = scene->mMaterials[i];
    SceneMaterial sceneMaterial{};
    loadMaterial(context, descriptorPool, imagePool, sampler, material,
                 basePath, stagingBuffer, sceneMaterial, result.images,
                 result.descriptorSets);
    result.sceneMaterials.emplace_back(std::move(sceneMaterial));
  }
}

} // namespace

JobResultCode processAssimpLoadGLMJob(AssimpLoadJob *job, LContext context,
                                      StagingBuffer stagingBuffer) {
  Assimp::Importer importer;
  const aiScene *scene = importer.ReadFile(
      job->file, aiProcess_Triangulate | aiProcess_ImproveCacheLocality |
                     aiProcess_FlipUVs | aiProcess_RemoveRedundantMaterials);

  if (not scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
      not scene->mRootNode) {
    return JobResultCode::ASSIMP_SCENE_NOT_LOADED;
  }

  uint32_t totalSize;
  determineBufferSize(scene, totalSize);

  BufferSpecification bufferSpec{};
  bufferSpec.size = totalSize;
  bufferSpec.initialOwnership = QueueOwnership::TRANSFER;
  bufferSpec.usage =
      BufferUsage::VERTEX | BufferUsage::INDEX | BufferUsage::TRANSFER_DST;
  bufferSpec.memoryPool = job->bufferPool;
  Buffer buffer = createBuffer(context, bufferSpec);
  if (not buffer) {
    std::println("[JobSystem] Failed to allocate a buffer");
    return JobResultCode::BUFFER_UPLOAD_ERROR;
  }
  job->sceneAddition.buffers.push_back(buffer);

  uint32_t bufferOffset = 0;
  // TODO: Staging buffer mishandled inside
  loadMeshes(context, scene, job->rotateZUp, job->scale, job->move,
             stagingBuffer, buffer, 0, bufferOffset, job->sceneAddition);

  std::filesystem::path basePath = job->file;
  basePath.remove_filename();
  // TODO: Staging buffer mishandled inside
  loadMaterials(context, job->descriptorPool, job->imagePool, job->sampler,
                scene, basePath, stagingBuffer, job->sceneAddition);

  BufferMetadata srcMeta{};
  srcMeta.stage = PipelineStage::TRANSFER;
  srcMeta.access = ResourceAccess::TRANSFER_WRITE;
  srcMeta.ownership = QueueOwnershipState::TRANSFER;
  BufferMetadata dstMeta{};
  dstMeta.stage = PipelineStage::BOTTOM_OF_PIPE;
  dstMeta.access = ResourceAccess::NONE;
  dstMeta.ownership = QueueOwnershipState::TRANSFER_RELEASED_TO_GRAPHICS;
  transitionBuffer(context, buffer, srcMeta, dstMeta);

  return JobResultCode::SUCCESS;
}
