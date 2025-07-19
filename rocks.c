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
    Mesh mesh;
    Vertex vertices[MAX_VERTICES];
    int vertexCount;
} EditableRock;

float cameraYaw = 0.0f;     // horizontal angle, left/right
float cameraPitch = 20.0f;  // vertical angle, up/down
float cameraDistance = 4.0f;
Vector3 cameraTarget = { 0.0f, 0.0f, 0.0f };

//for ray sphere collision
float m_radius = 1.5f;

EditableRock GenerateEditableRock(float radius, float noise, int rings, int slices);
void ApplySmush(EditableRock *rock, Vector3 hitPos, float radius, float strength);
void ExportOBJ(EditableRock rock, const char *filename);
Vector3 Perturb(Vector3 v, float magnitude);
EditableRock GenRock(float baseRadius, float noise);
int AddTriangleToMesh(Mesh *mesh, int v, Vector3 p1, Vector3 p2, Vector3 p3);
void UpdateRockUVs(Mesh *mesh, float radius);
void CenterMeshOnGround(Mesh *mesh);

int main() {
    Vector3 center = { 0, 0, 0 };
    InitWindow(800, 600, "Rock Editor");
    Camera camera = { 0 };
    camera.position = (Vector3){ 4.0f, 4.0f, 4.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    EditableRock rock = GenerateEditableRock(1.0f, 0.4f, 6, 8);
    Model model = LoadModelFromMesh(rock.mesh);
    Image image = GenImagePerlinNoise(256, 256, 0, 0, 6.0f);
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
                model = LoadModelFromMesh(rock.mesh);
                model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = rockTex;
            }
        }
        if (IsKeyPressed(KEY_G))
        {
            rock = GenerateEditableRock(1.0f, 0.3f, 16, 24); //this didnt work well -> rock = GenerateEditableRock(1.0f, 0.4f, 6, 8);
            model = LoadModelFromMesh(rock.mesh);
            model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = rockTex;
        }
        if (IsKeyPressed(KEY_R))
        {
            rock = GenRock(1.0f, 0.4f);
            model = LoadModelFromMesh(rock.mesh);
            model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = rockTex;
        }  
        if (IsKeyPressed(KEY_E)) {
            ExportOBJ(rock, "new_baked_asset/new_rock.obj");
            ExportImage(image, "new_baked_asset/new_rock.png");
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

        UpdateCamera(&camera, CAMERA_THIRD_PERSON);
        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);
        DrawModel(model, (Vector3){0}, 1.0f, WHITE);
        DrawGrid(10, 1);
        EndMode3D();
        DrawText("LMB: Smush | E: Export OBJ+PNG", 10, 10, 20, DARKGRAY);
        DrawText("G: re-Generate", 10, 30, 20, DARKGRAY);
        DrawText("R: re-Generate (diff alg.)", 10, 50, 20, DARKGRAY);
        EndDrawing();
    }

    UnloadTexture(rockTex);
    UnloadModel(model);
    CloseWindow();
    return 0;
}

EditableRock GenerateEditableRock(float radius, float noise, int rings, int slices) {
    EditableRock rock = { 0 };
    int v = 0;

    // Top vertex
    rock.vertices[v++].position = (Vector3){ 0, radius, 0 };

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
            rock.vertices[v++].position = Vector3Scale(Vector3Normalize(dir), radius);
        }
    }

    // Bottom vertex
    rock.vertices[v++].position = (Vector3){ 0, -radius, 0 };
    rock.vertexCount = v;

    // Triangle count: (rings - 2) * slices * 2 + top + bottom
    int quads = (rings - 2) * slices;
    int topTris = slices;
    int bottomTris = slices;
    int totalTris = quads * 2 + topTris + bottomTris;
    int vertCount = totalTris * 3;

    Mesh mesh = { 0 };
    mesh.vertexCount = vertCount;
    mesh.triangleCount = totalTris;
    mesh.vertices = MemAlloc(sizeof(float) * 3 * vertCount);
    mesh.normals = MemAlloc(sizeof(float) * 3 * vertCount);
    mesh.texcoords = MemAlloc(sizeof(float) * 2 * vertCount);

    v = 0;

    // Top cap
    for (int j = 0; j < slices; j++) {
        int next = (j + 1) % slices;
        Vector3 p0 = rock.vertices[0].position;
        Vector3 p1 = rock.vertices[1 + j].position;
        Vector3 p2 = rock.vertices[1 + next].position;

        v = AddTriangleToMesh(&mesh, v, p0, p1, p2);
    }

    // Body
    for (int i = 0; i < rings - 2; i++) {
        for (int j = 0; j < slices; j++) {
            int ringStart = 1 + i * slices;
            int nextRingStart = ringStart + slices;
            int next = (j + 1) % slices;

            Vector3 p1 = rock.vertices[ringStart + j].position;
            Vector3 p2 = rock.vertices[nextRingStart + j].position;
            Vector3 p3 = rock.vertices[ringStart + next].position;
            Vector3 p4 = rock.vertices[nextRingStart + next].position;

            // Triangle 1
            v = AddTriangleToMesh(&mesh, v, p1, p2, p3);
            // Triangle 2
            v = AddTriangleToMesh(&mesh, v, p2, p4, p3);
        }
    }

    // Bottom cap
    int bottomIndex = rock.vertexCount - 1;
    int lastRingStart = bottomIndex - slices;
    for (int j = 0; j < slices; j++) {
        int next = (j + 1) % slices;
        Vector3 p0 = rock.vertices[bottomIndex].position;
        Vector3 p1 = rock.vertices[lastRingStart + next].position;
        Vector3 p2 = rock.vertices[lastRingStart + j].position;

        v = AddTriangleToMesh(&mesh, v, p0, p1, p2);
    }

    rock.mesh = mesh;
    CenterMeshOnGround(&rock.mesh);
    UpdateRockUVs(&rock.mesh, radius);
    UploadMesh(&rock.mesh, true);
    return rock;
}


void ApplySmush(EditableRock *rock, Vector3 hitPos, float radius, float strength) {
    for (int i = 0; i < rock->vertexCount; i++) {
        float dist = Vector3Distance(rock->vertices[i].position, hitPos);
        if (dist < radius) {
            Vector3 pull = Vector3Scale(Vector3Normalize(Vector3Subtract(hitPos, rock->vertices[i].position)), strength);
            rock->vertices[i].position = Vector3Add(rock->vertices[i].position, pull);
        }
    }
    // Reupload mesh
    for (int i = 0; i < rock->vertexCount; i++) {
        ((float *)rock->mesh.vertices)[i*3+0] = rock->vertices[i].position.x;
        ((float *)rock->mesh.vertices)[i*3+1] = rock->vertices[i].position.y;
        ((float *)rock->mesh.vertices)[i*3+2] = rock->vertices[i].position.z;
    }
    CenterMeshOnGround(&rock->mesh);
    UpdateRockUVs(&rock->mesh, radius);
    UploadMesh(&rock->mesh, true);
}

void ExportOBJ(EditableRock rock, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return;
    for (int i = 0; i < rock.vertexCount; i++) {
        fprintf(f, "v %f %f %f\n", rock.vertices[i].position.x, rock.vertices[i].position.y, rock.vertices[i].position.z);
    }
    // No faces written (optional for full .obj), but can be extended
    fclose(f);
}



//old stuff that works okay
Vector3 Perturb(Vector3 v, float magnitude) {
    v.x += ((float)rand()/RAND_MAX - 0.5f) * magnitude;
    v.y += ((float)rand()/RAND_MAX - 0.5f) * magnitude;
    v.z += ((float)rand()/RAND_MAX - 0.5f) * magnitude;
    return Vector3Normalize(v); // Normalize to retain roundness
}

EditableRock GenRock(float baseRadius, float noise)
{
    EditableRock rock = { 0 };
    Mesh mesh = { 0 };

    const int rings = 6;
    const int slices = 8;

    int vertexCount = (rings - 1) * slices + 2;
    int triCount = slices * 2 + (rings - 2) * slices * 2;

    float *vertices = MemAlloc(vertexCount * 3 * sizeof(float));
    float *normals = MemAlloc(vertexCount * 3 * sizeof(float));
    float *texcoords = MemAlloc(vertexCount * 2 * sizeof(float));
    unsigned short *indices = MemAlloc(triCount * 3 * sizeof(unsigned short));

    int v = 0;

    // Top pole
    vertices[v++] = 0;
    vertices[v++] = baseRadius;
    vertices[v++] = 0;

    // Rings
    for (int i = 1; i < rings; i++) {
        float phi = PI * i / rings;
        float y = cosf(phi);
        float r = sinf(phi);
        for (int j = 0; j < slices; j++) {
            float theta = 2.0f * PI * j / slices;
            float x = r * cosf(theta);
            float z = r * sinf(theta);

            Vector3 dir = Perturb((Vector3){ x, y, z }, noise);
            vertices[v++] = dir.x * baseRadius;
            vertices[v++] = dir.y * baseRadius;
            vertices[v++] = dir.z * baseRadius;
        }
    }

    // Bottom pole
    vertices[v++] = 0;
    vertices[v++] = -baseRadius;
    vertices[v++] = 0;

    int top = 0;
    int bottom = vertexCount - 1;
    int idx = 0;

    // Top cap
    for (int i = 0; i < slices; i++) {
        indices[idx++] = top;
        indices[idx++] = 1 + i;
        indices[idx++] = 1 + (i + 1) % slices;
    }

    // Middle quads
    for (int i = 0; i < rings - 2; i++) {
        for (int j = 0; j < slices; j++) {
            int a = 1 + i * slices + j;
            int b = 1 + i * slices + (j + 1) % slices;
            int c = a + slices;
            int d = b + slices;

            indices[idx++] = a; indices[idx++] = c; indices[idx++] = b;
            indices[idx++] = b; indices[idx++] = c; indices[idx++] = d;
        }
    }

    // Bottom cap
    int offset = 1 + (rings - 2) * slices;
    for (int i = 0; i < slices; i++) {
        indices[idx++] = offset + i;
        indices[idx++] = bottom;
        indices[idx++] = offset + (i + 1) % slices;
    }

    mesh.vertexCount = vertexCount;
    mesh.triangleCount = triCount;

    mesh.vertices = vertices;
    mesh.normals = normals;     // You can use GenMeshTangents() later
    mesh.texcoords = texcoords;
    mesh.indices = indices;

    rock.mesh = mesh;
    CenterMeshOnGround(&rock.mesh);
    UpdateRockUVs(&rock.mesh, m_radius);
    UploadMesh(&rock.mesh, true);
    return rock;
}

int AddTriangleToMesh(Mesh *mesh, int v, Vector3 p1, Vector3 p2, Vector3 p3) 
{
    // Store vertices
    float *verts = (float *)mesh->vertices;
    verts[v*3 + 0] = p1.x;
    verts[v*3 + 1] = p1.y;
    verts[v*3 + 2] = p1.z;
    verts[v*3 + 3] = p2.x;
    verts[v*3 + 4] = p2.y;
    verts[v*3 + 5] = p2.z;
    verts[v*3 + 6] = p3.x;
    verts[v*3 + 7] = p3.y;
    verts[v*3 + 8] = p3.z;

    // Calculate normal
    Vector3 normal = Vector3Normalize(Vector3CrossProduct(
        Vector3Subtract(p2, p1), Vector3Subtract(p3, p1)));

    float *normals = (float *)mesh->normals;
    for (int i = 0; i < 3; i++) {
        normals[(v + i)*3 + 0] = normal.x;
        normals[(v + i)*3 + 1] = normal.y;
        normals[(v + i)*3 + 2] = normal.z;
    }

    float *uv = (float *)mesh->texcoords;
    for (int i = 0; i < 3; i++) {
        uv[(v + i)*2 + 0] = 0.0f;
        uv[(v + i)*2 + 1] = 0.0f;
    }

    return v + 3;
}

void UpdateRockUVs(Mesh *mesh, float radius) 
{
    if (!mesh->vertices || !mesh->texcoords) return;

    for (int i = 0; i < mesh->vertexCount; i++) {
        float x = mesh->vertices[i * 3 + 0];
        float y = mesh->vertices[i * 3 + 1];
        float z = mesh->vertices[i * 3 + 2];

        // Normalize to unit sphere
        float nx = x / radius;
        float ny = y / radius;
        float nz = z / radius;

        float u = 0.5f + (atan2f(nz, nx) / (2.0f * PI));
        float v = 0.5f - (asinf(ny) / PI);

        mesh->texcoords[i * 2 + 0] = u;
        mesh->texcoords[i * 2 + 1] = v;
    }
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

