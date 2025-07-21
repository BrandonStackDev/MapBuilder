#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define MAX_INSTANCES 1000

int main(void)
{
    InitWindow(1280, 720, "Raylib Instanced Trees");
    SetTargetFPS(60);

    // Load the GLB model
    Model treeModel = LoadModel("models/tree.glb");
    if (treeModel.meshCount == 0) {
        TraceLog(LOG_ERROR, "Failed to load model or mesh missing.");
        CloseWindow();
        return 1;
    }

    // Extract the mesh and material from the model
    Mesh mesh = treeModel.meshes[0];
    Material mat = treeModel.materials[0];

    // Generate instance transforms
    Matrix transforms[MAX_INSTANCES];
    srand(time(NULL));

    for (int i = 0; i < MAX_INSTANCES; i++) {
        float x = (float)(rand() % 100 - 50);
        float z = (float)(rand() % 100 - 50);
        float y = 0.0f;
        float scale = 0.8f + (rand() % 40) / 100.0f; // 0.8 to 1.2

        Matrix translation = MatrixTranslate(x, y, z);
        Matrix scaling = MatrixScale(scale, scale, scale);
        transforms[i] = MatrixMultiply(scaling, translation);
    }

    Camera3D camera = {
        .position = { 0.0f, 10.0f, 20.0f },
        .target = { 0.0f, 0.0f, 0.0f },
        .up = { 0.0f, 1.0f, 0.0f },
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE
    };

    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
        ClearBackground(SKYBLUE);

        BeginMode3D(camera);

        // Draw all instances
        DrawMeshInstanced(mesh, mat, transforms, MAX_INSTANCES);

        EndMode3D();

        DrawFPS(10, 10);
        EndDrawing();
    }

    UnloadModel(treeModel);
    CloseWindow();
    return 0;
}
