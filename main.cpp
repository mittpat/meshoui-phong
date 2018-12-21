#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "assets.h"
#include "manipulators.h"

#include <Camera.h>
#include <Mesh.h>
#include <Program.h>
#include <Renderer.h>
#include <MeshLoader.h>

using namespace linalg;
using namespace linalg::aliases;
#define MESHOUI_COLLADA_LINALG
#include <collada.h>

#include <q3.h>

#include <fstream>

using namespace Meshoui;

namespace
{
    const float timestep = 1.f/60;

    void phongProgramSPIRV(Program * phongProgram, const std::string & vert, const std::string & frag)
    {
        {
            std::ifstream fileStream(vert, std::ifstream::binary);
            phongProgram->vertexShaderSource = std::vector<char>(std::istreambuf_iterator<char>(fileStream), std::istreambuf_iterator<char>());
        }
        {
            std::ifstream fileStream(frag, std::ifstream::binary);
            phongProgram->fragmentShaderSource = std::vector<char>(std::istreambuf_iterator<char>(fileStream), std::istreambuf_iterator<char>());
        }
    }

    void collada(const std::string & filename, std::vector<SimpleMesh> & simpleMeshes)
    {
        DAE::Data data;
        if (DAE::parse(filename, data, DAE::Graphics))
        {
            for (const auto & libraryNode : data.nodes)
            {
                auto geometry = std::find_if(data.geometries.begin(), data.geometries.end(), [libraryNode](const DAE::Geometry & geometry){ return geometry.id == libraryNode.geometry.url; });
                auto material = std::find_if(data.materials.begin(), data.materials.end(), [libraryNode](const DAE::Material & material){ return material.id == libraryNode.geometry.material; });

                if (geometry != data.geometries.end())
                {
                    SimpleMesh simpleMesh;
                    simpleMesh.geometry = MeshLoader::makeGeometry(*geometry, data);
                    if (material != data.materials.end())
                    {
                        simpleMesh.material = MeshLoader::makeMaterial(*material, data);
                    }
                    simpleMesh.modelMatrix = libraryNode.transform;
                    simpleMeshes.push_back(simpleMesh);
                }
            }
        }
    }
}

int main(int, char**)
{
    q3Scene scene(timestep);
    Renderer renderer;

    Program phongProgram;
    phongProgramSPIRV(&phongProgram, "meshoui/resources/shaders/Phong.vert.spv",
                                     "meshoui/resources/shaders/Phong.frag.spv");
    renderer.add(&phongProgram);

    std::vector<SimpleMesh> simpleMeshes;
    collada("meshoui/resources/models/bricks.dae", simpleMeshes);
    for (auto & mesh : simpleMeshes) renderer.add(&mesh);

    {
        ScopedSkydome skydome(&renderer);
        //ScopedAsset bricks(&renderer, "meshoui/resources/models/bricks.dae");
        //bricks.meshes[0]->modelMatrix.w = float4(float3(0.0, 5, 0.0), 1.0);
        ScopedAsset island(&renderer, "meshoui/resources/models/island.dae");
        for (const auto & mesh : island.meshes) mesh->modelMatrix.w.y = -4.0f;
        ScopedAsset crates(&renderer, "meshoui/resources/models/crates.dae");
        std::vector<ScopedBody> bodies; bodies.reserve(crates.meshes.size());
        for (const auto & mesh : crates.meshes) { bodies.emplace_back(&scene, mesh->modelMatrix, &mesh->modelMatrix); }
        float4x4 groundModel = mul(pose_matrix(float4(0,0,0,1), float3(0,-5,0)), scaling_matrix(float3(100,1,100)));
        ScopedBody groundBody(&scene, groundModel, nullptr, false);

        Camera camera;
        camera.modelMatrix.w = float4(float3(-5.0, 0.0, 5.0), 1.0);
        renderer.add(&camera);
        camera.enable();

        float4x4 playerBodyModel = identity;
        ScopedBody playerBody(&scene, camera.modelMatrix, &playerBodyModel, true, true, 0.25f);

        float4x4 playerHeadAltitude = identity;
        float4x4 playerHeadAzimuth = identity;
        BodyAcceleration cameraAnimator(playerBody.body, 0.1f);
        WASD<BodyAcceleration> cameraStrafer(&cameraAnimator);
        renderer.add(&cameraStrafer);
        Mouselook cameraLook(&playerHeadAltitude, &playerHeadAzimuth);
        renderer.add(&cameraLook);

        Light light;
        light.modelMatrix.w = float4(float3(600.0, 1000.0, -300.0), 1.0);
        renderer.add(&light);
        light.enable(true);

        bool run = true;
        while (run)
        {
            cameraAnimator.step(timestep);
            scene.Step();
            for (auto & body : bodies) { body.step(timestep); }
            playerBody.step(timestep);
            camera.modelMatrix = mul(playerBodyModel, mul(translation_matrix(float3(0,1.8,0)), playerHeadAltitude));
            playerBody.setAzimuth(playerHeadAzimuth);
            playerBody.jump(cameraStrafer.space);
            renderer.update(timestep);
            if (renderer.shouldClose()) { run = false; }
        }

        renderer.remove(&cameraStrafer);
        renderer.remove(&light);
        renderer.remove(&camera);
    }

    renderer.remove(&phongProgram);

    return 0;
}
