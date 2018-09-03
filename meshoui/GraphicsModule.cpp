#include <GL/glew.h>

#include "GraphicsModule.h"
#include "GraphicsPrivate.h"
#include "Camera.h"
#include "GraphicsProgram.h"
#include "GraphicsUniform.h"

#include "loose.h"

#include <algorithm>
#include <set>

#include <SDL2/SDL_image.h>

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_opengl3.h>

using namespace linalg;
using namespace linalg::aliases;

GraphicsModule::~GraphicsModule()
{
    aboutToBeRemovedFromApplication();

    delete defaultProgram;

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

GraphicsModule::GraphicsModule(bool gles)
    : projectionMatrix(linalg::perspective_matrix(degreesToRadians(90.f), 1280/800.f, 0.1f, 1000.f))
    , sun(0.6f, 0.8f)
    , defaultProgram(new GraphicsProgram())
    , d(new GraphicsPrivate)
    , meshes()
    , programs()
{
    defaultProgram->load("meshoui/resources/shaders/Phong.shader");

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

    addedToApplication();
}

void GraphicsModule::addedToApplication()
{
    add(defaultProgram);
}

void GraphicsModule::aboutToBeRemovedFromApplication()
{
    remove(defaultProgram);
}

void GraphicsModule::add(IWhatever * whatever)
{
    if (auto camera = dynamic_cast<Camera *>(whatever))
    {
        if (std::find(cameras.begin(), cameras.end(), camera) == cameras.end())
        {
            cameras.push_back(camera);
        }
    }
    if (auto mesh = dynamic_cast<Mesh *>(whatever))
    {
        if (std::find(meshes.begin(), meshes.end(), mesh) == meshes.end())
        {
            meshes.push_back(mesh);
            if (!mesh->program)
                mesh->program = defaultProgram;
            d->registerGraphics(mesh);
        }
    }
    if (auto program = dynamic_cast<GraphicsProgram *>(whatever))
    {
        if (std::find(programs.begin(), programs.end(), program) == programs.end())
        {
            programs.push_back(program);
            d->registerGraphics(program);
        }
    }
}

void GraphicsModule::remove(IWhatever* whatever)
{
    if (auto camera = dynamic_cast<Camera *>(whatever))
    {
        cameras.erase(std::remove(cameras.begin(), cameras.end(), camera));
    }
    if (auto mesh = dynamic_cast<Mesh *>(whatever))
    {
        d->unregisterGraphics(mesh);
        if (mesh->program == defaultProgram)
            mesh->program = nullptr;
        meshes.erase(std::remove(meshes.begin(), meshes.end(), mesh));
    }
    if (auto program = dynamic_cast<GraphicsProgram *>(whatever))
    {
        d->unregisterGraphics(program);
        programs.erase(std::remove(programs.begin(), programs.end(), program));
    }
}

void GraphicsModule::update(double)
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

    static const float SunDistance = 1000.f;
    if (auto lightPositionUniform = dynamic_cast<GraphicsUniform3fv *>(defaultProgram->uniform("uniformLightPosition")))
    {
        static const float3 up(0.,1.,0.);
        static const float3 right(-1.,0.,0.);
        lightPositionUniform->value = mul(rotation_matrix(qmul(rotation_quat(up, sun.y), rotation_quat(right, sun.x))), float4(0., 0., 1., 1.0)).xyz() * SunDistance;
    }

    renderMeshes();
    renderWidgets();

    SDL_GL_SwapWindow(d->window);

    postUpdate();
}

void GraphicsModule::postUpdate()
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

void GraphicsModule::renderMeshes()
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

        if (auto uniform = dynamic_cast<GraphicsUniform44fm*>(mesh->program->uniform("uniformModel")))
            uniform->value = mesh->modelMatrix();
        if (auto uniform = dynamic_cast<GraphicsUniform44fm*>(mesh->program->uniform("uniformProjection")))
            uniform->value = mesh->viewFlags == View::None ? identity : projectionMatrix;
        if (!cameras.empty())
        {
            if (auto uniform = dynamic_cast<GraphicsUniform44fm*>(mesh->program->uniform("uniformView")))
                uniform->value = cameras[0]->viewMatrix(mesh->viewFlags);
            if (auto uniform = dynamic_cast<GraphicsUniform3fv*>(mesh->program->uniform("uniformViewPosition")))
            {
                float4 v = inverse(cameras[0]->viewMatrix(View::Rotation | View::Translation))[3];
                uniform->value = float3(v.x,v.y,v.z);
            }
        }

        mesh->program->bind();
        mesh->program->applyUniforms();
        mesh->applyUniforms();
        mesh->program->draw(mesh);
        mesh->unapplyUniforms();
        mesh->program->unapplyUniforms();
        mesh->program->unbind();

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}

void GraphicsModule::renderWidgets()
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
        ImGui::SliderFloat2("Sun", &sun[0], -M_PI, M_PI);
    }
    if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("fullscreen", &d->toFullscreen);
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

    // Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

std::vector<Mesh *> GraphicsModule::meshFactory(const std::string &filename)
{
    std::vector<Mesh *> ret;
    const MeshFile & fileCache = d->load(filename);
    for (const auto & instance : fileCache.instances)
    {
        Mesh * mesh = new Mesh();
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
        ret.push_back(mesh);
    }
    return ret;
}
