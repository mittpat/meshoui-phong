#pragma once

#include <vulkan/vulkan.h>

#include "Camera.h"
#include "Mesh.h"
#include "MeshLoader.h"
#include "RenderDevice.h"

#include <hashid.h>
#include <string>
#include <vector>

struct GLFWwindow;
namespace Meshoui
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

    class ProgramRegistration final
    {
    public:
        ~ProgramRegistration();
        ProgramRegistration();

        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSet;
    };
    inline ProgramRegistration::~ProgramRegistration() {}
    inline ProgramRegistration::ProgramRegistration() : pipelineLayout(VK_NULL_HANDLE), pipeline(VK_NULL_HANDLE), descriptorSetLayout(VK_NULL_HANDLE), descriptorSet(VK_NULL_HANDLE) {}

    class TextureRegistration final
    {
    public:
        ~TextureRegistration();
        TextureRegistration();
        TextureRegistration(HashId n);

        HashId name;
        //
    };
    inline TextureRegistration::~TextureRegistration() {}
    inline TextureRegistration::TextureRegistration() {}
    inline TextureRegistration::TextureRegistration(HashId n) : name(n) {}

    class MeshRegistration final
    {
    public:
        ~MeshRegistration();
        MeshRegistration();

        HashId definitionId;
        DeviceBuffer vertexBuffer;
        DeviceBuffer indexBuffer;
        size_t indexBufferSize;
        size_t referenceCount;
    };
    inline MeshRegistration::~MeshRegistration() {}
    inline MeshRegistration::MeshRegistration() : indexBufferSize(0), referenceCount(0) {}

    class IGraphics;
    class IUniform;
    class Program;
    typedef std::vector<std::pair<Program *, ProgramRegistration>> ProgramRegistrations;
    typedef std::vector<MeshRegistration> MeshRegistrations;
    typedef std::vector<TextureRegistration> TextureRegistrations;
    class RendererPrivate final
    {
    public:
        static const int FrameCount = 2;

        void unregisterProgram(ProgramRegistration & programRegistration);
        bool registerProgram(Program * program, ProgramRegistration & programRegistration);
        void bindProgram(const ProgramRegistration & programRegistration);
        void unbindProgram(const ProgramRegistration &);
        void unregisterMesh(const MeshRegistration & meshRegistration);
        void registerMesh(const MeshDefinition & meshDefinition, MeshRegistration & meshRegistration);
        void bindMesh(const MeshRegistration & meshRegistration, const ProgramRegistration &);
        void unbindMesh(const MeshRegistration &, const ProgramRegistration &);

        ~RendererPrivate();
        RendererPrivate();

        void selectSurfaceFormat(const VkFormat *request_formats, int request_formats_count, VkColorSpaceKHR request_color_space);
        void destroyGraphicsSubsystem();
        void createGraphicsSubsystem(const char* const* extensions, uint32_t extensions_count);
        void destroyCommandBuffers();
        void createCommandBuffers();
        void destroySwapChainAndFramebuffer();
        void createSwapChainAndFramebuffer(int w, int h);

        void renderDrawData(Program * program, Mesh * mesh, const PushConstant & pushConstants,
                            const linalg::aliases::float3 &position, const linalg::aliases::float3 &light);

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
        void draw(Program * program, Mesh * mesh);
        void fill(const std::string & filename, const std::vector<Mesh *> & meshes);
        const MeshFile & load(const std::string & filename);

        GLFWwindow*            window;
        VkInstance             instance;
        RenderDevice           renderDevice;
        uint32_t               queueFamily;
        VkQueue                queue;
        VkPipelineCache        pipelineCache;
        VkDescriptorPool       descriptorPool;
        VkSurfaceKHR           surface;
        VkSurfaceFormatKHR     surfaceFormat;

        uint32_t            width;
        uint32_t            height;
        VkSwapchainKHR      swapchain;
        VkRenderPass        renderPass;
        uint32_t            backBufferCount;
        VkImage             backBuffer[16];
        VkImageView         backBufferView[16];
        VkFramebuffer       framebuffer[16];
        struct FrameData
        {
            uint32_t        backbufferIndex;
            VkCommandPool   commandPool;
            VkCommandBuffer commandBuffer;
            VkFence         fence;
            VkSemaphore     imageAcquiredSemaphore;
            VkSemaphore     renderCompleteSemaphore;
            FrameData();
        }                   frames[FrameCount];
        uint32_t            frameIndex;

        ProgramRegistrations programRegistrations;
        MeshRegistrations    meshRegistrations;
        TextureRegistrations textureRegistrations;
        MeshFiles meshFiles;

        bool toFullscreen;
        bool fullscreen;

        linalg::aliases::float4x4 projectionMatrix;
        Camera * camera;
        std::vector<Light *> lights;
    };
    inline RendererPrivate::~RendererPrivate() {}
    inline RendererPrivate::FrameData::FrameData()
    {
        backbufferIndex = 0;
        commandPool = VK_NULL_HANDLE;
        commandBuffer = VK_NULL_HANDLE;
        fence = VK_NULL_HANDLE;
        imageAcquiredSemaphore = VK_NULL_HANDLE;
        renderCompleteSemaphore = VK_NULL_HANDLE;
    }
}
