#pragma once

#include <vulkan/vulkan.h>

#include "Camera.h"
#include "Mesh.h"
#include "MeshLoader.h"
#include "DeviceVk.h"
#include "InstanceVk.h"
#include "SwapChainVk.h"

#include <hashid.h>
#include <string>
#include <vector>

struct GLFWwindow;
namespace Meshoui
{
    namespace Blocks
    {
        struct PushConstant
        {
            ~PushConstant();
            PushConstant();

            linalg::aliases::float4x4 model;
            linalg::aliases::float4x4 view;
            linalg::aliases::float4x4 projection;
        };
        inline PushConstant::~PushConstant() {}
        inline PushConstant::PushConstant() : model(linalg::identity), view(linalg::identity), projection(linalg::identity) {}

        struct Uniform
        {
            ~Uniform();
            Uniform();

            alignas(16) linalg::aliases::float3 position;
            alignas(16) linalg::aliases::float3 light;
        };
        inline Uniform::~Uniform() {}
        inline Uniform::Uniform() : position(linalg::zero), light(linalg::zero) {}

        struct PhongMaterial
        {
            ~PhongMaterial();
            PhongMaterial();

            alignas(16) linalg::aliases::float3 ambient;
            alignas(16) linalg::aliases::float3 diffuse;
            alignas(16) linalg::aliases::float3 specular;
            alignas(16) linalg::aliases::float3 emissive;
        };
        inline PhongMaterial::~PhongMaterial() {}
        inline PhongMaterial::PhongMaterial() : ambient(linalg::zero), diffuse(0.64f, 0.64f, 0.64f), specular(0.5f, 0.5f, 0.5f), emissive(linalg::zero) {}
    }

#define MESHOUI_PROGRAM_DESC_LAYOUT 0
#define MESHOUI_MATERIAL_DESC_LAYOUT 1
    class ProgramPrivate final
    {
    public:
        ~ProgramPrivate();
        ProgramPrivate();

        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
        VkDescriptorSetLayout descriptorSetLayout[MESHOUI_MATERIAL_DESC_LAYOUT+1];
        VkDescriptorSet descriptorSet[FrameCount];
        DeviceBufferVk uniformBuffer[FrameCount];
        DeviceBufferVk materialBuffer[FrameCount];
    };
    inline ProgramPrivate::~ProgramPrivate() {}
    inline ProgramPrivate::ProgramPrivate() : pipelineLayout(VK_NULL_HANDLE), pipeline(VK_NULL_HANDLE) {}

    class MaterialPrivate final
    {
    public:
        ~MaterialPrivate();
        MaterialPrivate();

        HashId name;
        VkSampler diffuseSampler;
        VkSampler normalSampler;
        VkSampler specularSampler;
        VkSampler emissiveSampler;
        VkDescriptorSet descriptorSet[FrameCount];
        ImageBufferVk diffuseImage;
        ImageBufferVk normalImage;
        ImageBufferVk specularImage;
        ImageBufferVk emissiveImage;
        size_t referenceCount;
    };
    inline MaterialPrivate::~MaterialPrivate() {}
    inline MaterialPrivate::MaterialPrivate() : referenceCount(0), diffuseSampler(VK_NULL_HANDLE), normalSampler(VK_NULL_HANDLE), specularSampler(VK_NULL_HANDLE), emissiveSampler(VK_NULL_HANDLE) {}

    class MeshPrivate final
    {
    public:
        ~MeshPrivate();
        MeshPrivate();

        HashId definitionId;
        DeviceBufferVk vertexBuffer;
        DeviceBufferVk indexBuffer;
        size_t indexBufferSize;
        size_t referenceCount;
    };
    inline MeshPrivate::~MeshPrivate() {}
    inline MeshPrivate::MeshPrivate() : indexBufferSize(0), referenceCount(0) {}

    class Renderer;
    class RendererPrivate final
    {
    public:
        ~RendererPrivate();
        RendererPrivate() = delete;
        RendererPrivate(Renderer * r);

        void registerGraphics(Model * model);
        void registerGraphics(Mesh * mesh);
        void registerGraphics(Program * program);
        void registerGraphics(Camera * cam);
        void registerGraphics(const MeshFile & meshFile);
        void unregisterGraphics(Model * model);
        void unregisterGraphics(Mesh * mesh);
        void unregisterGraphics(Program * program);
        void unregisterGraphics(Camera * cam);
        void bindGraphics(Mesh * mesh);
        void bindGraphics(Program * program);
        void bindGraphics(Camera * cam, bool asLight = false);
        void unbindGraphics(Mesh * mesh);
        void unbindGraphics(Program * program);
        void unbindGraphics(Camera * cam);
        void draw(Mesh * mesh);
        void fill(const std::string & filename, const std::vector<Mesh *> & meshes);
        const MeshFile & load(const std::string & filename);

        Renderer*          renderer;
        GLFWwindow*        window;
        InstanceVk         instance;
        DeviceVk           device;
        SwapChainVk        swapChain;
        uint32_t           frameIndex;
        VkPipelineCache    pipelineCache;
        VkSurfaceKHR       surface;
        VkSurfaceFormatKHR surfaceFormat;

        ImageBufferVk  depthBuffer;

        MeshFiles meshFiles;

        bool toFullscreen;
        bool isFullscreen;

        bool toVSync;
        bool isVSync;

        linalg::aliases::float4x4 projectionMatrix;
        Camera * camera;
        std::vector<Light *> lights;

        Blocks::PushConstant pushConstants;
        Blocks::Uniform uniforms;
    };
}

inline Meshoui::RendererPrivate::~RendererPrivate() {}
