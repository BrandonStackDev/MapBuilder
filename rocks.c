// rock_editor.c
#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_VERTICES 2048

#define VERTEX_AT(v, i) (Vector3){ v[i*3 + 0], v[i*3 + 1], v[i*3 + 2] }

typedef struct {
    Vector3 position;
    Vector3 normal;
} Vertex;

typedef struct {
    bool isDirty, meshWasSet;
    Mesh mesh; //always gets finalized before draw
    Vertex vertices[MAX_VERTICES];
    int vertexCount;
    int ringCount;
    int sliceCount;
} EditableRock;

float cameraYaw = 0.0f;     // horizontal angle, left/right
float cameraPitch = 20.0f;  // vertical angle, up/down
float cameraDistance = 4.0f;
Vector3 cameraTarget = { 0.0f, 0.0f, 0.0f };

//for ray sphere collision
float m_radius = 1.5f;

void GenerateEditableRock(EditableRock *rock, float radius, float noise, int rings, int slices);
void ApplySmush(EditableRock *rock, Vector3 hitPos, float radius, float strength);
void FinalizeEditableRock(EditableRock *rock);
//old but maybe useful
void CenterMeshOnGround(Mesh *mesh);
Vector3 Perturb(Vector3 v, float magnitude);

int main() {
    Vector3 center = { 0, 0, 0 };
    InitWindow(800, 600, "Rock Editor");
    Camera camera = { 0 };
    camera.position = (Vector3){ 15.0f, 15.0f, 15.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    EditableRock rock = { 0 };
    GenerateEditableRock(&rock, 1.0f, 0.4f, 6, 8);
    Model model = LoadModelFromMesh(rock.mesh);
    Image image = GenImagePerlinNoise(64, 64, 0, 0, 6.0f);
    Texture2D rockTex = LoadTextureFromImage(image);
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = rockTex;

    Vector3 rotation = { 0 };  // Euler angles

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            Ray ray = GetMouseRay(GetMousePosition(), camera);
            RayCollision hit = GetRayCollisionSphere(ray, center, m_radius);
            if (hit.hit) {
                ApplySmush(&rock, hit.point, 0.4f, 0.5f);
                //rock.mesh = rock.mesh;  // already updated
                
            }
        }
        if (IsKeyPressed(KEY_G))
        {
            GenerateEditableRock(&rock, 1.0f, 0.3f, 16, 24); //this didnt work well -> rock = GenerateEditableRock(1.0f, 0.4f, 6, 8);
            if(rock.isDirty){
                FinalizeEditableRock(&rock);
                model = LoadModelFromMesh(rock.mesh);
                model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = rockTex;
            }
        }
        if (IsKeyPressed(KEY_E)) {
             TraceLog(LOG_INFO, "Exporting rock (before):");
            TraceLog(LOG_INFO, "  vertexCount: %d", rock.mesh.vertexCount);
            TraceLog(LOG_INFO, "  triangleCount: %d", rock.mesh.triangleCount);
            TraceLog(LOG_INFO, "  vertices: %p", rock.mesh.vertices);
            TraceLog(LOG_INFO, "  indices: %p", rock.mesh.indices);

            if(rock.isDirty){
                FinalizeEditableRock(&rock);
                model = LoadModelFromMesh(rock.mesh);
                model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = rockTex;
            }
            TraceLog(LOG_INFO, "Exporting rock (after):");
            TraceLog(LOG_INFO, "  vertexCount: %d", rock.mesh.vertexCount);
            TraceLog(LOG_INFO, "  triangleCount: %d", rock.mesh.triangleCount);
            TraceLog(LOG_INFO, "  vertices: %p", rock.mesh.vertices);
            TraceLog(LOG_INFO, "  indices: %p", rock.mesh.indices);

            ExportMesh(rock.mesh, "models/new_rock.obj");
            ExportImage(image, "textures/new_rock.png");
        }
        float turnSpeed = 1.0f;

        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))  cameraYaw -= turnSpeed;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) cameraYaw += turnSpeed;
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))    cameraPitch -= turnSpeed;
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))  cameraPitch += turnSpeed;

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
        if(rock.isDirty){
            FinalizeEditableRock(&rock);
            model = LoadModelFromMesh(rock.mesh);
            model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = rockTex;
        }
        UpdateCamera(&camera, CAMERA_THIRD_PERSON);
        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);
        DrawModel(model, (Vector3){0}, 1.0f, WHITE);
        DrawGrid(10, 1);
        EndMode3D();
        DrawText("LMB: Smush | E: Export OBJ+PNG", 10, 10, 20, DARKGRAY);
        DrawText("G: Generate", 10, 30, 20, DARKGRAY);
        EndDrawing();
    }

    UnloadTexture(rockTex);
    UnloadModel(model);
    CloseWindow();
    return 0;
}

void GenerateEditableRock(EditableRock *rock, float radius, float noise, int rings, int slices) {
    int v = 0;

    // Top vertex
    rock->vertices[v++].position = (Vector3){ 0, radius, 0 };

    for (int i = 1; i < rings; i++) {
        float phi = PI * i / rings;
        float y = cosf(phi);
        float r = sinf(phi);
        for (int j = 0; j < slices; j++) {
            float theta = 2 * PI * j / slices;
            float x = r * cosf(theta);
            float z = r * sinf(theta);
            Vector3 dir = Vector3Normalize((Vector3){ x, y, z });
            dir.x += ((float)rand()/RAND_MAX - 0.5f) * noise;
            dir.y += ((float)rand()/RAND_MAX - 0.5f) * noise;
            dir.z += ((float)rand()/RAND_MAX - 0.5f) * noise;
            rock->vertices[v++].position = Vector3Scale(Vector3Normalize(dir), radius);
        }
    }

    // Bottom vertex
    rock->ringCount = rings;
    rock->sliceCount = slices;
    rock->vertices[v++].position = (Vector3){ 0, -radius, 0 };
    rock->vertexCount = v;
    rock->isDirty = true;
}


void ApplySmush(EditableRock *rock, Vector3 hitPos, float radius, float strength) {
    TraceLog(LOG_INFO, "broken");
}

//old stuff that works okay
Vector3 Perturb(Vector3 v, float magnitude) {
    v.x += ((float)rand()/RAND_MAX - 0.5f) * magnitude;
    v.y += ((float)rand()/RAND_MAX - 0.5f) * magnitude;
    v.z += ((float)rand()/RAND_MAX - 0.5f) * magnitude;
    return Vector3Normalize(v); // Normalize to retain roundness
}


void CenterMeshOnGround(Mesh *mesh) {
    if (!mesh || !mesh->vertices) return;

    float minY = 99999999;

    // Find the lowest Y value
    for (int i = 0; i < mesh->vertexCount; i++) {
        float y = mesh->vertices[i * 3 + 1];
        if (y < minY) minY = y;
    }

    // Push all vertices up so minY == 0
    for (int i = 0; i < mesh->vertexCount; i++) {
        mesh->vertices[i * 3 + 1] -= minY;
    }
}

void FinalizeEditableRock(EditableRock *rock) {
    if (!rock->isDirty) return;

    // === Count and allocate mesh ===
    int rings = rock->ringCount;
    int slices = rock->sliceCount;

    int triangleCount = (rings - 1) * slices * 2; // body only, you can add caps later
    int vertexCount = triangleCount * 3;

    TraceLog(LOG_INFO, ">> rings = %d, slices = %d", rings, slices);
    TraceLog(LOG_INFO, ">> Computed triangleCount = %d", triangleCount);
    TraceLog(LOG_INFO, ">> Computed vertexCount = %d", vertexCount);


    Mesh mesh = { 0 };
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = triangleCount;

    mesh.vertices = MemAlloc(sizeof(float) * 3 * vertexCount);
    mesh.normals = MemAlloc(sizeof(float) * 3 * vertexCount);
    mesh.texcoords = MemAlloc(sizeof(float) * 2 * vertexCount);
    mesh.indices = MemAlloc(sizeof(unsigned short) * triangleCount * 3); // optional for export

    int v = 0;
    int idx = 0;

    for (int i = 0; i < rings - 1; i++) {
        for (int j = 0; j < slices; j++) {
            int a = 1 + i * slices + j;
            int b = 1 + (i + 1) * slices + j;
            int a1 = 1 + i * slices + (j + 1) % slices;
            int b1 = 1 + (i + 1) * slices + (j + 1) % slices;

            Vector3 v0 = rock->vertices[a].position;
            Vector3 v1 = rock->vertices[b].position;
            Vector3 v2 = rock->vertices[a1].position;
            Vector3 v3 = rock->vertices[b1].position;

            // Triangle 1
            Vector3 n1 = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(v1, v0), Vector3Subtract(v2, v0)));
            for (int k = 0; k < 3; k++) {
                Vector3 p = (k == 0) ? v0 : (k == 1) ? v1 : v2;
                mesh.vertices[v*3 + 0] = p.x;
                mesh.vertices[v*3 + 1] = p.y;
                mesh.vertices[v*3 + 2] = p.z;
                mesh.normals[v*3 + 0] = n1.x;
                mesh.normals[v*3 + 1] = n1.y;
                mesh.normals[v*3 + 2] = n1.z;
                mesh.texcoords[v*2 + 0] = 0.5f + p.x * 0.5f;
                mesh.texcoords[v*2 + 1] = 0.5f + p.z * 0.5f;
                mesh.indices[idx++] = v;
                v++;
            }

            // Triangle 2
            Vector3 n2 = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(v3, v1), Vector3Subtract(v2, v1)));
            for (int k = 0; k < 3; k++) {
                Vector3 p = (k == 0) ? v1 : (k == 1) ? v3 : v2;
                mesh.vertices[v*3 + 0] = p.x;
                mesh.vertices[v*3 + 1] = p.y;
                mesh.vertices[v*3 + 2] = p.z;
                mesh.normals[v*3 + 0] = n2.x;
                mesh.normals[v*3 + 1] = n2.y;
                mesh.normals[v*3 + 2] = n2.z;
                mesh.texcoords[v*2 + 0] = 0.5f + p.x * 0.5f;
                mesh.texcoords[v*2 + 1] = 0.5f + p.z * 0.5f;
                mesh.indices[idx++] = v;
                v++;
            }
        }
    }

    CenterMeshOnGround(&mesh);
    //rock->mesh.vertices = mesh.vertices;
    //rock->mesh.normals = mesh.normals;
   // rock->mesh.texcoords = mesh.texcoords;
    //rock->mesh.indices = mesh.indices;
    //UploadMesh(&mesh, false);
    // Replace the old mesh
    //if(rock->meshWasSet){UnloadMesh(rock->mesh);} // in case there was one
    rock->mesh = mesh;
    rock->mesh.vertexCount = mesh.vertexCount;
    rock->mesh.triangleCount = mesh.triangleCount;
    UploadMesh(&rock->mesh, false);
    rock->isDirty = false;
    rock->meshWasSet = true;
    TraceLog(LOG_INFO, "Vertex Count: %d", rock->mesh.vertexCount);
    TraceLog(LOG_INFO, "Triangle Count: %d", rock->mesh.triangleCount);
}



