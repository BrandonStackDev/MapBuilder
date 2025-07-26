#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

//#define	RAND_MAX	2147483647 //from stdlib
/// @brief little utility I like to drop into these to help me out, make cool art, etc...
/// @param  
void TakeScreenshotWithTimestamp(void) {
    // Get timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[128];
    strftime(filename, sizeof(filename), "screenshot_%Y%m%d_%H%M%S.png", t);

    // Save the screenshot
    TakeScreenshot(filename);
    TraceLog(LOG_INFO, "Saved screenshot: %s", filename);
}

//custom exports thanks to chatgpt
void ExportMeshAsGLTF(Mesh mesh, const char *folderPath) {
    // Ensure output folder
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", folderPath);
    system(cmd);

    // --- Write binary data ---
    char binPath[256];
    snprintf(binPath, sizeof(binPath), "%s/new_grass.glb", folderPath);
    FILE *bin = fopen(binPath, "wb");

    size_t vertexByteLength = mesh.vertexCount * 3 * sizeof(float);
    size_t normalByteLength = mesh.vertexCount * 3 * sizeof(float);
    size_t texcoordByteLength = mesh.vertexCount * 2 * sizeof(float);
    size_t indexByteLength = mesh.triangleCount * 3 * sizeof(unsigned short);

    size_t vertexOffset = 0;
    size_t normalOffset = vertexOffset + vertexByteLength;
    size_t texcoordOffset = normalOffset + normalByteLength;
    size_t indexOffset = texcoordOffset + texcoordByteLength;

    fwrite(mesh.vertices, 1, vertexByteLength, bin);
    fwrite(mesh.normals, 1, normalByteLength, bin);
    fwrite(mesh.texcoords, 1, texcoordByteLength, bin);
    fwrite(mesh.indices, 1, indexByteLength, bin);

    fclose(bin);

    // --- Write GLTF JSON ---
    char gltfPath[256];
    snprintf(gltfPath, sizeof(gltfPath), "%s/scene.gltf", folderPath);
    FILE *gltf = fopen(gltfPath, "w");

    fprintf(gltf,
        "{\n"
        "  \"asset\": { \"version\": \"2.0\" },\n"
        "  \"buffers\": [\n"
        "    { \"uri\": \"mesh.bin\", \"byteLength\": %zu }\n"
        "  ],\n"
        "  \"bufferViews\": [\n"
        "    { \"buffer\": 0, \"byteOffset\": %zu, \"byteLength\": %zu, \"target\": 34962 },\n"
        "    { \"buffer\": 0, \"byteOffset\": %zu, \"byteLength\": %zu, \"target\": 34962 },\n"
        "    { \"buffer\": 0, \"byteOffset\": %zu, \"byteLength\": %zu, \"target\": 34962 },\n"
        "    { \"buffer\": 0, \"byteOffset\": %zu, \"byteLength\": %zu, \"target\": 34963 }\n"
        "  ],\n"
        "  \"accessors\": [\n"
        "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": %d, \"type\": \"VEC3\" },\n"
        "    { \"bufferView\": 1, \"componentType\": 5126, \"count\": %d, \"type\": \"VEC3\" },\n"
        "    { \"bufferView\": 2, \"componentType\": 5126, \"count\": %d, \"type\": \"VEC2\" },\n"
        "    { \"bufferView\": 3, \"componentType\": 5123, \"count\": %d, \"type\": \"SCALAR\" }\n"
        "  ],\n"
        "  \"meshes\": [\n"
        "    {\n"
        "      \"primitives\": [\n"
        "        {\n"
        "          \"attributes\": {\n"
        "            \"POSITION\": 0,\n"
        "            \"NORMAL\": 1,\n"
        "            \"TEXCOORD_0\": 2\n"
        "          },\n"
        "          \"indices\": 3\n"
        "        }\n"
        "      ]\n"
        "    }\n"
        "  ],\n"
        "  \"nodes\": [ { \"mesh\": 0 } ],\n"
        "  \"scenes\": [ { \"nodes\": [ 0 ] } ],\n"
        "  \"scene\": 0\n"
        "}\n",
        vertexByteLength + normalByteLength + texcoordByteLength + indexByteLength,
        vertexOffset, vertexByteLength,
        normalOffset, normalByteLength,
        texcoordOffset, texcoordByteLength,
        indexOffset, indexByteLength,
        mesh.vertexCount, mesh.vertexCount, mesh.vertexCount, mesh.triangleCount * 3);

    fclose(gltf);

    printf("Exported GLTF + BIN to: %s\n", folderPath);
}


void ExportMeshAsOBJWithIndices(Mesh mesh, const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f) {
        TraceLog(LOG_ERROR, "Failed to open %s for writing", filename);
        return;
    }

    fprintf(f, "# Exported mesh\n");

    // Write vertices
    for (int i = 0; i < mesh.vertexCount; i++) {
        float x = mesh.vertices[i*3 + 0];
        float y = mesh.vertices[i*3 + 1];
        float z = mesh.vertices[i*3 + 2];
        fprintf(f, "v %f %f %f\n", x, y, z);
    }

    // Write texture coordinates (if any)
    if (mesh.texcoords) {
        for (int i = 0; i < mesh.vertexCount; i++) {
            float u = mesh.texcoords[i*2 + 0];
            float v = mesh.texcoords[i*2 + 1];
            fprintf(f, "vt %f %f\n", u, v);
        }
    }

    // Write normals (if any)
    if (mesh.normals) {
        for (int i = 0; i < mesh.vertexCount; i++) {
            float nx = mesh.normals[i*3 + 0];
            float ny = mesh.normals[i*3 + 1];
            float nz = mesh.normals[i*3 + 2];
            fprintf(f, "vn %f %f %f\n", nx, ny, nz);
        }
    }

    // Write faces
    for (int i = 0; i < mesh.triangleCount; i++) {
        unsigned short i0 = mesh.indices[i*3 + 0] + 1; // OBJ is 1-based
        unsigned short i1 = mesh.indices[i*3 + 1] + 1;
        unsigned short i2 = mesh.indices[i*3 + 2] + 1;

        if (mesh.texcoords && mesh.normals)
            fprintf(f, "f %d/%d/%d %d/%d/%d %d/%d/%d\n", 
                i0, i0, i0, i1, i1, i1, i2, i2, i2);
        else if (mesh.texcoords)
            fprintf(f, "f %d/%d %d/%d %d/%d\n", 
                i0, i0, i1, i1, i2, i2);
        else if (mesh.normals)
            fprintf(f, "f %d//%d %d//%d %d//%d\n", 
                i0, i0, i1, i1, i2, i2);
        else
            fprintf(f, "f %d %d %d\n", i0, i1, i2);
    }

    fclose(f);
    TraceLog(LOG_INFO, "Exported mesh to %s", filename);
}


// Function to generate a grass patch mesh
//count should be even, so we can divide by 2
Mesh GenerateGrassMesh(int count, float area, float h, float w) 
{
    Mesh mesh = { 0 };

    int vertexCount = count * 6;  // 2 triangles per blade = 6 vertices
    int indexCount = count * 6;   // 6 indices per blade

    mesh.vertexCount = vertexCount;
    mesh.triangleCount = count * 2;

    // Allocate memory for mesh data
    mesh.vertices = (float *)MemAlloc(vertexCount * 3 * sizeof(float));  // x, y, z
    mesh.normals = (float *)MemAlloc(vertexCount * 3 * sizeof(float));   // normals
    mesh.texcoords = (float *)MemAlloc(vertexCount * 2 * sizeof(float)); // UV coords
    mesh.indices = (unsigned short *)MemAlloc(indexCount * sizeof(unsigned short));

    for (int i = 0; i < count/2; i++) {
        // Random position in the grass patch
        float x = ((float)rand() / RAND_MAX) * area - (area / 2);
        float z = ((float)rand() / RAND_MAX) * area - (area / 2);
        float height = h * (0.8f + ((float)rand() / RAND_MAX) * 0.4f); // Small variation

        int v = i * 6;
        int ind = i * 6;

        // Vertex positions for a vertical quad
        float bladeVertices[] = {
            x - w / 2, 0, z,     // Bottom left
            x + w / 2, 0, z,     // Bottom right
            x, height, z,                  // Top center

            x - w / 2, 0, z,     // Bottom left
            x, height, z,                  // Top center
            x + w / 2, 0, z      // Bottom right
        };

        // Normals (pointing upwards for now)
        float bladeNormals[] = {
            0, 1, 0,  0, 1, 0,  0, 1, 0,
            0, 1, 0,  0, 1, 0,  0, 1, 0
        };

        // Simple texture coordinates
        float bladeTexcoords[] = {
            0, 1,  1, 1,  0.5, 0,
            0, 1,  0.5, 0,  1, 1
        };

        // Indices for the triangles
        //unsigned short bladeIndices[] = { v, v+1, v+2, v+3, v+4, v+5 };
        //unsigned short bladeIndices[] = { 0, 1, 2, 3, 4, 5 };

        // Copy data into mesh arrays
        for (int j = 0; j < 18; j++) mesh.vertices[v * 3 + j] = bladeVertices[j];
        for (int j = 0; j < 18; j++) mesh.normals[v * 3 + j] = bladeNormals[j];
        for (int j = 0; j < 12; j++) mesh.texcoords[v * 2 + j] = bladeTexcoords[j];
        //for (int j = 0; j < 6; j++) mesh.indices[ind + j] = bladeIndices[j];
        for (int j = 0; j < 6; j++) mesh.indices[ind + j] = v + j;
    }

    for (int i = count/2; i < count; i++) {
        // Random position in the grass patch
        float x = ((float)rand() / RAND_MAX) * area - (area / 2);
        float z = ((float)rand() / RAND_MAX) * area - (area / 2);
        float height = h * (0.8f + ((float)rand() / RAND_MAX) * 0.4f); // Small variation

        int v = i * 6;
        int ind = i * 6;

        // Vertex positions for a vertical quad
        float bladeVertices[] = {
            x, 0, z - w / 2,     // Bottom left
            x, 0, z + w / 2,     // Bottom right
            x, height, z,                  // Top center

            x, 0, z - w / 2,     // Bottom left
            x, height, z,                  // Top center
            x, 0, z + w / 2     // Bottom right
        };

        // Normals (pointing upwards for now)
        float bladeNormals[] = {
            0, 1, 0,  0, 1, 0,  0, 1, 0,
            0, 1, 0,  0, 1, 0,  0, 1, 0
        };

        // Simple texture coordinates
        float bladeTexcoords[] = {
            0, 1,  1, 1,  0.5, 0,
            0, 1,  0.5, 0,  1, 1
        };

        // Indices for the triangles
        //unsigned short bladeIndices[] = { v, v+1, v+2, v+3, v+4, v+5 };
        //unsigned short bladeIndices[] = { 0, 1, 2, 3, 4, 5 };

        // Copy data into mesh arrays
        for (int j = 0; j < 18; j++) mesh.vertices[v * 3 + j] = bladeVertices[j];
        for (int j = 0; j < 18; j++) mesh.normals[v * 3 + j] = bladeNormals[j];
        for (int j = 0; j < 12; j++) mesh.texcoords[v * 2 + j] = bladeTexcoords[j];
        //for (int j = 0; j < 6; j++) mesh.indices[ind + j] = bladeIndices[j];
        for (int j = 0; j < 6; j++) mesh.indices[ind + j] = v + j;
    }

    // Upload the mesh to the GPU
    UploadMesh(&mesh, true);
    return mesh;
}

float cameraYaw = 0.0f;     // horizontal angle, left/right
float cameraPitch = 20.0f;  // vertical angle, up/down
float cameraDistance = 4.0f;
Vector3 cameraTarget = { 0.0f, 0.0f, 0.0f };

//for ray sphere collision
float m_radius = 1.5f;

int main() {
    Vector3 center = { 0, 0, 0 };
    InitWindow(800, 600, "Rock Editor");
    Camera camera = { 0 };
    camera.position = (Vector3){ 15.0f, 15.0f, 15.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    //setup
    int count=128;
    float area=4.8f;
    float height=0.5f;
    float width=0.1f;
    Mesh grassMesh = GenerateGrassMesh(count, area, height, width);
    Model grassModel = LoadModelFromMesh(grassMesh);
    Texture grassTexture = LoadTexture("textures/grass.png");
    grassModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = grassTexture;
    //end setup

    Vector3 rotation = { 0 };  // Euler angles

    SetTargetFPS(60);
    DisableCursor();
    while (!WindowShouldClose()) {
        // if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        //     Ray ray = GetMouseRay(GetMousePosition(), camera);
        //     RayCollision hit = GetRayCollisionSphere(ray, center, m_radius);
        //     if (hit.hit) {
        //         ApplySmush(&rock, hit.point, 0.4f, 0.5f);
        //     }
        // }
        if (IsKeyPressed(KEY_M)){EnableCursor();}
        if (IsKeyPressed(KEY_R)){DisableCursor();}

        if (IsKeyPressed(KEY_UP)){count+=2;if(count>2048){count=2048;}}
        if (IsKeyPressed(KEY_DOWN)){count-=2;if(count<2){count=2;}}
        if (IsKeyPressed(KEY_RIGHT)){area+=0.1f;if(area>100.0f){area=100.0f;}}
        if (IsKeyPressed(KEY_LEFT)){area-=0.1f;if(area<0.25f){area=0.25f;}}
        if (IsKeyPressed(KEY_P)){height+=0.01f;if(height>16.0f){height=16.0f;}}
        if (IsKeyPressed(KEY_O)){height-=0.01f;if(height<0.01f){height=0.01f;}}
        if (IsKeyPressed(KEY_I)){width+=0.001f;if(width>4.0f){width=4.0f;}}
        if (IsKeyPressed(KEY_U)){width-=0.001f;if(width<0.001f){width=0.001f;}}

        if (IsKeyPressed(KEY_G))
        {
            grassMesh = GenerateGrassMesh(count, area, height, width);
            grassModel = LoadModelFromMesh(grassMesh);
            grassModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = grassTexture;
        }

        if (IsKeyPressed(KEY_E)) {
            TraceLog(LOG_INFO, "Exporting grass (before):");
            TraceLog(LOG_INFO, "  vertexCount: %d", grassMesh.vertexCount);
            TraceLog(LOG_INFO, "  triangleCount: %d", grassMesh.triangleCount);
            TraceLog(LOG_INFO, "  vertices: %p", grassMesh.vertices);
            TraceLog(LOG_INFO, "  indices: %p", grassMesh.indices);
            ExportMesh(grassMesh, "models/new_grass.obj");
            //ExportMeshAsOBJWithIndices(grassMesh, "models/new_grass.obj");
            //ExportMeshAsGLTF(grassMesh, "models");
        }
        float turnSpeed = 1.0f;

        if (IsKeyDown(KEY_A))  cameraYaw -= turnSpeed;
        if (IsKeyDown(KEY_D)) cameraYaw += turnSpeed;
        if (IsKeyDown(KEY_W))    cameraPitch -= turnSpeed;
        if (IsKeyDown(KEY_S))  cameraPitch += turnSpeed;

        // Clamp pitch to avoid flipping
        if (cameraPitch > 89.0f) cameraPitch = 89.0f;
        if (cameraPitch < -89.0f) cameraPitch = -89.0f;

        Vector3 camPos = {
            cameraTarget.x + cameraDistance * cosf(DEG2RAD * cameraPitch) * sinf(DEG2RAD * cameraYaw),
            cameraTarget.y + cameraDistance * sinf(DEG2RAD * cameraPitch),
            cameraTarget.z + cameraDistance * cosf(DEG2RAD * cameraPitch) * cosf(DEG2RAD * cameraYaw)
        };

        camera.position = camPos;
        camera.target = cameraTarget;
        UpdateCamera(&camera, CAMERA_THIRD_PERSON);
        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);
        DrawModel(grassModel, (Vector3){0}, 1.0f, WHITE);
        DrawGrid(10, 1);
        EndMode3D();
        DrawText("G=Generate | E=Export", 10, 10, 20, DARKGRAY);
        DrawText(TextFormat("count (Down/Up)   : %d    ", count), 10, 30, 20, DARKGRAY); //
        DrawText(TextFormat("area (Left/Right) : %.4f  ", area), 10, 50, 20, DARKGRAY); //
        DrawText(TextFormat("height (O/P)      : %.4f  ", height), 10, 70, 20, DARKGRAY); //
        DrawText(TextFormat("width (U/I)       : %.4f  ", width), 10, 90, 20, DARKGRAY); //
        EndDrawing();
    }

    UnloadTexture(grassTexture);
    UnloadModel(grassModel);
    CloseWindow();
    return 0;
}
