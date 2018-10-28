#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "RendererPrivate.h"
#include "Camera.h"
#include "Program.h"
#include "Uniform.h"
#include "Widget.h"

#include "loose.h"

#include <algorithm>
#include <set>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace
{
    void error_callback(int, const char* description)
    {
        printf("Error: %s\n", description);
    }
}

using namespace linalg;
using namespace linalg::aliases;

Renderer::~Renderer()
{
    // imgui cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // glfw & gl cleanup
    glfwDestroyWindow(d->window);
    glfwTerminate();
    delete d;
}

Renderer::Renderer()
    : d(new RendererPrivate)
    , defaultProgram(nullptr)
    , time(0.f)
    , meshes()
    , programs()
{
    // glfw & gl
    glfwSetErrorCallback(error_callback);
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    d->window = glfwCreateWindow(1920/2, 1080/2, "Meshoui", nullptr, nullptr);

    // glad
    glfwMakeContextCurrent(d->window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSwapInterval(1);

    // imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(d->window, true);
    ImGui_ImplOpenGL3_Init("#version 150");
    ImGui::StyleColorsDark();
}

bool Renderer::shouldClose() const
{
    return glfwWindowShouldClose(d->window);
}

void Renderer::add(Model *model)
{
    if (std::find(models.begin(), models.end(), model) == models.end())
    {
        models.push_back(model);
        d->registerGraphics(model);
    }
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

void Renderer::remove(Model *model)
{
    d->unregisterGraphics(model);
    models.erase(std::remove(models.begin(), models.end(), model));
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

void Renderer::update(float s)
{
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glCullFace(GL_BACK);

    int w, h;
    glfwGetFramebufferSize(d->window, &w, &h);
    glViewport(0, 0, w, h);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    renderMeshes();
    renderWidgets();

    glfwSwapBuffers(d->window);

    postUpdate();

    glfwPollEvents();

    time += s;
}

void Renderer::postUpdate()
{
    if (d->toFullscreen && !d->fullscreen)
    {
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowMonitor(d->window, glfwGetPrimaryMonitor(), 0, 0, 1920, 1080, GLFW_DONT_CARE);
    }
    else if (!d->toFullscreen && d->fullscreen)
    {
        glfwSetWindowMonitor(d->window, nullptr, 80, 80, 1920/2, 1080/2, GLFW_DONT_CARE);
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    }
    d->fullscreen = d->toFullscreen;
}

void Renderer::renderMeshes()
{
    std::stable_sort(meshes.begin(), meshes.end(), [](Mesh *, Mesh * right) { return (right->renderFlags & Render::DepthWrite) == 0; });
    for (Mesh * mesh : meshes)
    {
        if ((mesh->renderFlags & Render::Visible) == 0)
            continue;

        if (!mesh->program->lastError.empty())
        {
            printf("%s\n", mesh->program->lastError.c_str());
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

        if (mesh->renderFlags & Render::Points)
            glPointSize(mesh->scale.x);

        if (!d->lights.empty())
        {
            if (auto uniform = dynamic_cast<Uniform3fv *>(mesh->program->uniform("sunPosition")))
                uniform->value = normalize(d->lights[0]->position);
            if (auto uniform = dynamic_cast<Uniform3fv *>(mesh->program->uniform("uniformLightPosition")))
                uniform->value = d->lights[0]->position;
        }

        if (d->camera != nullptr)
        {
            if (auto uniform = dynamic_cast<Uniform44fm*>(mesh->program->uniform("uniformView")))
                uniform->value = d->camera->viewMatrix(mesh->viewFlags);
            if (auto uniform = dynamic_cast<Uniform3fv*>(mesh->program->uniform("uniformViewPosition")))
                uniform->value = d->camera->position;
        }

        if (auto uniform = dynamic_cast<Uniform44fm*>(mesh->program->uniform("uniformModel")))
            uniform->value = mesh->modelMatrix();
        if (auto uniform = dynamic_cast<Uniform44fm*>(mesh->program->uniform("uniformProjection")))
            uniform->value = mesh->viewFlags == View::None ? identity : d->projectionMatrix;

        if (auto uniform = dynamic_cast<Uniform2fv*>(mesh->program->uniform("uniformTime")))
            uniform->value = float2(time, 0.016f);

        d->bindGraphics(mesh->program);
        mesh->program->applyUniforms();
        mesh->applyUniforms();
        d->bindGraphics(mesh);
        mesh->program->draw(mesh);
        d->unbindGraphics(mesh);
        mesh->unapplyUniforms();
        mesh->program->unapplyUniforms();
        d->unbindGraphics(mesh->program);

        glPointSize(1.0f);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}

void Renderer::renderWidgets()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Main window");
    if (ImGui::CollapsingHeader("Menu", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Exit"))
        {
            glfwSetWindowShouldClose(d->window, 1);
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
        ImGui::Text("meshes : %zu instance(s), %zu definition(s), %zu file(s)", meshes.size(), d->meshRegistrations.size(), d->meshFiles.size());
        if (ImGui::Button("Clear cache"))
            d->meshFiles.clear();
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
            for (const auto & meshFile : d->meshFiles)
            {
                ImGui::Text("%s", meshFile.filename.c_str());
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
