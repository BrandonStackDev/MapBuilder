#include "raylib.h"
#include "raymath.h"
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <model.obj/glb/etc>\n", argv[0]);
        return 1;
    }

    const char *modelPath = argv[1];

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(800, 600, "Raylib Model Viewer");
    SetTargetFPS(60);

    Camera camera = { 0 };
    camera.position = (Vector3){ 0.0f, 10.0f, 10.0f };  // Start looking from above
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    char bgTreeTexturePath[64];
    snprintf(bgTreeTexturePath, sizeof(bgTreeTexturePath), "textures/tree_skin2.png");
    Texture tex = LoadTexture(bgTreeTexturePath);
    Model model = LoadModel(modelPath);
    if (model.meshCount == 0 || model.meshes[0].vertexCount == 0) {
        TraceLog(LOG_ERROR, "Failed to load model or model is empty.");
        CloseWindow();
        return 1;
    }
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = tex;
    BoundingBox bounds = GetModelBoundingBox(model);
    Vector3 center = Vector3Lerp(bounds.min, bounds.max, 0.5f);
    Vector3 size = Vector3Subtract(bounds.max, bounds.min);
    Vector3 drawOffset = Vector3Negate(center);
    camera.position = Vector3Subtract(center, (Vector3){10,0,10});

    Vector3 modelPos = (Vector3){ 0 };
    float modelScale = 1.0f; // / fmaxf(fmaxf(size.x, size.y), size.z);
    bool mouseEn = true;
    DisableCursor();

    while (!WindowShouldClose()) {
        // if(mouseEn){EnableCursor();}
        // else {DisableCursor();}
        // if (IsKeyPressed(KEY_M)) mouseEn = !mouseEn; 
        //UpdateCamera(&camera, CAMERA_ORBITAL);
        UpdateCamera(&camera, CAMERA_FREE);  // <--- Free look camera

        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode3D(camera);

        //DrawModelEx(model, drawOffset, (Vector3){ 0, 1, 0 }, 0, (Vector3){ modelScale, modelScale, modelScale }, WHITE);
        DrawModelEx(model, modelPos, (Vector3){ 0, 1, 0 }, 0, (Vector3){ modelScale, modelScale, modelScale }, WHITE);
        DrawBoundingBox(bounds, RED);
        DrawSphere((Vector3){0, 0, 0}, 0.1f, RED); // world origin
        DrawGrid(20, 1.0f);

        EndMode3D();

        DrawText(TextFormat("Triangles: %d", model.meshes[0].triangleCount), 10, 10, 20, DARKGRAY);
        EndDrawing();
    }

    UnloadModel(model);
    CloseWindow();
    return 0;
}
