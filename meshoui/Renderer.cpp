#include <GL/glew.h>

#include "Renderer.h"
#include "RendererPrivate.h"
#include "Camera.h"
#include "Program.h"
#include "Uniform.h"
#include "Widget.h"

#include "loose.h"

#include <algorithm>
#include <set>

#include <SDL2/SDL_image.h>

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_opengl3.h>

using namespace linalg;
using namespace linalg::aliases;

Renderer::~Renderer()
{
    // imgui cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    // sdl & gl cleanup
    SDL_GL_DeleteContext(d->glContext);
    SDL_DestroyWindow(d->window);
    delete d;

    IMG_Quit();
}

Renderer::Renderer(bool gles)
    : defaultProgram(nullptr)
    , d(new RendererPrivate)
    , meshes()
    , programs()
{
    // sdl & gl
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);

    if (gles)
    {
        // GLES 2.0, GLSL 320
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    }
    else
    {
        // LIBGL_ALWAYS_SOFTWARE mesa version (Raspberry PI + Intel GMA900M support)
        // Geforce 8xxx and up
        // Radeon HD 4xxx and up
        // OpenGL 3.3, GLSL 150
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    }

    d->window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1920/2, 1080/2, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
    //SDL_SetWindowFullscreen(d->window, SDL_WINDOW_FULLSCREEN);
    d->glContext = SDL_GL_CreateContext(d->window);

    // glew
    glewExperimental = GL_TRUE;
    d->glewError = glewInit();

    // imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForOpenGL(d->window, d->glContext);
    ImGui_ImplOpenGL3_Init("#version 150");
    ImGui::StyleColorsDark();
}

void Renderer::add(Mesh * mesh)
{
    if (std::find(meshes.begin(), meshes.end(), mesh) == meshes.end())
    {
        meshes.push_back(mesh);
        if (!mesh->program)
            mesh->program = defaultProgram;
        d->registerGraphics(mesh);
    }
}

void Renderer::add(Program * program)
{
    if (std::find(programs.begin(), programs.end(), program) == programs.end())
    {
        programs.push_back(program);
        d->registerGraphics(program);
    }
    if (defaultProgram == nullptr)
        defaultProgram = program;
}

void Renderer::add(Camera * camera)
{
    if (std::find(cameras.begin(), cameras.end(), camera) == cameras.end())
    {
        cameras.push_back(camera);
        d->registerGraphics(camera);
    }
}

void Renderer::add(Widget * widget)
{
    if (std::find(widgets.begin(), widgets.end(), widget) == widgets.end())
    {
        widgets.push_back(widget);
    }
}

void Renderer::remove(Mesh* mesh)
{
    d->unregisterGraphics(mesh);
    if (mesh->program == defaultProgram)
        mesh->program = nullptr;
    meshes.erase(std::remove(meshes.begin(), meshes.end(), mesh));
}

void Renderer::remove(Program* program)
{
    d->unregisterGraphics(program);
    programs.erase(std::remove(programs.begin(), programs.end(), program));
    if (defaultProgram == program)
        defaultProgram = nullptr;
}

void Renderer::remove(Camera* camera)
{
    d->unregisterGraphics(camera);
    cameras.erase(std::remove(cameras.begin(), cameras.end(), camera));
}

void Renderer::remove(Widget * widget)
{
    widgets.erase(std::remove(widgets.begin(), widgets.end(), widget));
}

void Renderer::update(double)
{
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glCullFace(GL_BACK);

    SDL_DisplayMode mode;
    SDL_GetWindowDisplayMode(d->window, &mode);
    glViewport(0, 0, mode.w, mode.h);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    renderMeshes();
    renderWidgets();

    SDL_GL_SwapWindow(d->window);

    postUpdate();
}

void Renderer::postUpdate()
{
    if (d->toFullscreen && !d->fullscreen)
    {
        SDL_DisplayMode mode;
        SDL_GetWindowDisplayMode(d->window, &mode);
        mode.w = 1920;
        mode.h = 1080;
        SDL_SetWindowDisplayMode(d->window, &mode);
        SDL_SetWindowFullscreen(d->window, SDL_WINDOW_FULLSCREEN);
    }
    else if (!d->toFullscreen && d->fullscreen)
    {
        SDL_DisplayMode mode;
        SDL_GetWindowDisplayMode(d->window, &mode);
        mode.w = 1920/2;
        mode.h = 1080/2;
        SDL_SetWindowDisplayMode(d->window, &mode);
        SDL_SetWindowFullscreen(d->window, 0);
    }
    d->fullscreen = d->toFullscreen;
}

void Renderer::renderMeshes()
{
    std::stable_sort(meshes.begin(), meshes.end(), [](Mesh *, Mesh * right) { return (right->renderFlags & Render::DepthWrite) == 0; });
    for (Mesh * mesh : meshes)
    {
        if (!mesh->program->lastError.empty())
        {
            printf((mesh->program->lastError + "\n").c_str(), 0);
            exit(1);
        }

        if (mesh->renderFlags & Render::DepthTest)
            glEnable(GL_DEPTH_TEST);

        if (mesh->renderFlags & Render::Blend)
            glEnable(GL_BLEND);

        if ((mesh->renderFlags & Render::DepthWrite) == 0)
            glDepthMask(GL_FALSE);

        if (mesh->renderFlags & Render::BackFaceCulling)
            glEnable(GL_CULL_FACE);

        d->bindGraphics(mesh->program);
        mesh->program->applyUniforms();
        mesh->applyUniforms();
        d->bindGraphics(mesh);
        mesh->program->draw(mesh);
        d->unbindGraphics(mesh);
        mesh->unapplyUniforms();
        mesh->program->unapplyUniforms();
        d->unbindGraphics(mesh->program);

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}

void Renderer::renderWidgets()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(d->window);
    ImGui::NewFrame();

    ImGui::Begin("Main window");
    if (ImGui::CollapsingHeader("Menu", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Exit"))
        {
            SDL_Event sdlevent;
            sdlevent.type = SDL_QUIT;
            SDL_PushEvent(&sdlevent);
        }
    }
    if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("fullscreen", &d->toFullscreen);
    }
    for (auto * widget : widgets)
    {
        if (widget->window == "Main window")
            widget->draw();
    }
    if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("frametime : %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Text("meshes : %zu instance(s), %zu definition(s), %zu file(s)", meshes.size(), d->meshRegistrations.size(), d->meshCache.size());
        if (ImGui::Button("Clear cache"))
            d->meshCache.clear();
        if (ImGui::CollapsingHeader("instances"))
        {
            for (const auto * mesh : meshes)
            {
                ImGui::Text("%s", mesh->instanceId.str.c_str());
            }
        }
        if (ImGui::CollapsingHeader("definitions"))
        {
            for (const auto & registration : d->meshRegistrations)
            {
                ImGui::Text("%s", registration.definitionId.str.c_str());
            }
        }
        if (ImGui::CollapsingHeader("textures"))
        {
            for (const auto & registration : d->textureRegistrations)
            {
                ImGui::Text("%s", registration.name.str.c_str());
            }
        }
        if (ImGui::CollapsingHeader("files"))
        {
            for (const auto & fileCache : d->meshCache)
            {
                ImGui::Text("%s", fileCache.filename.str.c_str());
            }
        }
    }
    ImGui::End();

    for (auto * widget : widgets)
    {
        if (widget->window != "Main window")
            widget->draw();
    }

    // Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool Renderer::load(const std::string &filename, size_t &count)
{
    const MeshFile & fileCache = d->load(filename);
    count = fileCache.instances.size();
    return count > 0;
}

void Renderer::fill(const std::string &filename, std::vector<Mesh *> &m)
{
    const MeshFile & fileCache = d->load(filename);
    for (size_t i = 0; i < fileCache.instances.size(); ++i)
    {
        const MeshInstance & instance = fileCache.instances[i];
        Mesh * mesh = m[i];
        mesh->name = instance.instanceId;
        mesh->instanceId = instance.instanceId;
        mesh->definitionId = instance.definitionId;
        mesh->filename = fileCache.filename.str;
        mesh->scale *= instance.scale;
        mesh->position += instance.position;
        mesh->orientation = linalg::qmul(instance.orientation, mesh->orientation);
        auto definition = std::find_if(fileCache.definitions.begin(), fileCache.definitions.end(), [mesh](const auto & def){ return def.definitionId == mesh->definitionId; });
        if (definition->doubleSided) mesh->renderFlags &= ~Render::BackFaceCulling;
        add(mesh);
    }
}
