#include "raylib.h"

int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Water Tile Shader - Raylib");

    Camera camera = { 0 };
    camera.position = (Vector3){ 0.0f, 10.0f, 10.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Generate a flat 1x1 water tile at origin
    Mesh mesh = GenMeshPlane(100.0f, 100.0f, 2, 2);
    Model model = LoadModelFromMesh(mesh);

    Shader shader = LoadShader("water.vs", "water.fs");
    model.materials[0].shader = shader;

    int timeLoc = GetShaderLocation(shader, "uTime");

    DisableCursor();

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        float time = GetTime();
        SetShaderValue(shader, timeLoc, &time, SHADER_UNIFORM_FLOAT);

        UpdateCamera(&camera, CAMERA_FIRST_PERSON);

        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode3D(camera);
            DrawModel(model, (Vector3){0}, 1.0f, WHITE);
            //DrawGrid(10, 1.0f);
        EndMode3D();

        DrawText("Water Tile Shader Example", 10, 10, 20, DARKBLUE);
        EndDrawing();
    }

    UnloadModel(model);
    UnloadShader(shader);
    CloseWindow();
    return 0;
}
