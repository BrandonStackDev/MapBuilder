#include "raylib.h"

#define SQUARE_SIZE 16

int main() {
    InitWindow(800, 600, "Water Shader Tile");
    SetTargetFPS(60);

    // Load shader
    Shader shader = LoadShader("water.vs", "water.fs");
    int timeLoc = GetShaderLocation(shader, "uTime");
    int offsetLoc = GetShaderLocation(shader, "worldOffset");

    // Generate quad mesh
    Mesh mesh = GenMeshPlane(1.0f, 1.0f, SQUARE_SIZE, SQUARE_SIZE);
    //Mesh mesh = GenMeshTorus(3.0f, 7.0f, 2, 32);
    Model water = LoadModelFromMesh(mesh);
    water.materials[0].shader = shader;

    Camera camera = { 0 };
    camera.position = (Vector3){ 12.0f, 3.5f, 12.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose()) {
        float time = GetTime();
        SetShaderValue(shader, timeLoc, &time, SHADER_UNIFORM_FLOAT);

        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
            ClearBackground(DARKBLUE);
            BeginMode3D(camera);
            BeginBlendMode(BLEND_ALPHA);
                //(water, (Vector3){ 0, 0, 0 }, 1.0f, WHITE);
                for (int z = -10; z <= 10; z++) {
                    for (int x = -10; x <= 10; x++) {
                        Vector3 pos = (Vector3){ x, 0.0f, z };
                        Vector2 offset = (Vector2){ x, z };
                        SetShaderValue(shader, offsetLoc, &offset, SHADER_UNIFORM_VEC2);
                        DrawModel(water, pos, 1.0f, WHITE);
                    }
                }
            EndBlendMode();
            EndMode3D();
            DrawFPS(10, 10);
        EndDrawing();
    }

    UnloadModel(water);
    UnloadShader(shader);
    CloseWindow();
    return 0;
}
