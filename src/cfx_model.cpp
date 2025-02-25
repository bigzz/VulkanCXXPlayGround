#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "cfx_model.hpp"
#include "cfx_utils.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <unordered_map>

namespace std
{
    template <>
    struct hash<cfx::CFXModel::Vertex>
    {
        size_t operator()(cfx::CFXModel::Vertex const &vertex) const
        {
            size_t seed = 0;
            cfx::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);

            // cout << "SEED " << seed << endl;
            return seed;
        }
    };
}

namespace cfx
{
    CFXModel::CFXModel(CFXDevice &device, const CFXModel::Builder &builder) : cfxDevice{device}
    {
        vertexBuffer.resize(cfxDevice.getDevicesinDeviceGroup());
        indexBuffer.resize(cfxDevice.getDevicesinDeviceGroup());
        for (int i = 0; i < cfxDevice.getDevicesinDeviceGroup(); i++)
        {
            createVertexBuffers(builder.vertices, i);
            createIndexBuffers(builder.indices, i);
        }
    }
    CFXModel::~CFXModel()
    {
    }
    std::unique_ptr<CFXModel> CFXModel::createModelFromFile(CFXDevice &device, const std::string &filepath)
    {
        Builder builder{};
        builder.loadModel(filepath);
        // std::cout << "Vertex Count "<< builder.vertices.size() << std::endl;
        return std::make_unique<CFXModel>(device, builder);
    }
    void CFXModel::createVertexBuffers(const std::vector<Vertex> &vertices, int deviceIndex)
    {
        vertexCount = static_cast<uint32_t>(vertices.size());
        assert(vertexCount >= 3 && "Vertex count must be at least 3");
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
        uint32_t vertexSize = sizeof(vertices[0]);
        CFXBuffer stagingBuffer{cfxDevice, vertexSize, vertexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, deviceIndex};
        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void *)vertices.data());

        vertexBuffer[deviceIndex] = std::make_unique<CFXBuffer>(cfxDevice, vertexSize, vertexCount, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, deviceIndex);

        cfxDevice.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer[deviceIndex]->getBuffer(), bufferSize, deviceIndex);
    }

    void CFXModel::createIndexBuffers(const std::vector<uint32_t> &indices, int deviceIndex)
    {
        indexCount = static_cast<uint32_t>(indices.size());
        hasIndexBuffer = indexCount > 0;
        if (!hasIndexBuffer)
        {
            return;
        }
        VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
        uint32_t indexSize = sizeof(indices[0]);
        CFXBuffer stagingBuffer{cfxDevice, indexSize, indexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, deviceIndex};
        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void *)indices.data());

        indexBuffer[deviceIndex] = std::make_unique<CFXBuffer>(cfxDevice, indexSize, indexCount, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, deviceIndex);

        cfxDevice.copyBuffer(stagingBuffer.getBuffer(), indexBuffer[deviceIndex]->getBuffer(), bufferSize, deviceIndex);
    }
    void CFXModel::draw(VkCommandBuffer commandBuffer)
    {
        if (hasIndexBuffer)
        {
            vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
        }
    }

    void CFXModel::bind(VkCommandBuffer commandBuffer, int deviceIndex)
    {
        // std::cout << "BIND OBJECT TO COMMAND BUFFER " << deviceIndex <<std::endl;

        VkDeviceSize offsets[] = {0};
        VkBuffer buffers[] = {vertexBuffer[deviceIndex]->getBuffer()};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

        if (hasIndexBuffer)
        {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer[deviceIndex]->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        }
        // std::cout << "BIND OBJECT TO COMMAND BUFFER END" <<std::endl;
    }

    std::vector<VkVertexInputBindingDescription> CFXModel::Vertex::getBindingDescriptions()
    {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(Vertex);
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescriptions;
    }
    std::vector<VkVertexInputAttributeDescription> CFXModel::Vertex::getAttributeDescriptions()
    {
        std::vector<VkVertexInputAttributeDescription> inputAttributeDescriptions{};
        inputAttributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)});
        inputAttributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});
        inputAttributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
        inputAttributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});
        return inputAttributeDescriptions;
    }
    void CFXModel::Builder::loadModel(const std::string &filepath)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn;
        std::string error;
        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &error, filepath.c_str()))
        {
            throw std::runtime_error(warn + error);
        }
        vertices.clear();
        indices.clear();
        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

                for (const auto &shape : shapes)
        {
            for (const auto &index : shape.mesh.indices)
            {
                Vertex vertex{};

                if (index.vertex_index >= 0)
                {
                    vertex.position = {
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2],
                    };

                    vertex.color = {
                        attrib.colors[3 * index.vertex_index + 0],
                        attrib.colors[3 * index.vertex_index + 1],
                        attrib.colors[3 * index.vertex_index + 2],
                    };
                }

                if (index.normal_index >= 0)
                {
                    vertex.normal = {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2],
                    };
                }

                if (index.texcoord_index >= 0)
                {
                    vertex.uv = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        attrib.texcoords[2 * index.texcoord_index + 1],
                    };
                }

                if (uniqueVertices.count(vertex) == 0)
                {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }
                indices.push_back(uniqueVertices[vertex]);
            }
        }
    }

}