// lod_study.c - A simplified LOD chunk visualizer
// Based on preview.c but stripped down to test chunk seams at different LODs

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> // for usleep
#include <string.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define CHUNK_COUNT 16
#define CHUNK_SIZE 64
#define HEIGHT_SCALE 20.0f
#define MAP_SIZE (CHUNK_COUNT * CHUNK_SIZE)
#define MAX_CHUNKS_TO_QUEUE (CHUNK_COUNT * CHUNK_COUNT)
#define MAP_SCALE 16
#define MAP_VERTICAL_OFFSET 0 //(MAP_SCALE * -64)
#define PLAYER_HEIGHT 1.7f
#define USE_TREE_CUBES false

typedef enum {
    LOD_64,
    LOD_32,
    LOD_16,
    LOD_8
} TypeLOD;

typedef struct {
    bool isLoaded;
    bool isReady;
    BoundingBox box;
    BoundingBox origBox;
    Model model;
    Model model32;
    Model model16;
    Model model8;
    TypeLOD lod;
    Texture2D texture;
    Texture2D textureBig;
    Texture2D textureFull;
    Vector3 position;
    Vector3 center;
    float heightData[(CHUNK_SIZE + 1) * (CHUNK_SIZE + 1)];
    Vector3 *treePositions;
    int treeCount;
} Chunk;

Chunk chunks[CHUNK_COUNT][CHUNK_COUNT];
Vector3 cameraVelocity = { 0 };

Camera3D camera = { 0 };
int activeCX = CHUNK_COUNT / 2;
int activeCY = CHUNK_COUNT / 2;
bool showBoxes = true;

void ImageDataFlipVertical(Image *image) {
    int width = image->width;
    int height = image->height;
    int bytesPerPixel = 4;  // Assuming RGBA8
    int stride = width * bytesPerPixel;

    unsigned char *pixels = (unsigned char *)image->data;
    unsigned char *tempRow = (unsigned char *)malloc(stride);

    for (int y = 0; y < height / 2; y++) {
        unsigned char *row1 = pixels + y * stride;
        unsigned char *row2 = pixels + (height - 1 - y) * stride;

        memcpy(tempRow, row1, stride);
        memcpy(row1, row2, stride);
        memcpy(row2, tempRow, stride);
    }

    free(tempRow);
}

void ImageDataFlipHorizontal(Image *image) {
    int width = image->width;
    int height = image->height;
    int bytesPerPixel = 4;  // Assuming RGBA8
    unsigned char *pixels = (unsigned char *)image->data;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width / 2; x++) {
            int i1 = (y * width + x) * bytesPerPixel;
            int i2 = (y * width + (width - 1 - x)) * bytesPerPixel;

            for (int b = 0; b < bytesPerPixel; b++) {
                unsigned char tmp = pixels[i1 + b];
                pixels[i1 + b] = pixels[i2 + b];
                pixels[i2 + b] = tmp;
            }
        }
    }
}

void SetAllLOD(int lod)
{
    for (int cy = 0; cy < CHUNK_COUNT; cy++) {
        for (int cx = 0; cx < CHUNK_COUNT; cx++) {
            chunks[cx][cy].lod = lod;
        }
    }
}

void ApplyReal64GridLOD(int cx, int cy)
{
    for (int j = 0; j < CHUNK_COUNT; j++) {
        for (int i = 0; i < CHUNK_COUNT; i++) {
            int dx = abs(i - cx);
            int dy = abs(j - cy);
            int dist = dx > dy ? dx : dy;
            if (dist == 0) chunks[i][j].lod = LOD_64;
            else if (dist == 1) chunks[i][j].lod = LOD_32;
            else if (dist == 2) chunks[i][j].lod = LOD_16;
            else chunks[i][j].lod = LOD_8;
        }
    }
}


Texture2D LoadSafeTexture(const char *filename) {
    Image img = LoadImage(filename);
    if (img.width != 64 || img.height != 64) {
        TraceLog(LOG_WARNING, "Image %s is not 64x64: (%d x %d)", filename, img.width, img.height);
    }

    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ImageDataFlipVertical(&img);
    //ImageDataFlipHorizontal(&img);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);

    if (tex.id == 0) {
        TraceLog(LOG_WARNING, "Failed to create texture from image: %s", filename);
    }
    return tex;
}

void PreLoadTexture(int cx, int cy)
{
    char colorPath[256];
    char colorBigPath[256];
    char slopePath[256];
    char slopeBigPath[256];
    char avgPath[256];
    char avgBigPath[256];
    char avgFullPath[256];
    snprintf(colorPath, sizeof(colorPath), "map/chunk_%02d_%02d/color.png", cx, cy);
    snprintf(colorBigPath, sizeof(colorBigPath), "map/chunk_%02d_%02d/color_big.png", cx, cy);
    snprintf(slopePath, sizeof(slopePath), "map/chunk_%02d_%02d/slope.png", cx, cy);
    snprintf(slopeBigPath, sizeof(slopeBigPath), "map/chunk_%02d_%02d/slope_big.png", cx, cy);
    snprintf(avgPath, sizeof(avgPath), "map/chunk_%02d_%02d/avg.png", cx, cy);
    snprintf(avgBigPath, sizeof(avgBigPath), "map/chunk_%02d_%02d/avg_big.png", cx, cy);
    snprintf(avgFullPath, sizeof(avgFullPath), "map/chunk_%02d_%02d/avg_full.png", cx, cy);
    // --- Load texture and assign to model material ---
    TraceLog(LOG_INFO, "Loading texture: %s", colorPath);
    Texture2D texture = LoadSafeTexture(avgPath); //using slope and color avg right now
    Texture2D textureBig = LoadSafeTexture(avgBigPath);
    Texture2D textureFull = LoadSafeTexture(avgFullPath);
    chunks[cx][cy].texture = texture;
    chunks[cx][cy].textureBig = textureBig;
    chunks[cx][cy].textureFull = textureFull;
}

void LoadChunk(int cx, int cy)
{
    // --- Assemble filenames based on chunk coordinates ---
    char objPath[256];
    char objPath32[256];
    char objPath16[256];
    char objPath8[256];
    snprintf(objPath, sizeof(objPath), "map/chunk_%02d_%02d/64.obj", cx, cy);
    snprintf(objPath32, sizeof(objPath32), "map/chunk_%02d_%02d/32.obj", cx, cy);
    snprintf(objPath16, sizeof(objPath16), "map/chunk_%02d_%02d/16.obj", cx, cy);
    snprintf(objPath8, sizeof(objPath8), "map/chunk_%02d_%02d/8.obj", cx, cy);
    
    // --- Load 3D model from .obj file ---
    TraceLog(LOG_INFO, "Loading OBJ: %s", objPath);
    Model model = LoadModel(objPath);
    Model model32 = LoadModel(objPath32);
    Model model16 = LoadModel(objPath16);
    Model model8 = LoadModel(objPath8);

    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].textureFull;
    model32.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].textureFull;
    model16.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].texture;
    model8.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].texture;

    // --- Position the model in world space ---
    float worldHalfSize = (CHUNK_COUNT * CHUNK_SIZE) / 2.0f;
    Vector3 position = (Vector3){
        (cx * CHUNK_SIZE - worldHalfSize) * MAP_SCALE,
        MAP_VERTICAL_OFFSET,
        (cy * CHUNK_SIZE - worldHalfSize) * MAP_SCALE
    };
    Vector3 center = {
        position.x + (CHUNK_SIZE * 0.5f * MAP_SCALE),
        position.y,
        position.z + (CHUNK_SIZE * 0.5f * MAP_SCALE)
    };

    // --- Store the chunk data ---
    // Save CPU-side mesh before uploading
    //chunks[cx][cy].mesh = model.meshes[0];  // <- capture BEFORE LoadModelFromMesh
    chunks[cx][cy].model = model;
    chunks[cx][cy].model32 = model32;
    chunks[cx][cy].model16 = model16;
    chunks[cx][cy].model8 = model8;
    chunks[cx][cy].position = position;
    chunks[cx][cy].center = center;
    chunks[cx][cy].isReady = true;
    chunks[cx][cy].lod = LOD_8;

    TraceLog(LOG_INFO, "Chunk [%02d, %02d] loaded at position (%.1f, %.1f, %.1f)", 
             cx, cy, position.x, position.y, position.z);
}


void *ChunkLoaderThread(void *arg) {
    for (int cy = 0; cy < CHUNK_COUNT; cy++) {
        for (int cx = 0; cx < CHUNK_COUNT; cx++) {
            if (!chunks[cx][cy].isLoaded) {
                LoadChunk(cx, cy);
            }
        }
    }
    return NULL;
}

void StartChunkLoader() {
    pthread_t thread;
    pthread_create(&thread, NULL, ChunkLoaderThread, NULL);
    pthread_detach(thread);
}

int main(void)
{
    float pitch = 0.0f;
    float yaw = 0.0f;  // Face toward -Z (the terrain is likely laid out in +X, +Z space)

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Chunk LOD Seam Study");
    SetTargetFPS(60);
    DisableCursor();

    camera.position = (Vector3){ 0.0f, 2000.0f, 0.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    for (int cy = 0; cy < CHUNK_COUNT; cy++) {
        for (int cx = 0; cx < CHUNK_COUNT; cx++) {
            if (!chunks[cx][cy].isLoaded && !chunks[cx][cy].isReady) {
                PreLoadTexture(cx, cy);
                //LoadChunk(cx, cy);
            }
        }
    }
    StartChunkLoader();

    SetAllLOD(LOD_64);  // Start in 64 mode w/grid
    ApplyReal64GridLOD(activeCX, activeCY);

    while (!WindowShouldClose())
    {
        //check chunks for anyone that needs mesh upload again
        for (int cy = 0; cy < CHUNK_COUNT; cy++) {
            for (int cx = 0; cx < CHUNK_COUNT; cx++) {
                if (chunks[cx][cy].isReady && !chunks[cx][cy].isLoaded) {
                    TraceLog(LOG_INFO, "loading chunk model: %d,%d", cx, cy);

                    // Upload meshes to GPU
                    UploadMesh(&chunks[cx][cy].model.meshes[0], false);
                    UploadMesh(&chunks[cx][cy].model32.meshes[0], false);
                    UploadMesh(&chunks[cx][cy].model16.meshes[0], false);
                    UploadMesh(&chunks[cx][cy].model8.meshes[0], false);

                    // Load GPU models
                    chunks[cx][cy].model = LoadModelFromMesh(chunks[cx][cy].model.meshes[0]);
                    chunks[cx][cy].model32 = LoadModelFromMesh(chunks[cx][cy].model32.meshes[0]);
                    chunks[cx][cy].model16 = LoadModelFromMesh(chunks[cx][cy].model16.meshes[0]);
                    chunks[cx][cy].model8 = LoadModelFromMesh(chunks[cx][cy].model8.meshes[0]);

                    // Apply textures
                    chunks[cx][cy].model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].textureFull;
                    chunks[cx][cy].model32.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].textureBig;
                    chunks[cx][cy].model16.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].texture;
                    chunks[cx][cy].model8.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].texture;

                    // Setup bounding box - skipped in this program because we dont need it

                    chunks[cx][cy].isLoaded = true;
                    TraceLog(LOG_INFO, "loaded chunk model -> %d,%d", cx, cy);
                }
            }
        }
        // Mouse look
        Vector2 mouse = GetMouseDelta();
        yaw -= mouse.x * 0.003f;
        pitch -= mouse.y * 0.003f;
        pitch = Clamp(pitch, -PI/2.0f, PI/2.0f);

        Vector3 forward = {
            cosf(pitch) * cosf(yaw) * MAP_SCALE,
            sinf(pitch) * MAP_SCALE,
            cosf(pitch) * sinf(yaw) * MAP_SCALE
        };

        // Input for LOD control
        if (IsKeyPressed(KEY_ONE))  SetAllLOD(LOD_8);
        if (IsKeyPressed(KEY_TWO))  SetAllLOD(LOD_16);
        if (IsKeyPressed(KEY_THREE)) SetAllLOD(LOD_32);
        if (IsKeyPressed(KEY_FOUR))  ApplyReal64GridLOD(activeCX, activeCY);

        // Navigation of active chunk when using 64-grid mode
        if (IsKeyPressed(KEY_LEFT))  { activeCX = (activeCX - 1 + CHUNK_COUNT) % CHUNK_COUNT; ApplyReal64GridLOD(activeCX, activeCY); }
        if (IsKeyPressed(KEY_RIGHT)) { activeCX = (activeCX + 1) % CHUNK_COUNT; ApplyReal64GridLOD(activeCX, activeCY); }
        if (IsKeyPressed(KEY_UP))    { activeCY = (activeCY - 1 + CHUNK_COUNT) % CHUNK_COUNT; ApplyReal64GridLOD(activeCX, activeCY); }
        if (IsKeyPressed(KEY_DOWN))  { activeCY = (activeCY + 1) % CHUNK_COUNT; ApplyReal64GridLOD(activeCX, activeCY); }
        camera.target = Vector3Add(camera.position, forward);
        UpdateCamera(&camera, CAMERA_THIRD_PERSON);
        BeginDrawing();
        ClearBackground(SKYBLUE);

        BeginMode3D(camera);
        for (int cy = 0; cy < CHUNK_COUNT; cy++) {
            for (int cx = 0; cx < CHUNK_COUNT; cx++) {
                if (chunks[cx][cy].isLoaded) {
                    Vector3 pos = chunks[cx][cy].position;
                    if (chunks[cx][cy].lod == LOD_64)
                        DrawModel(chunks[cx][cy].model, pos, MAP_SCALE, RED);
                    else if (chunks[cx][cy].lod == LOD_32)
                        DrawModel(chunks[cx][cy].model32, pos, MAP_SCALE, BLUE);
                    else if (chunks[cx][cy].lod == LOD_16)
                        DrawModel(chunks[cx][cy].model16, pos, MAP_SCALE, PINK);
                    else
                        DrawModel(chunks[cx][cy].model8, pos, MAP_SCALE, WHITE);
                }
            }
        }
        EndMode3D();

        DrawText("1: LOD 8", 10, 10, 20, DARKGRAY);
        DrawText("2: LOD 16", 10, 30, 20, DARKGRAY);
        DrawText("3: LOD 32", 10, 50, 20, DARKGRAY);
        DrawText("4: Real LOD 64 Grid", 10, 70, 20, DARKGRAY);
        DrawText("Arrow Keys: Move active chunk (64 mode only)", 10, 90, 20, DARKGRAY);
        DrawText(TextFormat("Active chunks: %d,%d",activeCX, activeCY), 10, 110, 20, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
