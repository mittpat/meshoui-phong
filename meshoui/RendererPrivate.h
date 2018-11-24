#pragma once

#include <vulkan/vulkan.h>

#include "glslang.h"
#include "gltypes.h"
#include "Camera.h"
#include "Mesh.h"
#include "MeshLoader.h"

#include <hashid.h>
#include <string>
#include <vector>

struct GLFWwindow;
namespace Meshoui
{
    typedef ReflectionInfo ProgramUniform;
    typedef ReflectionInfo ProgramAttribute;

    class ProgramRegistration final
    {
    public:
        ~ProgramRegistration();
        ProgramRegistration();

        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSet;

        PipelineReflectionInfo pipelineReflectionInfo;
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
        GLuint buffer;
    };
    inline TextureRegistration::~TextureRegistration() {}
    inline TextureRegistration::TextureRegistration() : buffer(0) {}
    inline TextureRegistration::TextureRegistration(HashId n) : name(n), buffer(0) {}

    class MeshRegistration final
    {
    public:
        ~MeshRegistration();
        MeshRegistration();

        HashId definitionId;
        std::vector<unsigned int> indices;  //
        std::vector<Vertex>       vertices; //
        GLuint indexBuffer;
        GLuint vertexBuffer;
        size_t indexBufferSize;
        size_t vertexBufferSize;
        size_t referenceCount;
    };
    inline MeshRegistration::~MeshRegistration() {}
    inline MeshRegistration::MeshRegistration() : indexBuffer(0), vertexBuffer(0), indexBufferSize(0), vertexBufferSize(0), referenceCount(0) {}

    class IGraphics;
    class IUniform;
    class Program;
    typedef std::vector<std::pair<Program *, ProgramRegistration>> ProgramRegistrations;
    typedef std::vector<MeshRegistration> MeshRegistrations;
    typedef std::vector<TextureRegistration> TextureRegistrations;
    class RendererPrivate final
    {
    public:
        void unregisterProgram(ProgramRegistration & programRegistration);
        bool registerProgram(Program * program, ProgramRegistration & programRegistration);
        static void bindProgram(const ProgramRegistration & programRegistration);
        static void unbindProgram(const ProgramRegistration &);
        static void unregisterMesh(const MeshRegistration & meshRegistration);
        static void registerMesh(const MeshDefinition & meshDefinition, MeshRegistration & meshRegistration);
        static void bindMesh(const MeshRegistration & meshRegistration, const ProgramRegistration & programRegistration);
        static void unbindMesh(const MeshRegistration & meshRegistration, const ProgramRegistration & programRegistration);

        ~RendererPrivate();
        RendererPrivate();

        void selectSurfaceFormat(const VkFormat *request_formats, int request_formats_count, VkColorSpaceKHR request_color_space);
        void selectPresentMode(const VkPresentModeKHR *request_modes, int request_modes_count);

        void destroyGraphicsSubsystem();
        void createGraphicsSubsystem(const char* const* extensions, uint32_t extensions_count);
        void destroyCommandBuffers();
        void createCommandBuffers();
        void destroySwapChainAndFramebuffer();
        void createSwapChainAndFramebuffer(int w, int h);

        uint32_t memoryType(VkMemoryPropertyFlags properties, uint32_t type_bits);
        void createOrResizeBuffer(VkBuffer &buffer, VkDeviceMemory &buffer_memory, VkDeviceSize &p_buffer_size, size_t new_size, VkBufferUsageFlagBits usage);
        void renderDrawData(Program * program, Mesh * mesh,
                            const linalg::aliases::float4x4 &model, const linalg::aliases::float4x4 &view, const linalg::aliases::float4x4 &projection,
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
        void setProgramUniforms(Mesh * mesh);
        void setProgramUniform(Program * program, IUniform * uniform);
        void unsetProgramUniform(Program * program, IUniform * uniform);
        void draw(Program * program, Mesh * mesh);
        void fill(const std::string & filename, const std::vector<Mesh *> & meshes);
        const MeshFile & load(const std::string & filename);

        GLFWwindow*            window;
        VkAllocationCallbacks* allocator;
        VkInstance             instance;
        VkPhysicalDevice       physicalDevice;
        VkDevice               device;
        uint32_t               queueFamily;
        VkQueue                queue;
        VkPipelineCache        pipelineCache;
        VkDescriptorPool       descriptorPool;
        VkSurfaceKHR           surface;
        VkSurfaceFormatKHR     surfaceFormat;
        VkPresentModeKHR       presentMode;

        int                 width;
        int                 height;
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
        }                   frames[2];
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
