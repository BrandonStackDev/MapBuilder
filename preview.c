
//raylib
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
//me
#include "models.h"
#include "gpu.h"
//fairlry standard things
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> // for usleep
#include <string.h>
#include <time.h> // for seeding rand once if needed
//for big report numbers
#include <stdint.h>
#include <inttypes.h>


#define MAX_CHUNK_DIM 16
#define CHUNK_COUNT 16
#define CHUNK_SIZE 64
#define CHUNK_WORLD_SIZE 1024.0f
#define MAX_WORLD_SIZE (CHUNK_COUNT * CHUNK_WORLD_SIZE)
#define WORLD_WIDTH  MAX_WORLD_SIZE
#define WORLD_HEIGHT MAX_WORLD_SIZE
#define GAME_MAP_SIZE 128
#define MAP_SIZE (CHUNK_COUNT * CHUNK_SIZE)
#define MAX_CHUNKS_TO_QUEUE (CHUNK_COUNT * CHUNK_COUNT)
#define MAP_SCALE 16
#define MAP_VERTICAL_OFFSET 0 //(MAP_SCALE * -64)
#define PLAYER_HEIGHT 1.7f
#define FULL_TREE_DIST 85.42f //112.2f

//chunk tile system
#define TILE_GRID_SIZE 8 //sync with main.c
#define TILE_WORLD_SIZE (CHUNK_WORLD_SIZE / TILE_GRID_SIZE)
#define WORLD_ORIGIN_OFFSET (CHUNK_COUNT / 2 * CHUNK_WORLD_SIZE)
#define MAX_TILES ((CHUNK_WORLD_SIZE * CHUNK_WORLD_SIZE / TILE_GRID_SIZE / TILE_GRID_SIZE));
#define ACTIVE_TILE_GRID_OFFSET 1 //controls the size of the active tile grid, set to 0=1x1, 1=3x3, 2=5x5 etc... (0 may not work?)
#define TILE_GPU_UPLOAD_GRID_DIST 4

//water
#define MAX_WATER_PATCHES_PER_CHUNK 64
#define WATER_Y_OFFSET 60.0f
#define PLAYER_FLOAT_OFFSET 339.9f


//movement
#define GOKU_DASH_DIST 512.333f
#define GOKU_DASH_DIST_SHORT 128.2711f
#define MOVE_SPEED 16.16f

//screen
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 800

//raylib gpu system stuff
#define MAX_MESH_VERTEX_BUFFERS 7

//display/render settings
#define USE_TREE_CUBES false
#define USE_TILES_ONLY false
#define USE_GPU_INSTANCING true

//pthread
//pthread_mutex_t tileMutex = PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t chunkMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//enums
typedef enum {
    LOD_64,
    LOD_32,
    LOD_16,
    LOD_8
} TypeLOD;

typedef struct {
    int id;
    int cx;
    int cy;
    bool isLoaded; //in GPU
    bool isReady; //in RAM
    bool isTextureReady;
    bool isTextureLoaded;
    BoundingBox box;
    BoundingBox origBox;
    Model model;
    Model model32;
    Model model16;
    Model model8;
    Mesh mesh;
    Mesh mesh32;
    Mesh mesh16;
    Mesh mesh8;
    TypeLOD lod;
    Image img_tex;
    Image img_tex_big;
    Image img_tex_full;
    Image img_tex_damn;
    Texture2D texture;
    Texture2D textureBig;
    Texture2D textureFull;
    Texture2D textureDamn;
    Vector3 position;
    Vector3 center;
    StaticGameObject *props;
    int treeCount;
    int curTreeIdx;
    Model *water;
    int waterCount;
} Chunk;

//tiles-------------------------------------------------------------------------
typedef struct {
    int cx, cy;
    int tx, ty;
    char path[256];
    Mesh mesh;
    //Model_Type type; eventually it might be nice so we could tell what we are drawing if we desire
    Model model;
    BoundingBox box;
    bool isReady, isLoaded; //is ready but !isloaded means needs gpu upload
    Model_Type type;
} TileEntry;

typedef struct {
    float angle; //radians
    float rate;      // vertical up/down rate
    Vector3 pos;
    BoundingBox origBox, box;
    float timer; 
    float alpha; 
} LightningBug;
#define BUG_COUNT 256 //oh yeah!
typedef struct {
    Vector3 pos;
    float timer; 
    float alpha; 
} Star;
#define STAR_COUNT 512 //oh yeah!
Color starColors[4] = {
    (Color){255, 200, 100, 255}, // warm
    (Color){100, 200, 255, 255}, // cool
    (Color){255, 255, 255, 255}, // white
    (Color){200, 100, 255, 255}  // purple-ish
};
//////////////////////IMPORTANT GLOBAL VARIABLES///////////////////////////////
//int curTreeIdx = 0;
int tree_elf = 0;
//very very important
int chosenX = 7;
int chosenY = 7;
int closestCX = 7;
int closestCY = 7;
bool onLoad = false;
Vector3 lastLBSpawnPosition ={0};
TileEntry *foundTiles = NULL; //will be quite large potentially (in reality not as much)
int foundTileCount = 0;
int manifestTileCount = 2048; //start with a guess, not 0 because used as a denominatorfor the load bar
int waterManifestCount = 0; //this is required so start at 0
bool wasTilesDocumented = false;
Color chunk_16_color = (Color){255,255,255,220};
Color chunk_08_color = (Color){255,255,255,180};
Chunk **chunks = NULL;
Vector3 cameraVelocity = { 0 };
Mesh skyboxPanelMesh;
Model skyboxPanelFrontModel;
Model skyboxPanelBackModel;
Model skyboxPanelLeftModel;
Model skyboxPanelRightModel;
Model skyboxPanelUpModel;
Color backGroundColor = SKYBLUE;
bool dayTime = true;
Color skyboxDay     = (Color){ 255, 255, 255, 180 };
Color skyboxNight   = (Color){   8,  10,  80, 253 };
Color backgroundDay = (Color){ 255, 255, 255, 255 };
Color backgroundNight = (Color){  7,   8, 120, 255 };
Vector3 LightPosTargetDay = (Vector3){ 50.0f, 50.0f, 0.0f };
Vector3 LightPosTargetNight = (Vector3){ 4.0f, 30.0f, 35.0f };
Vector3 LightPosDraw = (Vector3){ 4.0f, 30.0f, 35.0f };
Vector3 LightTargetTargetDay = (Vector3){ 1.0f, 0.0f, 0.0f };
Vector3 LightTargetTargetNight = (Vector3){ 4.0f, 50.0f, 5.0f };
Vector3 LightTargetDraw = (Vector3){ 4.0f, 0.0f, 5.0f };
Color lightColorTargetNight = (Color){  20,   30, 140, 202 };
Color lightColorTargetDay = (Color){  102, 191, 255, 255 };
Color lightColorDraw = (Color){  102, 191, 255, 255 };
Color lightTileColor = (Color){  254, 254, 254, 254 };
////////////////////////////////////////////////////////////////////////////////
BoundingBox UpdateBoundingBox(BoundingBox box, Vector3 pos)
{
    // Calculate the half-extents from the current box
    Vector3 halfSize = {
        (box.max.x - box.min.x) / 2.0f,
        (box.max.y - box.min.y) / 2.0f,
        (box.max.z - box.min.z) / 2.0f
    };

    // Create new min and max based on the new center
    BoundingBox movedBox = {
        .min = {
            pos.x - halfSize.x,
            pos.y - halfSize.y,
            pos.z - halfSize.z
        },
        .max = {
            pos.x + halfSize.x,
            pos.y + halfSize.y,
            pos.z + halfSize.z
        }
    };

    return movedBox;
}
/////////////////////////////////////REPORT FUNCTIONS///////////////////////////////////////////
void MemoryReport()
{
    printf("Start Memory Report -> \n");
    printf("FPS                                : %d\n", GetFPS());
    printf("Chunk Memory         (estimated)   : %zu\n", (CHUNK_COUNT * CHUNK_COUNT) * sizeof(Chunk));
    printf("Batched Props Memory (estimated)   : %zu\n", foundTileCount * sizeof(StaticGameObject));
    int64_t ct_8_tri=0, ct_16_tri=0, ct_32_tri=0, ct_64_tri=0;
    int64_t ct_8_vt=0, ct_16_vt=0, ct_32_vt=0, ct_64_vt=0;
    for (int cx = 0; cx < CHUNK_COUNT; cx++)
    {
        for (int cy = 0; cy < CHUNK_COUNT; cy++)
        {
            if (!chunks[cx][cy].isLoaded) {continue;}
            ct_64_tri += chunks[cx][cy].model.meshes[0].triangleCount;
            ct_64_vt += chunks[cx][cy].model.meshes[0].vertexCount;
            ct_32_tri += chunks[cx][cy].model32.meshes[0].triangleCount;
            ct_32_vt += chunks[cx][cy].model32.meshes[0].vertexCount;
            ct_16_tri += chunks[cx][cy].model16.meshes[0].triangleCount;
            ct_16_vt += chunks[cx][cy].model16.meshes[0].vertexCount;
            ct_8_tri += chunks[cx][cy].model8.meshes[0].triangleCount;
            ct_8_vt += chunks[cx][cy].model8.meshes[0].vertexCount;
        }
    }
    printf("CHUNK 64 Triangles             : %" PRId64 "\n", ct_64_tri);
    printf("CHUNK 64 Vertices              : %" PRId64 "\n", ct_64_vt);
    printf("CHUNK 32 Triangles             : %" PRId64 "\n", ct_32_tri);
    printf("CHUNK 32 Vertices              : %" PRId64 "\n", ct_32_vt);
    printf("CHUNK 16 Triangles             : %" PRId64 "\n", ct_16_tri);
    printf("CHUNK 16 Vertices              : %" PRId64 "\n", ct_16_vt);
    printf("CHUNK 08 Triangles             : %" PRId64 "\n", ct_8_tri);
    printf("CHUNK 08 Vertices              : %" PRId64 "\n", ct_8_vt);
    int64_t chunkTotalTri = ct_8_tri + ct_16_tri + ct_32_tri + ct_64_tri;
    int64_t chunkTotalVert = ct_8_vt + ct_16_vt + ct_32_vt + ct_64_vt;
    printf("CHUNK TOTAL Triangles          : %" PRId64 "\n", chunkTotalTri);
    printf("CHUNK TOTAL Vertices           : %" PRId64 "\n", chunkTotalVert);
    printf("(found tiles %d)\n", foundTileCount);
    int64_t tileGpuTri=0, tileGpuVert=0;
    int64_t tileTotalTri=0, tileTotalVert=0;
    for (int i=0; i<foundTileCount; i++)
    {
        if(!foundTiles[i].isReady){ continue; }
        tileTotalTri+=foundTiles[i].model.meshes[0].triangleCount;
        tileTotalVert+=foundTiles[i].model.meshes[0].vertexCount;
        if(!foundTiles[i].isLoaded){ continue; }
        tileGpuTri+=foundTiles[i].model.meshes[0].triangleCount;
        tileGpuVert+=foundTiles[i].model.meshes[0].vertexCount;
    }
    printf("GPU   Tile Triangles           : %" PRId64 "\n", tileGpuTri);
    printf("GPU   Tile Vertices            : %" PRId64 "\n", tileGpuVert);
    printf("Total Tile Triangles           : %" PRId64 "\n", tileTotalTri);
    printf("Total Tile Vertices            : %" PRId64 "\n", tileTotalVert);
    printf("Total Triangles                : %" PRId64 "\n", tileTotalTri + chunkTotalTri);
    printf("Total Vertices                 : %" PRId64 "\n", tileTotalVert + chunkTotalVert);
    printf("   ->   ->   End Memory Report. \n");
}

void GridChunkReport()
{
    printf("Start Chunk Grid Report -> \n");
    for (int cx = 0; cx < CHUNK_COUNT; cx++)
    {
        for (int cy = 0; cy < CHUNK_COUNT; cy++)
        {
            if(chunks[cx][cy].lod==LOD_8){continue;}//dont show these as they are numerous and beligerant
            printf("Chunk %d %d - %d\n", cx, cy, chunks[cx][cy].lod);
        }
    }
    printf("   ->   ->   End Chunk Grid Report. \n");
}

void GridTileReport()
{
    printf("Start Tile Grid Report -> \n");
    for (int cx = 0; cx < CHUNK_COUNT; cx++)
    {
        for (int cy = 0; cy < CHUNK_COUNT; cy++)
        {
            if(chunks[cx][cy].lod!=LOD_64){continue;}//we only care about the active tile grid
            for (int i=0; i<foundTileCount; i++)
            {
                if(foundTiles[i].cx==cx&&foundTiles[i].cy==cy)
                {
                    printf("(%d,%d) - [%d,%d] - {%d,%d} - %s\n", 
                        cx, cy, 
                        foundTiles[i].tx, foundTiles[i].ty, 
                        foundTiles[i].isReady, foundTiles[i].isLoaded,
                        GetModelName(foundTiles[i].type)
                    );
                }
            }
        }
    }
    printf("   ->   ->   End Tile Grid Report. \n");
}

////////////////////////////////////////////////////////////////////////////////
// Barycentric interpolation to get Y at point (x, z) on triangle
float GetHeightOnTriangle(Vector3 p, Vector3 a, Vector3 b, Vector3 c)
{
    // Convert to 2D XZ plane
    float px = p.x, pz = p.z;

    float ax = a.x, az = a.z;
    float bx = b.x, bz = b.z;
    float cx = c.x, cz = c.z;

    // Compute vectors
    float v0x = bx - ax;
    float v0z = bz - az;
    float v1x = cx - ax;
    float v1z = cz - az;
    float v2x = px - ax;
    float v2z = pz - az;

    // Compute dot products
    float d00 = v0x * v0x + v0z * v0z;
    float d01 = v0x * v1x + v0z * v1z;
    float d11 = v1x * v1x + v1z * v1z;
    float d20 = v2x * v0x + v2z * v0z;
    float d21 = v2x * v1x + v2z * v1z;

    // Compute barycentric coordinates
    float denom = d00 * d11 - d01 * d01;
    if (denom == 0.0f)
    {
        //TraceLog(LOG_INFO, "denom == 0");
        return -10000.0f;
    }

    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;

    // If point is outside triangle
    if (u < 0 || v < 0 || w < 0)
    {
        //TraceLog(LOG_INFO, "Outside of plane (%.2f,%.2f,%.2f)", u, v, w);
        return -10000.0f;
    }

    // Interpolate Y
    return u * a.y + v * b.y + w * c.y;
}

float GetTerrainHeightFromMeshXZ(Chunk chunk, float x, float z)
{
    Mesh mesh = chunk.model.meshes[0];
    float *verts = (float *)mesh.vertices;
    unsigned short *tris = (unsigned short *)mesh.indices;
    //TraceLog(LOG_INFO, "chunk pos (%f, %f, %f)", chunk.position.x, chunk.position.y, chunk.position.z);
    //TraceLog(LOG_INFO, "chunk cen (%f, %f, %f)", chunk.center.x, chunk.center.y, chunk.center.z);
    if (!verts || mesh.vertexCount < 3 || mesh.triangleCount < 1)
    {
        TraceLog(LOG_WARNING, "Something wrong with collision: (%f x %f)", x, z);
        if(!verts){TraceLog(LOG_WARNING, "!verts");}
        if(mesh.vertexCount < 3){TraceLog(LOG_WARNING, "mesh.vertexCount < 3");}
        if(mesh.triangleCount < 1){TraceLog(LOG_WARNING, "mesh.triangleCount < 1");}
        return -10000.0f;
    }

    Vector3 p = { x, 0.0f, z };

    for (int i = 0; i < mesh.triangleCount; i++) {
        int i0, i1, i2;

        if (tris) {
            i0 = tris[i * 3 + 0];
            i1 = tris[i * 3 + 1];
            i2 = tris[i * 3 + 2];
        } else {
            i0 = i * 3 + 0;
            i1 = i * 3 + 1;
            i2 = i * 3 + 2;
        }

        if (i0 >= mesh.vertexCount || i1 >= mesh.vertexCount || i2 >= mesh.vertexCount){continue;}

        Vector3 a = {
            (MAP_SCALE * verts[i0 * 3 + 0] + chunk.position.x),
            (MAP_SCALE * verts[i0 * 3 + 1] + chunk.position.y),
            (MAP_SCALE * verts[i0 * 3 + 2] + chunk.position.z)
        };
        Vector3 b = {
            (MAP_SCALE * verts[i1 * 3 + 0] + chunk.position.x),
            (MAP_SCALE * verts[i1 * 3 + 1] + chunk.position.y),
            (MAP_SCALE * verts[i1 * 3 + 2] + chunk.position.z)
        };
        Vector3 c = {
            (MAP_SCALE * verts[i2 * 3 + 0] + chunk.position.x),
            (MAP_SCALE * verts[i2 * 3 + 1] + chunk.position.y),
            (MAP_SCALE * verts[i2 * 3 + 2] + chunk.position.z)
        };
        //TraceLog(LOG_INFO, "Tri %d verts: a=(%.2f,%.2f,%.2f)", i, a.x, a.y, a.z);
        //TraceLog(LOG_INFO, "Tri %d verts: b=(%.2f,%.2f,%.2f)", i, b.x, b.y, b.z);
        //TraceLog(LOG_INFO, "Tri %d verts: c=(%.2f,%.2f,%.2f)", i, c.x, c.y, c.z);
        float y = GetHeightOnTriangle((Vector3){x, 0, z}, a, b, c);
        if (y > -9999.0f) return y;
    }

    TraceLog(LOG_WARNING, "Not found in any triangle: (%f x %f)", x, z);
    return -10000.0f; // Not found in any triangle
}
////////////////////////////////////////////////////////////////////////////////
// Strip GPU buffers but keep CPU data
void UnloadMeshGPU(Mesh *mesh) {
    rlUnloadVertexArray(mesh->vaoId);
    for (int i = 0; i < MAX_MESH_VERTEX_BUFFERS; i++) {
        if (mesh->vboId[i] > 0) rlUnloadVertexBuffer(mesh->vboId[i]);
        mesh->vboId[i] = 0;
    }
    mesh->vaoId = 0;
    mesh->vboId[0] = 0;
}

int loadTileCnt = 0; //-- need this counter to be global, counted in these functions
void OpenTiles()
{
    FILE *f = fopen("map/manifest.txt", "r"); // Open for read
    if (f != NULL) {
        char line[512];  // Adjust size based on expected path lengths
        while (fgets(line, sizeof(line), f)) {
            // Remove newline if present
            //char *newline = strchr(line, '\n');
            //if (newline) *newline = '\0';
            int cx, cy, tx, ty, type;
            char path[256];
            if (sscanf(line, "%d %d %d %d %d %255[^\n]", &cx, &cy, &tx, &ty, &type, path) == 6) 
            {
                // Save entry
                    TileEntry entry = { cx, cy, tx, ty };
                    strcpy(entry.path, path);
                    entry.model = LoadModel(entry.path);
                    entry.mesh = entry.model.meshes[0];
                    entry.isReady = true;
                    entry.type = (Model_Type)type;
                    foundTiles[foundTileCount++] = entry;
                    TraceLog(LOG_INFO, "manifest entry: %s", path);
                    loadTileCnt++;
            } 
            else {
                printf("Malformed line: %s\n", line);
            }
        }
        fclose(f);
    }
}
void DocumentTiles(int cx, int cy)
{
    for (int tx = 0; tx < TILE_GRID_SIZE; tx++) {
        for (int ty = 0; ty < TILE_GRID_SIZE; ty++) {
            for (int i=0; i < MODEL_TOTAL_COUNT; i++)
            {
                pthread_mutex_lock(&mutex);
                char path[256];
                snprintf(path, sizeof(path),
                        "map/chunk_%02d_%02d/tile_64/%02d_%02d/tile_%s_64.obj",
                        cx, cy, tx, ty, GetModelName(i));

                FILE *f = fopen(path, "r");
                if (f) {
                    fclose(f);
                    // Save entry
                    TileEntry entry = { cx, cy, tx, ty };
                    strcpy(entry.path, path);
                    entry.model = LoadModel(entry.path);
                    entry.mesh = entry.model.meshes[0];
                    entry.isReady = true;
                    entry.type = (Model_Type)i;
                    foundTiles[foundTileCount++] = entry;
                    TraceLog(LOG_INFO, "Found tile: %s", path);
                    loadTileCnt++;
                }
                pthread_mutex_unlock(&mutex);
            }
        }
    }
}
//
Color LerpColor(Color from, Color to, float t)
{
    Color result = {
        .r = (unsigned char)(from.r + (to.r - from.r) * t),
        .g = (unsigned char)(from.g + (to.g - from.g) * t),
        .b = (unsigned char)(from.b + (to.b - from.b) * t),
        .a = (unsigned char)(from.a + (to.a - from.a) * t)
    };
    return result;
}

Vector3 LerpVector3(Vector3 from, Vector3 to, float t)
{
    Vector3 result = {
        .x = (float)(from.x + (to.x - from.x) * t),
        .y = (float)(from.y + (to.y - from.y) * t),
        .z = (float)(from.z + (to.z - from.z) * t)
    };
    return result;
}

int lightCount = 0;
// Send light properties to shader
// NOTE: Light shader locations should be available
static void UpdateLightPbr(Shader shader, Light light)
{
    SetShaderValue(shader, light.enabledLoc, &light.enabled, SHADER_UNIFORM_INT);
    SetShaderValue(shader, light.typeLoc, &light.type, SHADER_UNIFORM_INT);
    
    // Send to shader light position values
    float position[3] = { light.position.x, light.position.y, light.position.z };
    SetShaderValue(shader, light.positionLoc, position, SHADER_UNIFORM_VEC3);

    // Send to shader light target position values
    float target[3] = { light.target.x, light.target.y, light.target.z };
    SetShaderValue(shader, light.targetLoc, target, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, light.colorLoc, &light.color, SHADER_UNIFORM_VEC4);
    //SetShaderValue(shader, light.intensityLoc, &light.intensity, SHADER_UNIFORM_FLOAT);
}
// Create light with provided data
// NOTE: It updated the global lightCount and it's limited to MAX_LIGHTS
static Light CreateLightPbr(int type, Vector3 position, Vector3 target, Color color, float intensity, Shader shader)
{
    Light light = { 0 };

    if (lightCount < MAX_LIGHTS)
    {
        light.enabled = 1;
        light.type = type;
        light.position = position;
        light.target = target;
        light.color.r = (float)color.r/255.0f;
        light.color.g = (float)color.g/255.0f;
        light.color.b = (float)color.b/255.0f;
        light.color.a = (float)color.a/255.0f;
        //light.intensity = intensity;
        
        // NOTE: Shader parameters names for lights must match the requested ones
        light.enabledLoc = GetShaderLocation(shader, TextFormat("lights[%i].enabled", lightCount));
        light.typeLoc = GetShaderLocation(shader, TextFormat("lights[%i].type", lightCount));
        light.positionLoc = GetShaderLocation(shader, TextFormat("lights[%i].position", lightCount));
        light.targetLoc = GetShaderLocation(shader, TextFormat("lights[%i].target", lightCount));
        light.colorLoc = GetShaderLocation(shader, TextFormat("lights[%i].color", lightCount));
        //light.intensityLoc = GetShaderLocation(shader, TextFormat("lights[%i].intensity", lightCount));
        
        UpdateLightPbr(shader, light);

        lightCount++;
    }

    return light;
}

bool bugGenHappened = false;
LightningBug *GenerateLightningBugs(Vector3 cameraPos, int count, float maxDistance)
{
    LightningBug *bugs = (LightningBug *)malloc(sizeof(LightningBug) * count);
    if (!bugs) return NULL;
    BoundingBox box = {
        .min = (Vector3){ -0.25f, -0.25f, -0.25f },
        .max = (Vector3){  0.25f,  0.25f,  0.25f }
    };
    lastLBSpawnPosition=cameraPos;
    for (int i = 0; i < count; i++)
    {
        float angle = (float)GetRandomValue(0, 359) * DEG2RAD;
        float dist = ((float)GetRandomValue(0, 1000) / 1000.0f) * maxDistance; // random 0 to maxDistance

        float x = cameraPos.x + cosf(angle) * dist;
        float z = cameraPos.z + sinf(angle) * dist;
        bugs[i].angle = 0.0f;
        bugs[i].pos = (Vector3){ x, 0.0f, z }; // you'll set .y later
        bugs[i].pos.y = GetTerrainHeightFromMeshXZ(chunks[closestCX][closestCY], bugs[i].pos.x, bugs[i].pos.z);
        bugs[i].pos.y = bugs[i].pos.y + GetRandomValue(1, 10);
        if(bugs[i].pos.y<-5000){bugs[i].pos.y=500;}
        bugs[i].rate = GetRandomValue(0.1f, 10.01f);
        bugs[i].origBox = box;
        bugs[i].box = UpdateBoundingBox(box,bugs[i].pos);
    }

    return bugs;
}

void RegenerateLightningBugs(LightningBug *bugs, Vector3 cameraPos, int count, float maxDistance)
{
    BoundingBox box = {
        .min = (Vector3){ -0.25f, -0.25f, -0.25f },
        .max = (Vector3){  0.25f,  0.25f,  0.25f }
    };
    lastLBSpawnPosition = cameraPos;
    //if (!bugs) return NULL;
    for (int i = 0; i < count; i++)
    {
        float angle = (float)GetRandomValue(0, 359) * DEG2RAD;
        float dist = ((float)GetRandomValue(0, 1000) / 1000.0f) * maxDistance; // random 0 to maxDistance

        float x = cameraPos.x + cosf(angle) * dist;
        float z = cameraPos.z + sinf(angle) * dist;
        bugs[i].angle = 0.0f;
        bugs[i].pos = (Vector3){ x, 0.0f, z }; // you'll set .y later
        bugs[i].pos.y = GetTerrainHeightFromMeshXZ(chunks[closestCX][closestCY], bugs[i].pos.x, bugs[i].pos.z);
        bugs[i].pos.y = bugs[i].pos.y + GetRandomValue(1, 10);
        if(bugs[i].pos.y<-5000){bugs[i].pos.y=500;}
        bugs[i].rate = GetRandomValue(0.1f, 10.01f);
        bugs[i].origBox = box;
        bugs[i].box = UpdateBoundingBox(box,bugs[i].pos);
    }
   // return bugs;
}

void UpdateLightningBugs(LightningBug *bugs, int count, float deltaTime)
{
    for (int i = 0; i < count; i++)
    {
        // === XZ movement ===
        float speed = 0.26f; // units per second
        float randDelt = (float)((float)GetRandomValue(0, 10))/18788.0f;
        float randDeltZ = (float)((float)GetRandomValue(0, 10))/18888.0f;
        bugs[i].pos.x += cosf(bugs[i].angle) * speed * deltaTime + randDelt;
        bugs[i].pos.z += sinf(bugs[i].angle) * speed * deltaTime + randDeltZ;

        // Drift the angle slightly (wander)
        float angleWander = ((float)GetRandomValue(-50, 50) / 10000.0f) * PI; // small random
        bugs[i].angle += angleWander;

        // === Y movement ===
        float verticalSpeed = bugs[i].rate * deltaTime;
        bugs[i].pos.y += verticalSpeed;

        // Optional: bounce up/down within limits (simple floating)
        if (bugs[i].pos.y > 2.5f || bugs[i].pos.y < 0.5f) {
            bugs[i].rate *= -1.0f; // invert direction
        }
        bugs[i].box = UpdateBoundingBox(bugs[i].origBox ,bugs[i].pos);

        //new stuff
        bugs[i].timer -= deltaTime;
        if (bugs[i].timer <= 0.0f)
        {
            // 25% chance this bug blinks
            if (GetRandomValue(0, 2970000) < 23)
            {
                bugs[i].alpha = 1.0f;
            }
            bugs[i].timer = 1.0f + (GetRandomValue(0, 100) / 100.0f); // reset 1–2 sec
        }
        // fade out
        if (bugs[i].alpha > 0.0f)
        {
            bugs[i].alpha -= deltaTime * 2.0f; // fade fast
            if (bugs[i].alpha < 0.0f) bugs[i].alpha = 0.0f;
        }
    }
}

void UpdateStars(Star *stars, int count)
{
    for (int i = 0; i < count; i++)
    {
        // === XZ movement ===
        //new stuff
        stars[i].timer -= 0.0001;
        if (stars[i].timer <= 0.0f)
        {
            // 25% chance this bug blinks
            if (GetRandomValue(1, 597) < 13)
            {
                stars[i].alpha = 1.0f;
            }
            stars[i].timer = 1.0f + (GetRandomValue(0, 100000) / 100.0f); // reset 1–2 sec
        }
        // fade out
        if (stars[i].alpha > 0.0f)
        {
            stars[i].alpha -= 0.07f; // fade fast
            if (stars[i].alpha < 0.0f) stars[i].alpha = 0.0f;
        }
    }
}

//stars
bool starGenHappened = false;
Star *GenerateStars(int count)
{
    Star *stars = (Star *)malloc(sizeof(LightningBug) * count);
    if (!stars) return NULL;

    for (int i = 0; i < count; i++)
    {
        float angle = (float)GetRandomValue(0, 359) * 0.88;
        float dist = ((float)GetRandomValue(0, 1000) / 1000.0f) * 9008; // random 0 to maxDistance

        float x = cosf(angle) * dist;
        float z = sinf(angle) * dist;
        stars[i].pos = (Vector3){ x, GetRandomValue(1800, 2400), z }; // you'll set .y later
    }
    // for (int i = 0; i < STAR_COUNT; i++) //sphere
    // {
    //     float theta = GetRandomValue(0, 360) * DEG2RAD;
    //     float phi = acosf(GetRandomValue(-1000, 1000) / 1000.0f);  // uniform sphere

    //     float radius = 1000.0f; // or whatever looks good

    //     stars[i].pos.x = radius * sinf(phi) * cosf(theta);
    //     stars[i].pos.y = (200 * cosf(phi)) + 1600;
    //     stars[i].pos.z = radius * sinf(phi) * sinf(theta);

    //     // Matrix mat = MatrixTranslate(stars[i].pos.x, stars[i].pos.y, stars[i].pos.z);
    //     // mat.m12 = (float)i;  // unique instance ID
    //     // starTransforms[i] = mat;
    // }
    return stars;
}


//water is similar to tiles with a manifest
void OpenWaterObjects(Shader shader) {
    FILE *f = fopen("map/water_manifest.txt", "r"); // Open the manifest
    if (!f) {
        TraceLog(LOG_WARNING, "Failed to open water manifest");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int cx, cy, patch;
        if (sscanf(line, "%d %d %d", &cx, &cy, &patch) == 3) {
            if (cx >= 0 && cx < CHUNK_COUNT && cy >= 0 && cy < CHUNK_COUNT) {
                // Build file path
                char path[256];
                snprintf(path, sizeof(path), "map/chunk_%02d_%02d/water/patch_%d.obj", cx, cy, patch);

                Model model = LoadModel(path);
                if (model.meshCount > 0) {
                    // Allocate if needed
                    if (chunks[cx][cy].water == NULL) {
                        chunks[cx][cy].water = MemAlloc(sizeof(Model) * MAX_WATER_PATCHES_PER_CHUNK);
                        chunks[cx][cy].waterCount = 0;
                    }

                    if (chunks[cx][cy].waterCount < MAX_WATER_PATCHES_PER_CHUNK) {
                        chunks[cx][cy].water[chunks[cx][cy].waterCount] = model;
                        chunks[cx][cy].water[chunks[cx][cy].waterCount].materials[0].shader = shader;
                        chunks[cx][cy].water[chunks[cx][cy].waterCount].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 0, 100, 255, 255 }; // semi-transparent blue;
                        chunks[cx][cy].waterCount++;
                        TraceLog(LOG_INFO, "Loaded water model: %s", path);
                    } else {
                        TraceLog(LOG_WARNING, "Too many water patches in chunk %d,%d", cx, cy);
                        UnloadModel(model);
                    }
                } else {
                    TraceLog(LOG_WARNING, "Failed to load water mesh: %s", path);
                }
            }
        } else {
            TraceLog(LOG_WARNING, "Malformed line in water manifest: %s", line);
        }
    }

    fclose(f);
}

// Convert world-space position to global tile coordinates
void GetGlobalTileCoords(Vector3 pos, int *out_gx, int *out_gy) {
    float worldX = pos.x + WORLD_ORIGIN_OFFSET;
    float worldZ = pos.z + WORLD_ORIGIN_OFFSET;

    *out_gx = (int)(worldX / TILE_WORLD_SIZE);
    *out_gy = (int)(worldZ / TILE_WORLD_SIZE);
}
bool IsTreeInActiveTile(Vector3 pos, int playerChunkX, int playerChunkY, int playerTileX, int playerTileY) {
    int gx, gy;
    GetGlobalTileCoords(pos, &gx, &gy);

    int center_gx = playerChunkX * TILE_GRID_SIZE + playerTileX;
    int center_gy = playerChunkY * TILE_GRID_SIZE + playerTileY;

    return (gx >= center_gx - ACTIVE_TILE_GRID_OFFSET && gx <= center_gx + ACTIVE_TILE_GRID_OFFSET &&
            gy >= center_gy - ACTIVE_TILE_GRID_OFFSET && gy <= center_gy + ACTIVE_TILE_GRID_OFFSET);
}
bool IsTileActive(int cx, int cy, int tx, int ty, int playerChunkX, int playerChunkY, int playerTileX, int playerTileY) {
    int tile_gx = cx * TILE_GRID_SIZE + tx;
    int tile_gy = cy * TILE_GRID_SIZE + ty;

    int center_gx = playerChunkX * TILE_GRID_SIZE + playerTileX;
    int center_gy = playerChunkY * TILE_GRID_SIZE + playerTileY;

    return (tile_gx >= center_gx - ACTIVE_TILE_GRID_OFFSET && tile_gx <= center_gx + ACTIVE_TILE_GRID_OFFSET &&
            tile_gy >= center_gy - ACTIVE_TILE_GRID_OFFSET && tile_gy <= center_gy + ACTIVE_TILE_GRID_OFFSET);
}
//-------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////CUSTOM CAMERA PROJECTION//////////////////////////////////////////////////
void SetCustomCameraProjection(Camera camera, float fovY, float aspect, float nearPlane, float farPlane) {
    // Build custom projection matrix
    Matrix proj = MatrixPerspective(fovY * DEG2RAD, aspect, nearPlane, farPlane);
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlMultMatrixf(MatrixToFloat(proj));  // Apply custom projection

    // Re-apply view matrix (so things don’t disappear)
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    rlMultMatrixf(MatrixToFloat(view));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//structs
typedef struct Plane {
    Vector3 normal;
    float d;
} Plane;

typedef struct Frustum {
    Plane planes[6]; // left, right, top, bottom, near, far
} Frustum;

static Plane NormalizePlane(Plane p) {
    float len = Vector3Length(p.normal);
    return (Plane){
        .normal = Vector3Scale(p.normal, 1.0f / len),
        .d = p.d / len
    };
}

Frustum ExtractFrustum(Matrix mat)
{
    Frustum f;

    // LEFT
    f.planes[0] = NormalizePlane((Plane){
        .normal = (Vector3){ mat.m3 + mat.m0, mat.m7 + mat.m4, mat.m11 + mat.m8 },
        .d = mat.m15 + mat.m12
    });

    // RIGHT
    f.planes[1] = NormalizePlane((Plane){
        .normal = (Vector3){ mat.m3 - mat.m0, mat.m7 - mat.m4, mat.m11 - mat.m8 },
        .d = mat.m15 - mat.m12
    });

    // BOTTOM
    f.planes[2] = NormalizePlane((Plane){
        .normal = (Vector3){ mat.m3 + mat.m1, mat.m7 + mat.m5, mat.m11 + mat.m9 },
        .d = mat.m15 + mat.m13
    });

    // TOP
    f.planes[3] = NormalizePlane((Plane){
        .normal = (Vector3){ mat.m3 - mat.m1, mat.m7 - mat.m5, mat.m11 - mat.m9 },
        .d = mat.m15 - mat.m13
    });

    // NEAR
    f.planes[4] = NormalizePlane((Plane){
        .normal = (Vector3){ mat.m3 + mat.m2, mat.m7 + mat.m6, mat.m11 + mat.m10 },
        .d = mat.m15 + mat.m14
    });

    // FAR
    f.planes[5] = NormalizePlane((Plane){
        .normal = (Vector3){ mat.m3 - mat.m2, mat.m7 - mat.m6, mat.m11 - mat.m10 },
        .d = mat.m15 - mat.m14
    });

    return f;
}

bool IsBoxInFrustum(BoundingBox box, Frustum frustum)
{
    for (int i = 0; i < 6; i++)
    {
        Plane plane = frustum.planes[i];

        // Find the corner of the AABB that is most *opposite* to the normal
        Vector3 positive = {
            (plane.normal.x >= 0) ? box.max.x : box.min.x,
            (plane.normal.y >= 0) ? box.max.y : box.min.y,
            (plane.normal.z >= 0) ? box.max.z : box.min.z
        };

        // If that corner is outside, the box is not visible
        float distance = Vector3DotProduct(plane.normal, positive) + plane.d;
        if (distance < 0){return false;}
    }

    return true;
}

void TakeScreenshotWithTimestamp(void) {
    // Get timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[128];
    strftime(filename, sizeof(filename), "screenshot/screenshot_%Y%m%d_%H%M%S.png", t);

    // Save the screenshot
    TakeScreenshot(filename);
    TraceLog(LOG_INFO, "Saved screenshot: %s", filename);
}

BoundingBox ScaleBoundingBox(BoundingBox box, Vector3 scale)
{
    // Compute the center of the box
    Vector3 center = {
        (box.min.x + box.max.x) / 2.0f,
        (box.min.y + box.max.y) / 2.0f,
        (box.min.z + box.max.z) / 2.0f
    };

    // Compute half-size (extent) of the box
    Vector3 halfSize = {
        (box.max.x - box.min.x) / 2.0f,
        (box.max.y - box.min.y) / 2.0f,
        (box.max.z - box.min.z) / 2.0f
    };

    // Apply scaling to the half-size
    halfSize.x *= scale.x;
    halfSize.y *= scale.y;
    halfSize.z *= scale.z;

    // Create new box
    BoundingBox scaledBox = {
        .min = {
            center.x - halfSize.x,
            center.y - halfSize.y,
            center.z - halfSize.z
        },
        .max = {
            center.x + halfSize.x,
            center.y + halfSize.y,
            center.z + halfSize.z
        }
    };

    return scaledBox;
}

Color skyboxTint = (Color){255,255,255,100};
void DrawSkyboxPanelFixed(Model model, Vector3 position, float angleDeg, Vector3 axis, float size)
{
    Vector3 scale = (Vector3){ size, size, 1.0f };
    DrawModelEx(model, position, axis, angleDeg, scale, skyboxTint);
}

void FindClosestChunkAndAssignLod(Camera3D *camera) 
{
    bool foundChunkWithBox = false;
    if(!foundChunkWithBox)//compute it directly
    {
        int half = CHUNK_COUNT / 2;
        int chunkX = (int)floor(camera->position.x / CHUNK_WORLD_SIZE) + half;
        int chunkY = (int)floor(camera->position.z / CHUNK_WORLD_SIZE) + half;
        //TraceLog(LOG_INFO, "!foundChunkWithBox => (%f,%f)=>[%d,%d]",camera->position.x,camera->position.z, chunkX, chunkY);
        if (chunkX >= 0 && chunkX < CHUNK_COUNT &&
            chunkY >= 0 && chunkY < CHUNK_COUNT &&
            chunks[chunkX][chunkY].isLoaded)
        {
            closestCX = chunkX;
            closestCY = chunkY;
        }
        else if(onLoad)
        {
            TraceLog(LOG_WARNING, "Camera is outside of valid chunk bounds");
        }
    }
    //TraceLog(LOG_INFO, "FindClosestChunkAndAssignLod (2): (%d x %d)", closestCX, closestCY);
    // --- Second pass: assign LODs ---
    for (int cy = 0; cy < CHUNK_COUNT; cy++) {
        for (int cx = 0; cx < CHUNK_COUNT; cx++) {
            int dx = abs(cx - closestCX);
            int dy = abs(cy - closestCY);

            if (cx == closestCX && cy == closestCY) {
                chunks[cx][cy].lod = LOD_64;  // Highest LOD
            }
            else if (dx <= 1 && dy <= 1) {
                chunks[cx][cy].lod = LOD_64;
            }
            else if (dx <= 2 && dy <= 2) {
                chunks[cx][cy].lod = LOD_32;
            }
            else if (dx <= 3 && dy <=3) {
                chunks[cx][cy].lod = LOD_16;
            }
            else {
                chunks[cx][cy].lod = LOD_8;
            }
        }
    }
}

bool FindNextTreeInChunk(Camera3D *camera, int cx, int cy, float minDistance, Model_Type type) {
    if (!chunks[cx][cy].props || chunks[cx][cy].props<=0) {
        TraceLog(LOG_INFO, "No props data loaded for chunk_%02d_%02d -> (%d)", cx, cy, chunks[cx][cy].treeCount);
        return false;
    }

    int count = chunks[cx][cy].treeCount;
    if (count <= 0) return false;
    if (count <= chunks[cx][cy].curTreeIdx){chunks[cx][cy].curTreeIdx = 0;}//if for whatever reason our index is too large, reset
    float minDistSqr = minDistance * minDistance;
    Vector3 camPos = camera->position; //readonly

    for (int i = chunks[cx][cy].curTreeIdx; i < count; i++) {
        Vector3 t = chunks[cx][cy].props[i].pos;
        float dx = t.x - camPos.x;
        float dy = t.y - camPos.y;
        float dz = t.z - camPos.z;
        float distSqr = dx * dx + dy * dy + dz * dz;

        if (distSqr >= minDistSqr && chunks[cx][cy].props[i].type==type) {
            camera->position = (Vector3){t.x + 0.987, t.y+PLAYER_HEIGHT, t.z + 1.1};  // Set cam above tree
            FindClosestChunkAndAssignLod(camera);
            camera->target = (Vector3){0, 0, t.z};           // Look at the tree but not really
            chunks[cx][cy].curTreeIdx = i;
            return true;
        }
    }
    TraceLog(LOG_INFO, "No suitable trees found in this chunk.");
    return false;  // No suitable tree found
}

bool FindAnyTreeInWorld(Camera *camera, float radius, Model_Type type) {
    int attempts = 0;
    const int maxAttempts = MAX_CHUNK_DIM * MAX_CHUNK_DIM;

    while (attempts < maxAttempts) {
        int cx = rand() % MAX_CHUNK_DIM;
        int cy = rand() % MAX_CHUNK_DIM;
        tree_elf++;

        if (FindNextTreeInChunk(camera, cx, cy, radius, type)) {
            TraceLog(LOG_INFO, "Found tree in chunk_%02d_%02d", cx, cy);
            return true;
        }

        attempts++;
    }

    TraceLog(LOG_WARNING, "No suitable trees found in any chunk."); //this one is a warning because this would mean no trees anywhere
    return false;
}

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

Image LoadSafeImage(const char *filename) {
    Image img = LoadImage(filename);
    // if (img.width != 64 || img.height != 64) {
    //     TraceLog(LOG_WARNING, "Image %s is not 64x64: (%d x %d)", filename, img.width, img.height);
    // }

    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ImageDataFlipVertical(&img);
    // //ImageDataFlipHorizontal(&img);
    // Texture2D tex = LoadTextureFromImage(img);
    // UnloadImage(img);

    // if (tex.id == 0) {
    //     TraceLog(LOG_WARNING, "Failed to create texture from image: %s", filename);
    // }
    // return tex;
    return img;
}

void PreLoadTexture(int cx, int cy)
{
    //char colorPath[256];
    //char colorBigPath[256];
    //char slopePath[256];
    //char slopeBigPath[256];
    char avgPath[64];
    char avgBigPath[64];
    char avgFullPath[64];
    char avgDamnPath[64];
    //snprintf(colorPath, sizeof(colorPath), "map/chunk_%02d_%02d/color.png", cx, cy);
    //snprintf(colorBigPath, sizeof(colorBigPath), "map/chunk_%02d_%02d/color_big.png", cx, cy);
    //snprintf(slopePath, sizeof(slopePath), "map/chunk_%02d_%02d/slope.png", cx, cy);
    //snprintf(slopeBigPath, sizeof(slopeBigPath), "map/chunk_%02d_%02d/slope_big.png", cx, cy);
    snprintf(avgPath, sizeof(avgPath), "map/chunk_%02d_%02d/avg.png", cx, cy);
    snprintf(avgBigPath, sizeof(avgBigPath), "map/chunk_%02d_%02d/avg_big.png", cx, cy);
    snprintf(avgFullPath, sizeof(avgFullPath), "map/chunk_%02d_%02d/avg_full.png", cx, cy);
    snprintf(avgDamnPath, sizeof(avgDamnPath), "map/chunk_%02d_%02d/avg_damn.png", cx, cy);
    // --- Load images and assign to model material ---
    TraceLog(LOG_INFO, "Loading image in worker thread: %s", avgPath);
    Image img = LoadSafeImage(avgPath); //using slope and color avg right now
    Image imgBig = LoadSafeImage(avgBigPath);
    Image imgFull = LoadSafeImage(avgFullPath);
    Image imgDamn = LoadSafeImage(avgDamnPath);
    chunks[cx][cy].img_tex = img;
    chunks[cx][cy].img_tex_big = imgBig;
    chunks[cx][cy].img_tex_full = imgFull;
    chunks[cx][cy].img_tex_damn = imgDamn;
    chunks[cx][cy].isTextureReady = true;
}

void LoadTreePositions(int cx, int cy)
{
    char treePath[64];
    snprintf(treePath, sizeof(treePath), "map/chunk_%02d_%02d/trees.txt", cx, cy);

    FILE *fp = fopen(treePath, "r");
    if (!fp) {
        TraceLog(LOG_WARNING, "No tree file for chunk (%d,%d)", cx, cy);
        return;
    }

    int treeCount = 0;
    fscanf(fp, "%d\n", &treeCount);
    if (treeCount <= 0) {
        fclose(fp);
        return;
    }

    //Vector3 *treePositions = (Vector3 *)malloc(sizeof(Vector3) * (treeCount + 1));
    StaticGameObject *treePositions = malloc(sizeof(StaticGameObject) * (MAX_PROPS_UPPER_BOUND));//some buffer for these, should never be above 512
    for (int i = 0; i < treeCount; i++) {
        float x, y, z;
        int type;
        fscanf(fp, "%f %f %f %d\n", &x, &y, &z, &type);
        treePositions[i] = (StaticGameObject){type, (Vector3){ x, y, z }};
    }

    fclose(fp);

    chunks[cx][cy].props = treePositions;
    chunks[cx][cy].treeCount = treeCount;
    TraceLog(LOG_INFO, "Loaded %d trees for chunk (%d,%d)", treeCount, cx, cy);
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

    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].textureDamn;
    model32.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].textureFull;
    model16.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].textureBig;
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
    pthread_mutex_lock(&mutex);
    chunks[cx][cy].model = model;
    chunks[cx][cy].model32 = model32;
    chunks[cx][cy].model16 = model16;
    chunks[cx][cy].model8 = model8;
    chunks[cx][cy].position = position;
    chunks[cx][cy].center = center;
    chunks[cx][cy].isReady = true;
    chunks[cx][cy].lod = LOD_8;
    //mark the chunk with identifiers
    chunks[cx][cy].cx = cx;
    chunks[cx][cy].cy = cy;
    chunks[cx][cy].id = (cx*CHUNK_COUNT) + cy;
    //load trees
    LoadTreePositions(cx, cy);
    pthread_mutex_unlock(&mutex);
    //report
    TraceLog(LOG_INFO, "Chunk [%02d, %02d] loaded at position (%.1f, %.1f, %.1f)", 
             cx, cy, position.x, position.y, position.z);
}


void *ChunkLoaderThread(void *arg) {
    bool haveManifest = false;
    FILE *f = fopen("map/manifest.txt", "r"); // Open for append
    if (f != NULL) {
        haveManifest = true;
        //need to count the lines in the file and then set manifestTileCount
        int lines = 0;
        int c;
        while ((c = fgetc(f)) != EOF) {
            if (c == '\n') lines++;
        }
        manifestTileCount = lines;
        fclose(f);
        OpenTiles();
    }
    for (int cy = 0; cy < CHUNK_COUNT; cy++) {
        for (int cx = 0; cx < CHUNK_COUNT; cx++) {
            if(!haveManifest)
            {
                manifestTileCount = 2048; //fall back for the load bar, we dont know so guess and hope its close
                DocumentTiles(cx,cy);
            }
            if (!chunks[cx][cy].isLoaded) {
                PreLoadTexture(cx, cy);
                LoadChunk(cx, cy);
            }
        }
    }
    wasTilesDocumented = true;
    return NULL;
}

void StartChunkLoader() {
    pthread_t thread;
    pthread_create(&thread, NULL, ChunkLoaderThread, NULL);
    pthread_detach(thread);
}

int main(void) {
    bool displayBoxes = false;
    bool displayLod = false;
    LightningBug *bugs;
    Star *stars;
    //----------------------init chunks---------------------
    chunks = malloc(sizeof(Chunk *) * CHUNK_COUNT);
    for (int i = 0; i < CHUNK_COUNT; i++) chunks[i] = calloc(CHUNK_COUNT, sizeof(Chunk));
    if (!chunks) {
        TraceLog(LOG_ERROR, "Failed to allocate chunk row pointers");
        exit(1);
    }

    for (int x = 0; x < CHUNK_COUNT; x++) {
        chunks[x] = malloc(sizeof(Chunk) * CHUNK_COUNT);
        if (!chunks[x]) {
            TraceLog(LOG_ERROR, "Failed to allocate chunk column %d", x);
            exit(1); // or clean up and fail gracefully
        }

        // Optional: clear/init each chunk
        for (int y = 0; y < CHUNK_COUNT; y++) {
            memset(&chunks[x][y], 0, sizeof(Chunk));
            chunks[x][y].water = NULL;chunks[x][y].waterCount = 0;//make sure water is ready to be checked and then instantiated
        }
    }
    //----------------------DONE -> init chunks---------------------
    //-----INIT TILES
    int maxTiles = ((CHUNK_COUNT * CHUNK_COUNT) * (TILE_GRID_SIZE * TILE_GRID_SIZE));  // = 16,384
    foundTiles = malloc(sizeof(TileEntry) * maxTiles);
    foundTileCount = 0;

    if (!foundTiles) {
        TraceLog(LOG_ERROR, "Out of memory allocating tile entry buffer");
        return -666;
    }
    //---------------RAYLIB INIT STUFF---------------------------------------
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Map Preview with Trees & Grass");
    InitAudioDevice();
    DisableCursor();
    SetTargetFPS(60);

    //shaders  
        // - 
    // Shader heightShader = LoadShader("shaders/120/height_color.vs", "shaders/120/height_color.fs");
    // int mvpLoc = GetShaderLocation(heightShader, "mvp");
    // float strength = 0.25f;
    // SetShaderValue(heightShader, GetShaderLocation(heightShader, "slopeStrength"), &strength, SHADER_UNIFORM_FLOAT);
        // - 
    Shader heightShaderLight = LoadShader("shaders/120/height_color_lighting.vs", "shaders/120/height_color_lighting.fs");
    int mvpLocLight = GetShaderLocation(heightShaderLight, "mvp");
    int modelLocLight = GetShaderLocation(heightShaderLight, "model");
    float strengthLight = 0.25f;
    SetShaderValue(heightShaderLight, GetShaderLocation(heightShaderLight, "slopeStrength"), &strengthLight, SHADER_UNIFORM_FLOAT);
    // Set standard locations
    heightShaderLight.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(heightShaderLight, "mvp");
    heightShaderLight.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(heightShaderLight, "model");
    // Set light direction manually
    Vector3 lightDir = (Vector3){ -10.2f, -100.0f, -10.3f };
    int lightDirLoc = GetShaderLocation(heightShaderLight, "lightDir");
    SetShaderValue(heightShaderLight, lightDirLoc, &lightDir, SHADER_UNIFORM_VEC3);
        // - 
    Shader waterShader = LoadShader("shaders/120/water.vs", "shaders/120/water.fs");
    int timeLoc = GetShaderLocation(waterShader, "uTime");
    int offsetLoc = GetShaderLocation(waterShader, "worldOffset");
    //tree model
    Model treeCubeModel, treeModel, bgTreeModel, rockModel;
    Texture bgTreeTexture, rockTexture;
    char treePath[64];
    char bgTreePath[64];
    char bgTreeTexturePath[64];
    char rockPath[64];
    char rockTexturePath[64];
    snprintf(treePath, sizeof(treePath), "models/tree.glb");
    snprintf(bgTreePath, sizeof(bgTreePath), "models/tree_bg.glb");
    snprintf(bgTreeTexturePath, sizeof(bgTreeTexturePath), "textures/tree_skin2.png");
    snprintf(rockPath, sizeof(rockPath), "models/rock1.glb");
    snprintf(rockTexturePath, sizeof(rockTexturePath), "textures/rock1.png");
    //trees
    treeModel = LoadModel(treePath);
    bgTreeModel = LoadModel(bgTreePath);
    treeCubeModel = LoadModelFromMesh(GenMeshCube(0.67f, 16.0f, 0.67f));
    treeCubeModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = DARKGREEN;
    BoundingBox treeOrigBox = GetModelBoundingBox(treeCubeModel);
    bgTreeTexture = LoadTexture(bgTreeTexturePath);//for cookies (todo: try the small one)
    //rocks
    rockModel = LoadModel(rockPath);
    rockTexture = LoadTexture(rockTexturePath);//for rocks
    rockModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = rockTexture;
    //game map
    Texture2D mapTexture;
    bool showMap = true;
    float mapZoom = 1.0f;
    Rectangle mapViewport = { SCREEN_WIDTH - GAME_MAP_SIZE - 10, 10, 128, 128 };  // Map position + size
    mapTexture = LoadTexture("map/elevation_color_map.png");
    //gpu instancing section
    // Load lighting shader---------------------------------------------------------------------------------------
    Shader instancingLightShader = LoadShader("shaders/100/lighting_instancing.vs","shaders/100/lighting.fs");
    // Get shader locations
    instancingLightShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(instancingLightShader, "mvp");
    instancingLightShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(instancingLightShader, "viewPos");
    // Set shader value: ambient light level
    int ambientLoc = GetShaderLocation(instancingLightShader, "ambient");
    SetShaderValue(instancingLightShader, ambientLoc, (float[4]){ 0.2f, 0.2f, 0.2f, 1.0f }, SHADER_UNIFORM_VEC4);
    int lightPositionLoc = GetShaderLocation(instancingLightShader, "ambient");
    SetShaderValue(instancingLightShader, ambientLoc, (float[4]){ 0.2f, 0.2f, 0.2f, 1.0f }, SHADER_UNIFORM_VEC4);
    int lightColorLoc = GetShaderLocation(instancingLightShader, "ambient");
    SetShaderValue(instancingLightShader, ambientLoc, (float[4]){ 0.2f, 0.2f, 0.2f, 1.0f }, SHADER_UNIFORM_VEC4);
    // int ambientLoc = GetShaderLocation(instancingLightShader, "ambient");
    // SetShaderValue(instancingLightShader, ambientLoc, (float[4]){ 0.2f, 0.2f, 0.2f, 1.0f }, SHADER_UNIFORM_VEC4);
    // Create one light
    Light instanceLight = CreateLight(LIGHT_DIRECTIONAL, LightPosDraw, LightTargetDraw, lightColorDraw, instancingLightShader);
    //init the static game props stuff
    InitStaticGameProps(instancingLightShader);//get the high fi models ready
    //END -- lighting shader---------------------------------------------------------------------------------------
    //START -- lightning bug shader :)---------------------------------------------------------------------------------------
    // Load PBR shader and setup all required locations
    Shader lightningBugShader = LoadShader(TextFormat("shaders/100/lighting_instancing_bug.vs"),
                               TextFormat("shaders/100/lighting_bug.fs"));
    lightningBugShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(lightningBugShader, "mvp");
    lightningBugShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lightningBugShader, "viewPos");
    int ambientBugLoc = GetShaderLocation(lightningBugShader, "ambient");
    SetShaderValue(lightningBugShader, ambientLoc, (float[4]){ 0.2f, 0.2f, 0.2f, 1.0f }, SHADER_UNIFORM_VEC4);
    int lightPositionBugLoc = GetShaderLocation(lightningBugShader, "ambient");
    SetShaderValue(lightningBugShader, ambientLoc, (float[4]){ 0.2f, 0.2f, 0.2f, 1.0f }, SHADER_UNIFORM_VEC4);
    int lightColorBugLoc = GetShaderLocation(lightningBugShader, "ambient");
    SetShaderValue(lightningBugShader, ambientLoc, (float[4]){ 0.2f, 0.2f, 0.2f, 1.0f }, SHADER_UNIFORM_VEC4);
    // lightningBugShader.locs[SHADER_LOC_MAP_ALBEDO] = GetShaderLocation(lightningBugShader, "albedoMap");
    // // WARNING: Metalness, roughness, and ambient occlusion are all packed into a MRA texture
    // // They are passed as to the SHADER_LOC_MAP_METALNESS location for convenience,
    // // shader already takes care of it accordingly
    // lightningBugShader.locs[SHADER_LOC_MAP_METALNESS] = GetShaderLocation(lightningBugShader, "mraMap");
    // lightningBugShader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(lightningBugShader, "normalMap");
    // // WARNING: Similar to the MRA map, the emissive map packs different information 
    // // into a single texture: it stores height and emission data
    // // It is binded to SHADER_LOC_MAP_EMISSION location an properly processed on shader
    // lightningBugShader.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation(lightningBugShader, "emissiveMap");
    // lightningBugShader.locs[SHADER_LOC_COLOR_DIFFUSE] = GetShaderLocation(lightningBugShader, "albedoColor");

    // // Setup additional required shader locations, including lights data
    // lightningBugShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lightningBugShader, "viewPos");
    // int lightCountLoc = GetShaderLocation(lightningBugShader, "numOfLights");
    // int maxLightCount = MAX_LIGHTS;
    // SetShaderValue(lightningBugShader, lightCountLoc, &maxLightCount, SHADER_UNIFORM_INT);

    // // Setup ambient color and intensity parameters
    // float ambientIntensity = 0.02f;
    // Color ambientColor = (Color){ 26, 32, 135, 255 };
    // Vector3 ambientColorNormalized = (Vector3){ ambientColor.r/255.0f, ambientColor.g/255.0f, ambientColor.b/255.0f };
    // SetShaderValue(lightningBugShader, GetShaderLocation(lightningBugShader, "ambientColor"), &ambientColorNormalized, SHADER_UNIFORM_VEC3);
    // SetShaderValue(lightningBugShader, GetShaderLocation(lightningBugShader, "ambient"), &ambientIntensity, SHADER_UNIFORM_FLOAT);

    // // Get location for shader parameters that can be modified in real time
    // int metallicValueLoc = GetShaderLocation(lightningBugShader, "metallicValue");
    // int roughnessValueLoc = GetShaderLocation(lightningBugShader, "roughnessValue");
    // int emissiveIntensityLoc = GetShaderLocation(lightningBugShader, "emissivePower");
    // int emissiveColorLoc = GetShaderLocation(lightningBugShader, "emissiveColor");
    // int textureTilingLoc = GetShaderLocation(lightningBugShader, "tiling");
    Light lights[MAX_LIGHTS] = { 0 };
    lights[0] = CreateLight(LIGHT_POINT, (Vector3){ -1.0f, 1.0f, -2.0f }, (Vector3){ 0.0f, 0.0f, 0.0f }, YELLOW, lightningBugShader);
    lights[1] = CreateLight(LIGHT_POINT, (Vector3){ 2.0f, 1.0f, 1.0f }, (Vector3){ 0.0f, 0.0f, 0.0f }, GREEN,lightningBugShader);
    lights[2] = CreateLight(LIGHT_POINT, (Vector3){ -2.0f, 1.0f, 1.0f }, (Vector3){ 0.0f, 0.0f, 0.0f }, RED,lightningBugShader);
    lights[3] = CreateLight(LIGHT_POINT, (Vector3){ 1.0f, 1.0f, -2.0f }, (Vector3){ 0.0f, 0.0f, 0.0f }, BLUE, lightningBugShader);
    
    // Setup material texture maps usage in shader
    // NOTE: By default, the texture maps are always used
    int usage = 1;
    SetShaderValue(lightningBugShader, GetShaderLocation(lightningBugShader, "useTexAlbedo"), &usage, SHADER_UNIFORM_INT);
    SetShaderValue(lightningBugShader, GetShaderLocation(lightningBugShader, "useTexNormal"), &usage, SHADER_UNIFORM_INT);
    SetShaderValue(lightningBugShader, GetShaderLocation(lightningBugShader, "useTexMRA"), &usage, SHADER_UNIFORM_INT);
    SetShaderValue(lightningBugShader, GetShaderLocation(lightningBugShader, "useTexEmissive"), &usage, SHADER_UNIFORM_INT);
    //--stars
    Shader starShader = LoadShader(TextFormat("shaders/100/lighting_star.vs"),
                               TextFormat("shaders/100/lighting_star.fs"));
    starShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(starShader, "mvp");
    starShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(starShader, "viewPos");
    int ambientStarLoc = GetShaderLocation(starShader, "ambient");
    SetShaderValue(starShader, ambientStarLoc, (float[4]){ 0.2f, 0.2f, 0.2f, 1.0f }, SHADER_UNIFORM_VEC4);
    int lightPositionStarLoc = GetShaderLocation(starShader, "position");
    SetShaderValue(starShader, lightPositionStarLoc, (float[4]){ 0.2f, 0.2f, 0.2f, 1.0f }, SHADER_UNIFORM_VEC4);
    int lightColorStarLoc = GetShaderLocation(starShader, "color");
    SetShaderValue(starShader, lightColorStarLoc, (float[4]){ 0.2f, 0.2f, 0.2f, 1.0f }, SHADER_UNIFORM_VEC4);
    Light starLights[MAX_LIGHTS] = { 0 };
    starLights[0] = CreateLight(LIGHT_POINT, (Vector3){ -1.0f, 1.0f, -2.0f }, (Vector3){ 0.0f, 0.0f, 0.0f }, YELLOW, starShader);
    starLights[1] = CreateLight(LIGHT_POINT, (Vector3){ 2.0f, 1.0f, 1.0f }, (Vector3){ 0.0f, 0.0f, 0.0f }, GREEN,starShader);
    starLights[2] = CreateLight(LIGHT_POINT, (Vector3){ -2.0f, 1.0f, 1.0f }, (Vector3){ 0.0f, 0.0f, 0.0f }, RED,starShader);
    starLights[3] = CreateLight(LIGHT_POINT, (Vector3){ 1.0f, 1.0f, -2.0f }, (Vector3){ 0.0f, 0.0f, 0.0f }, BLUE, starShader);
    usage = 1;
    SetShaderValue(starShader, GetShaderLocation(starShader, "useTexAlbedo"), &usage, SHADER_UNIFORM_INT);
    SetShaderValue(starShader, GetShaderLocation(starShader, "useTexNormal"), &usage, SHADER_UNIFORM_INT);
    SetShaderValue(starShader, GetShaderLocation(starShader, "useTexMRA"), &usage, SHADER_UNIFORM_INT);
    SetShaderValue(starShader, GetShaderLocation(starShader, "useTexEmissive"), &usage, SHADER_UNIFORM_INT);
        //more lb stuff
    Mesh sphereMesh = GenMeshHemiSphere(0.108f,8, 8);
    Material sphereMaterial = LoadMaterialDefault();
    sphereMaterial.maps[MATERIAL_MAP_DIFFUSE].color = (Color){50,200,100,200};
    sphereMaterial.shader = lightningBugShader;
    Mesh sphereStarMesh = GenMeshHemiSphere(6.2f,3, 3);
    Material sphereStarMaterial = LoadMaterialDefault();
    sphereStarMaterial.maps[MATERIAL_MAP_DIFFUSE].color = (Color){80,80,150,230};
    sphereStarMaterial.shader = starShader;
    Vector4 starColorVecs[4];
    for (int i = 0; i < 4; i++)
    {
        starColorVecs[i] = (Vector4) {
            starColors[i].r / 255.0f,
            starColors[i].g / 255.0f,
            starColors[i].b / 255.0f,
            1.0f
        };
    }
    float instanceIDs[STAR_COUNT];
    for (int i = 0; i < STAR_COUNT; i++) {
        instanceIDs[i] = (float)i;
    }
    int idAttribLoc = GetShaderLocationAttrib(starShader, "instanceID");
    SetShaderValueV(starShader, idAttribLoc, instanceIDs, SHADER_ATTRIB_FLOAT, STAR_COUNT);

    //END -- lighting bug shader---------AND STARS!------------------------------------------------------------------------------
    //skybox stuff
    skyboxPanelMesh = GenMeshCube(1.0f, 1.0f, 0.01f); // very flat panel
    skyboxPanelFrontModel = LoadModelFromMesh(skyboxPanelMesh);
    skyboxPanelBackModel = LoadModelFromMesh(skyboxPanelMesh);
    skyboxPanelLeftModel = LoadModelFromMesh(skyboxPanelMesh);
    skyboxPanelRightModel = LoadModelFromMesh(skyboxPanelMesh);
    skyboxPanelUpModel = LoadModelFromMesh(skyboxPanelMesh);
    Texture2D skyTexFront, skyTexBack, skyTexLeft, skyTexRight, skyTexUp;
    skyTexFront = LoadTexture("skybox/sky_front_smooth.png");
    skyTexBack  = LoadTexture("skybox/sky_back_smooth.png");
    skyTexLeft  = LoadTexture("skybox/sky_left_smooth.png");
    skyTexRight = LoadTexture("skybox/sky_right_smooth.png");
    skyTexUp    = LoadTexture("skybox/sky_up_smooth.png");
    skyboxPanelFrontModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = skyTexFront;
    skyboxPanelBackModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = skyTexBack;
    skyboxPanelLeftModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = skyTexLeft;
    skyboxPanelRightModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = skyTexRight;
    skyboxPanelUpModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = skyTexUp;
    // for (int cy = 0; cy < CHUNK_COUNT; cy++) {
    //     for (int cx = 0; cx < CHUNK_COUNT; cx++) {
    //         if (!chunks[cx][cy].isLoaded && !chunks[cx][cy].isReady) {
    //             //PreLoadTexture(cx, cy);
    //             //LoadChunk(cx, cy);
    //         }
    //     }
    // }
    
    //lets get the water
    FILE *f = fopen("map/water_manifest.txt", "r"); // Open for append
    if (f != NULL) {
        //need to count the lines in the file and then set manifestTileCount
        int lines = 0;
        int c;
        while ((c = fgetc(f)) != EOF) {
            if (c == '\n') lines++;
        }
        waterManifestCount = lines;
        fclose(f);
        OpenWaterObjects(waterShader);//water manifest is required right now
    }
    //TODO: loop through each chunk, then each water feature for that chunk, set the sahder of the model
    //launch the initial loading background threads
    StartChunkLoader();

    Camera3D camera = {
        .position = (Vector3){ 0.0f, 2000.0f, 0.0f },  // Higher if needed,
        .target = (Vector3){ 0.0f, 0.0f, 0.0f },  // Centered
        .up = (Vector3){ 0.0f, 1.0f, 0.0f },
        .fovy = 80.0f,
        .projection = CAMERA_PERSPECTIVE
    };
    Camera skyCam = camera;
    skyCam.position = (Vector3){ 0, 0, 0 };
    skyCam.target = (Vector3){ 0, 0, 1 };  // looking forward
    skyCam.up = (Vector3){0, 1, 0};
    skyCam.fovy = 60.0f;
    skyCam.projection = CAMERA_PERSPECTIVE;

    float pitch = 0.0f;
    float yaw = 0.0f;  // Face toward -Z (the terrain is likely laid out in +X, +Z space)

    while (!WindowShouldClose()) {
        float time = GetTime();
        SetShaderValue(waterShader, timeLoc, &time, SHADER_UNIFORM_FLOAT);
        bool reportOn = false;
        int tileTriCount = 0;
        int tileBcCount = 0;
        int treeTriCount = 0;
        int treeBcCount = 0;
        int chunkTriCount = 0;
        int chunkBcCount = 0;
        int totalTriCount = 0;
        int totalBcCount = 0;
        float dt = GetFrameTime();
        //idk, for pbr;
        float cameraPosVecF[3] = {camera.position.x, camera.position.y, camera.position.z};
        SetShaderValue(lightningBugShader, lightningBugShader.locs[SHADER_LOC_VECTOR_VIEW], cameraPosVecF, SHADER_UNIFORM_VEC3);
        SetShaderValue(starShader, starShader.locs[SHADER_LOC_VECTOR_VIEW], cameraPosVecF, SHADER_UNIFORM_VEC3);
        //main thread of the file management system, needed for GPU operations
        if(wasTilesDocumented)
        {
            int gx, gy;
            float time = GetTime(); // or your own time tracker
            SetShaderValue(starShader, GetShaderLocation(starShader, "u_time"), &time, SHADER_UNIFORM_FLOAT);
            GetGlobalTileCoords(camera.position, &gx, &gy);
            int playerTileX  = gx % TILE_GRID_SIZE;
            int playerTileY  = gy % TILE_GRID_SIZE;
            for (int te = 0; te < foundTileCount; te++)
            {
                // bool defNeeded = foundTiles[te].cx==closestCX && foundTiles[te].cy==closestCY;
                // int dx = abs(playerTileX - foundTiles[te].tx);
                // int dy = abs(playerTileY - foundTiles[te].ty);
                // bool withinRange = dx <= TILE_GPU_UPLOAD_GRID_DIST && dy <= TILE_GPU_UPLOAD_GRID_DIST;
                // bool maybeNeeded = (chunks[foundTiles[te].cx][foundTiles[te].cy].lod == LOD_64) && (withinRange || defNeeded);
                bool maybeNeeded = (chunks[foundTiles[te].cx][foundTiles[te].cy].lod == LOD_64); //todo: testing this to see if it is my issue
                //if you find this direct method has too many tiles with too much stuff, then go back to the other version commented out above
                //that version will really cut down VRAM footprint,but medium-distant objects might start to disappear and then appear again as you get closer and closer
                if(foundTiles[te].isReady && !foundTiles[te].isLoaded && maybeNeeded)
                {
                    TraceLog(LOG_INFO, "loading tiles: %d", te);
                    pthread_mutex_lock(&mutex);
                    // Upload meshes to GPU
                    UploadMesh(&foundTiles[te].model.meshes[0], false);
                    
                    // Load GPU models
                    foundTiles[te].model = LoadModelFromMesh(foundTiles[te].model.meshes[0]);
                    foundTiles[te].box = GetModelBoundingBox(foundTiles[te].model);
                    // Apply textures
                    foundTiles[te].model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = foundTiles[te].type==MODEL_TREE?bgTreeTexture:rockTexture;
                    //mark work done
                    foundTiles[te].isLoaded = true;
                    //and now its safe to unlock
                    pthread_mutex_unlock(&mutex);
                }
                else if(foundTiles[te].isLoaded && !maybeNeeded)
                {
                    pthread_mutex_lock(&mutex);
                    UnloadMeshGPU(&foundTiles[te].model.meshes[0]);
                    foundTiles[te].isLoaded = false;
                    pthread_mutex_unlock(&mutex);
                }
            }
        }
        for (int cy = 0; cy < CHUNK_COUNT; cy++) {
            for (int cx = 0; cx < CHUNK_COUNT; cx++) {
                if(chunks[cx][cy].isTextureReady && !chunks[cx][cy].isTextureLoaded)
                {
                    pthread_mutex_lock(&mutex);
                    TraceLog(LOG_INFO, "loading chunk textures: %d,%d", cx, cy);
                    Texture2D texture = LoadTextureFromImage(chunks[cx][cy].img_tex); //using slope and color avg right now
                    Texture2D textureBig = LoadTextureFromImage(chunks[cx][cy].img_tex_big);
                    Texture2D textureFull = LoadTextureFromImage(chunks[cx][cy].img_tex_full);
                    Texture2D textureDamn = LoadTextureFromImage(chunks[cx][cy].img_tex_damn);
                    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
                    SetTextureWrap(textureBig, TEXTURE_WRAP_CLAMP);
                    SetTextureWrap(textureFull, TEXTURE_WRAP_CLAMP);
                    SetTextureWrap(textureDamn, TEXTURE_WRAP_CLAMP);
                    GenTextureMipmaps(&textureFull);  // <-- this generates mipmaps
                    SetTextureFilter(textureFull, TEXTURE_FILTER_TRILINEAR); // use a better filter
                    GenTextureMipmaps(&textureBig);  // <-- this generates mipmaps
                    SetTextureFilter(textureBig, TEXTURE_FILTER_TRILINEAR); // use a better filter
                    GenTextureMipmaps(&texture);  // <-- this generates mipmaps
                    SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR); // use a better filter
                    GenTextureMipmaps(&textureDamn);  // <-- this generates mipmaps
                    SetTextureFilter(textureDamn, TEXTURE_FILTER_TRILINEAR); // use a better filter
                    chunks[cx][cy].texture = texture;  // Copy contents
                    chunks[cx][cy].textureBig = textureBig;
                    chunks[cx][cy].textureFull = textureFull;
                    chunks[cx][cy].textureDamn = textureDamn;
                    chunks[cx][cy].isTextureLoaded = true;
                    pthread_mutex_unlock(&mutex);
                }
                else if (chunks[cx][cy].isTextureLoaded && chunks[cx][cy].isReady && !chunks[cx][cy].isLoaded) {
                    pthread_mutex_lock(&mutex);
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

                    //apply transform to vertices based on world position -- and of course it does not work because we are using a custom shader now
                    // chunks[cx][cy].model.transform = MatrixTranslate(chunks[cx][cy].position.x, chunks[cx][cy].position.y, chunks[cx][cy].position.z);
                    // chunks[cx][cy].model32.transform = MatrixTranslate(chunks[cx][cy].position.x, chunks[cx][cy].position.y, chunks[cx][cy].position.z);
                    // chunks[cx][cy].model16.transform = MatrixTranslate(chunks[cx][cy].position.x, chunks[cx][cy].position.y, chunks[cx][cy].position.z);
                    // chunks[cx][cy].model.transform = MatrixTranslate(chunks[cx][cy].position.x, chunks[cx][cy].position.y, chunks[cx][cy].position.z);
                    //apply shader to 64 chunk
                    chunks[cx][cy].model.materials[0].shader = heightShaderLight;
                    chunks[cx][cy].model32.materials[0].shader = heightShaderLight;//only do this for reltively close things, not 8 and 16
                    // Apply textures
                    chunks[cx][cy].model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].textureDamn;
                    chunks[cx][cy].model32.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].textureFull;
                    chunks[cx][cy].model16.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].textureBig;
                    chunks[cx][cy].model8.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].texture;

                    // Setup bounding box
                    chunks[cx][cy].origBox = ScaleBoundingBox(GetModelBoundingBox(chunks[cx][cy].model), (Vector3){MAP_SCALE, MAP_SCALE, MAP_SCALE});
                    chunks[cx][cy].box = UpdateBoundingBox(chunks[cx][cy].origBox, chunks[cx][cy].center);

                    chunks[cx][cy].isLoaded = true;
                    TraceLog(LOG_INFO, "loaded chunk model -> %d,%d", cx, cy);
                    pthread_mutex_unlock(&mutex);
                }
            }
        }

        FindClosestChunkAndAssignLod(&camera); //Im not sure If I need this here, but things work okay so...?

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
        Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));

        Vector3 move = { 0 };
        if (IsKeyDown(KEY_T)) 
        {
            if (onLoad && !FindNextTreeInChunk(&camera, closestCX, closestCY, 15.0f, MODEL_TREE)) {
                TraceLog(LOG_INFO, "No suitable tree found in current chunk.");
                if(!FindAnyTreeInWorld(&camera, 15.0f, MODEL_TREE))
                {
                    TraceLog(LOG_INFO, "Tree Error, cant find a tree, time to hug a tree hehe, and blow a whistle");
                }
            }
        }
        if (IsKeyDown(KEY_R)) 
        {
            if (onLoad && !FindNextTreeInChunk(&camera, closestCX, closestCY, 15.0f, MODEL_ROCK)) {
                TraceLog(LOG_INFO, "No suitable rock found in current chunk.");
                if(!FindAnyTreeInWorld(&camera, 15.0f, MODEL_ROCK))
                {
                    TraceLog(LOG_INFO, "Rock Error, geology rocks!");
                }
            }
        }
        //map input
        float goku = false;
        float spd = MOVE_SPEED;
        if (IsKeyPressed(KEY_M)) showMap = !showMap; // Toggle map
        if (IsKeyDown(KEY_EQUAL)) mapZoom += 0.01f;  // Zoom in (+ key)
        if (IsKeyDown(KEY_MINUS)) mapZoom -= 0.01f;  // Zoom out (- key)
        mapZoom = Clamp(mapZoom, 0.5f, 4.0f);
        //end map input
        if (IsKeyDown(KEY_B)) {displayBoxes = !displayBoxes;}
        if (IsKeyDown(KEY_L)) {displayLod = !displayLod;}
        if (IsKeyDown(KEY_F12)) {TakeScreenshotWithTimestamp();}
        if (IsKeyDown(KEY_F11)) {reportOn = true;}
        if (IsKeyPressed(KEY_F10)) {MemoryReport();}
        if (IsKeyPressed(KEY_F9)) {GridChunkReport();}
        if (IsKeyPressed(KEY_F8)) {GridTileReport();}
        //if (IsKeyDown(KEY_M)) {DisableCursor();} //I forget the right way to do this ...
        if (IsKeyDown(KEY_PAGE_UP)) {chosenX = (chosenX+1)%CHUNK_COUNT;}
        if (IsKeyDown(KEY_PAGE_DOWN)) {chosenY = (chosenY+1)%CHUNK_COUNT;}
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {goku=true;move = Vector3Add(move, forward);spd = GOKU_DASH_DIST;TraceLog(LOG_INFO, " --> Instant Transmission -->");}
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {goku=true;move = Vector3Add(move, forward);spd = GOKU_DASH_DIST_SHORT;TraceLog(LOG_INFO, " --> Steady does it -->");}
        if (IsKeyDown(KEY_W)) move = Vector3Add(move, forward);
        if (IsKeyDown(KEY_W)) move = Vector3Add(move, forward);
        if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, forward);
        if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);
        if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);
        if (IsKeyDown(KEY_Z)) { dayTime=!dayTime;}
        if(IsKeyDown(KEY_F1))
        {
            for (int i = 0; i < MAX_LIGHTS; i++)
            {
                lights[i].enabled = !lights[i].enabled;
            }
        }
        if (IsKeyDown(KEY_LEFT_SHIFT)) move.y -= (1.0f * MAP_SCALE);
        if (IsKeyDown(KEY_SPACE)) move.y += (1.0f * MAP_SCALE);
        if (IsKeyDown(KEY_ENTER)) {chunks[chosenX][chosenY].curTreeIdx=0;closestCX=chosenX;closestCY=chosenY;camera.position.x=chunks[closestCX][closestCY].center.x;camera.position.z=chunks[closestCX][closestCY].center.z;}

        //fade to black, end scene...
        if (dayTime) {
            skyboxTint = LerpColor(skyboxTint, skyboxDay, 0.02f);
            backGroundColor = LerpColor(backGroundColor, backgroundDay, 0.004f);
            LightPosDraw = LerpVector3(LightPosDraw, LightPosTargetDay, 0.04f);
            LightTargetDraw = LerpVector3(LightTargetDraw, LightTargetTargetDay, 0.04f);
            lightColorDraw = LerpColor(lightColorDraw, lightColorTargetDay, 0.05f);
            instanceLight.position = LightPosDraw;
            instanceLight.target = LightTargetDraw;
            instanceLight.color = lightColorDraw;
            UpdateLightValues(instancingLightShader,instanceLight);
            lightDir = LerpVector3(lightDir,(Vector3){ -10.2f, -100.0f, -10.3f },0.02f);
            SetShaderValue(heightShaderLight, lightDirLoc, &lightDir, SHADER_UNIFORM_VEC3);
            lightTileColor = LerpColor(lightTileColor, (Color){254,254,254,254}, 0.02f);
        }
        else { //night time
            skyboxTint = LerpColor(skyboxTint, skyboxNight, 0.02f);
            backGroundColor = LerpColor(backGroundColor, backgroundNight, 0.002f);
            LightPosDraw = LerpVector3(LightPosDraw, LightPosTargetNight, 0.04f);
            LightTargetDraw = LerpVector3(LightTargetDraw, LightTargetTargetNight, 0.04f);
            lightColorDraw = LerpColor(lightColorDraw, lightColorTargetNight, 0.05f);
            instanceLight.position = LightPosDraw;
            instanceLight.target = LightTargetDraw;
            instanceLight.color = lightColorDraw;
            UpdateLightValues(instancingLightShader,instanceLight);
            lightDir = LerpVector3(lightDir,(Vector3){ -5.2f, -70.0f, 15.3f },0.02f);
            SetShaderValue(heightShaderLight, lightDirLoc, &lightDir, SHADER_UNIFORM_VEC3);
            lightTileColor = LerpColor(lightTileColor, (Color){50,50,112,180}, 0.005f);
            if(onLoad && !bugGenHappened)
            {
                bugs = GenerateLightningBugs(camera.position, BUG_COUNT, 256.0256f);
                bugGenHappened=true;
            }
            else if (bugGenHappened && Vector3Distance(camera.position,lastLBSpawnPosition)>360.12f)
            {
                RegenerateLightningBugs(bugs, camera.position, BUG_COUNT, 256.0256f);
            }
            if(onLoad && !starGenHappened)
            {
                stars = GenerateStars(STAR_COUNT);
                starGenHappened = true;
            }
        }

        if (Vector3Length(move) > 0.01f) {
            move = Vector3Normalize(move);
            move = Vector3Scale(move, goku ? spd : spd * dt);
            camera.position = Vector3Add(camera.position, move);
        }
        camera.target = Vector3Add(camera.position, forward);
        skyCam.target = Vector3Add(skyCam.position, forward);
        FindClosestChunkAndAssignLod(&camera);//this one is definetley needed
        if(onLoad && camera.position.y > PLAYER_FLOAT_OFFSET)//he floats underwater
        {
            if (closestCX < 0 || closestCY < 0 || closestCX >= CHUNK_COUNT || closestCY >= CHUNK_COUNT) {
                // Outside world bounds
                TraceLog(LOG_INFO, "Outside of world bounds: %d,%d", closestCX, closestCY);
            }
            else
            {
                float groundY = GetTerrainHeightFromMeshXZ(chunks[closestCX][closestCY], camera.position.x, camera.position.z);
                //TraceLog(LOG_INFO, "setting camera y: (%d,%d){%f,%f,%f}[%f]", closestCX, closestCY, camera.position.x, camera.position.y, camera.position.z, groundY);
                if(groundY < -9000.0f){groundY=camera.position.y - PLAYER_HEIGHT;} // if we error, dont change y
                camera.position.y = groundY + PLAYER_HEIGHT;  // e.g. +1.8f for standing
            }
        }

        UpdateCamera(&camera, CAMERA_FIRST_PERSON);
        UpdateCamera(&skyCam, CAMERA_FIRST_PERSON);

        // Update the light shader with the camera view position
        SetShaderValue(lightningBugShader, lightningBugShader.locs[SHADER_LOC_VECTOR_VIEW], &camera.position, SHADER_UNIFORM_VEC3);
        SetShaderValue(instancingLightShader, instancingLightShader.locs[SHADER_LOC_VECTOR_VIEW], &camera.position, SHADER_UNIFORM_VEC3);
            //updaet ligtning bugs                        
        for (int i = 0; i < MAX_LIGHTS; i++)
        {
            lights[i].position.x = camera.position.x + 5 + i;
            lights[i].position.y = camera.position.y + 6 - i;//todo: fix the fire flies
            lights[i].position.z = camera.position.z + 4 - (i*3);
            UpdateLightValues(lightningBugShader, lights[i]);//update
            UpdateLightValues(starShader, starLights[i]);//update
        }

        BeginDrawing();
        ClearBackground(backGroundColor);
        //skybox separate scene
        BeginMode3D(skyCam);
            if(true)
            {
                //skybox stuff
                rlDisableDepthMask();
                Vector3 cam = skyCam.position;
                float dist = 60.0f;
                float size = dist * 2.0f; //has to be double to line up
                // FRONT (+Z)
                DrawSkyboxPanelFixed(skyboxPanelFrontModel, (Vector3){cam.x, cam.y, cam.z - dist}, 0.0f, (Vector3){0, 1, 0}, size);
                // BACK (-Z)
                DrawSkyboxPanelFixed(skyboxPanelBackModel, (Vector3){cam.x, cam.y, cam.z + dist}, 180.0f, (Vector3){0, 1, 0}, size);
                // LEFT (-X)
                DrawSkyboxPanelFixed(skyboxPanelLeftModel, (Vector3){cam.x - dist, cam.y, cam.z}, 90.0f, (Vector3){0, 1, 0}, size);
                // RIGHT (+X)
                DrawSkyboxPanelFixed(skyboxPanelRightModel, (Vector3){cam.x + dist, cam.y, cam.z}, -90.0f, (Vector3){0, 1, 0}, size);
                // UP (+Y)
                DrawSkyboxPanelFixed(skyboxPanelUpModel, (Vector3){cam.x, cam.y + dist, cam.z}, 90.0f, (Vector3){1, 0, 0}, size);
                rlEnableDepthMask();
            }
        EndMode3D();
        //regular scene of the map
        BeginMode3D(camera);
            if(onLoad){SetCustomCameraProjection(camera, 45.0f, (float)SCREEN_WIDTH/SCREEN_HEIGHT, 0.3f, 5000.0f);} // Near = 1, Far = 4000
            //rlDisableBackfaceCulling();
            bool loadedEem = true;
            bool loadedEemTiles = true;
            int loadCnt = 0;
            //int loadTileCnt = 0; -- this one needs to be global so we can update it while loading tiles
            //get frustum
            Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
            Matrix proj = MatrixPerspective(DEG2RAD * camera.fovy, SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 2048.0f);
            Matrix projChunk8 = MatrixPerspective(DEG2RAD * camera.fovy, SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 16384.0f);//for far away chunks
            Matrix vp = MatrixMultiply(view, proj);
            Matrix vpChunk8 = MatrixMultiply(view, projChunk8);
            Frustum frustum = ExtractFrustum(vp);
            Frustum frustumChunk8 = ExtractFrustum(vpChunk8);
            int gx, gy;
            GetGlobalTileCoords(camera.position, &gx, &gy);
            int playerTileX  = gx % TILE_GRID_SIZE;
            int playerTileY  = gy % TILE_GRID_SIZE;
            //lightning bugs
            if(!dayTime)
            {
                //if doing reflectoinis, stuff like this ....
                // // Set floor model texture tiling and emissive color parameters on shader
                // SetShaderValue(shader, textureTilingLoc, &floorTextureTiling, SHADER_UNIFORM_VEC2);
                // Vector4 floorEmissiveColor = ColorNormalize(floor.materials[0].maps[MATERIAL_MAP_EMISSION].color);
                // SetShaderValue(shader, emissiveColorLoc, &floorEmissiveColor, SHADER_UNIFORM_VEC4);

                // // Set floor metallic and roughness values
                // SetShaderValue(shader, metallicValueLoc, &floor.materials[0].maps[MATERIAL_MAP_METALNESS].value, SHADER_UNIFORM_FLOAT);
                // SetShaderValue(shader, roughnessValueLoc, &floor.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value, SHADER_UNIFORM_FLOAT);

                // // Set old car model texture tiling, emissive color and emissive intensity parameters on shader
                // SetShaderValue(shader, textureTilingLoc, &carTextureTiling, SHADER_UNIFORM_VEC2);
                // Vector4 carEmissiveColor = ColorNormalize(car.materials[0].maps[MATERIAL_MAP_EMISSION].color);
                // SetShaderValue(shader, emissiveColorLoc, &carEmissiveColor, SHADER_UNIFORM_VEC4);
                // float emissiveIntensity = 0.01f;
                // SetShaderValue(shader, emissiveIntensityLoc, &emissiveIntensity, SHADER_UNIFORM_FLOAT);
                
                // // Set old car metallic and roughness values
                // SetShaderValue(shader, metallicValueLoc, &car.materials[0].maps[MATERIAL_MAP_METALNESS].value, SHADER_UNIFORM_FLOAT);
                // SetShaderValue(shader, roughnessValueLoc, &car.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value, SHADER_UNIFORM_FLOAT);
                // Draw spheres to show the lights positions
                // for (int i = 0; i < MAX_LIGHTS; i++)
                // {
                //     // Color lightColor = (Color){ lights[i].color.r*255, lights[i].color.g*255, lights[i].color.g*255, lights[i].color.b*255 };
                    
                //     // if (lights[i].enabled) DrawSphereEx(lights[i].position, 0.2f, 8, 8, lightColor);
                //     // else DrawSphereWires(lights[i].position, 0.2f, 8, 8, ColorAlpha(lightColor, 0.3f));
                    
                // }
                if(onLoad) //fire flies
                {
                    int bugsAdded = 0;
                    int starsAdded = 0;
                    //- loop through all of the static props that are int he active active tile zone
                    Matrix transforms[BUG_COUNT] = {0};
                    float blinkValues[BUG_COUNT] = {0};
                    for (int i = 0; i < BUG_COUNT; i++)
                    {
                        if(!IsBoxInFrustum(bugs[i].box , frustumChunk8)){continue;}
                        UpdateLightningBugs(bugs,BUG_COUNT,dt*0.0073f);//I think this is wrong, but it works out better this way
                        //first update the bugs positions
                        blinkValues[bugsAdded] = bugs[i].alpha;
                        //get ready to draw
                        Vector3 _p = bugs[i].pos;
                        Matrix translation = MatrixTranslate(_p.x, _p.y, _p.z);
                        Vector3 toCamera = Vector3Subtract(camera.position, bugs[i].pos);
                        toCamera.y = 0; // Optional: lock to horizontal billboard
                        toCamera = Vector3Normalize(toCamera);
                        Vector3 axis = (Vector3){0,1,0};//Vector3Normalize((Vector3){ (float)GetRandomValue(0, 360), (float)GetRandomValue(0, 360), (float)GetRandomValue(0, 360) });
                        float angle = -bugs[i].angle+PI/2.8f;//float angle = 0.0f;//(float)GetRandomValue(0, 180)*DEG2RAD;
                        Matrix rotation = MatrixRotate(axis, angle);
                        transforms[bugsAdded] = MatrixMultiply(rotation, translation);//todo: add rotations and such
                        bugsAdded++;
                    }   
                    // Before drawing:
                    int blinkAttribLoc = GetShaderLocationAttrib(lightningBugShader, "instanceBlink");
                    SetShaderValueV(lightningBugShader, blinkAttribLoc, blinkValues, SHADER_ATTRIB_FLOAT, bugsAdded);
                    float time = GetTime(); // Raylib built-in
                    int timeLoc = GetShaderLocation(lightningBugShader, "u_time");
                    SetShaderValue(lightningBugShader, timeLoc, &time, SHADER_UNIFORM_FLOAT);
                    DrawMeshInstancedCustom(
                            sphereMesh, 
                            sphereMaterial, 
                            transforms, 
                            bugsAdded
                    );//rpi5
                    //stars ** ** ** ** ** **** ** ** ** ** **** ** ** ** ** **** ** ** ** ** **** ** ** ** ** **
                    Matrix starTransforms[STAR_COUNT] = {0};
                    float starBlinkValues[STAR_COUNT] = {0};
                    //UpdateStars(stars,STAR_COUNT);
                    for (int i = 0; i < STAR_COUNT; i++)
                    {
                        //first update the bugs positions
                        starBlinkValues[starsAdded] = stars[i].alpha;
                        // //get ready to draw
                        Vector3 __p = stars[i].pos;
                        // Matrix translation = MatrixTranslate();
                        // starTransforms[starsAdded] = translation;//MatrixMultiply(rotation, translation);//todo: add rotations and such
                        
                        Matrix mat = MatrixTranslate(__p.x, __p.y, __p.z);
                        mat.m15 = (float)i;  // Encode instanceId into the matrix (row 3, column 1)
                        starTransforms[i] = mat;
                        starsAdded++;
                    }   
                    // Before drawing:
                    int blinkStarAttribLoc = GetShaderLocationAttrib(starShader, "instanceBlink");
                    SetShaderValueV(starShader, blinkStarAttribLoc, starBlinkValues, SHADER_ATTRIB_FLOAT, starsAdded);
                    float timeStar = GetTime(); // Raylib built-in
                    int timeStarLoc = GetShaderLocation(starShader, "u_time");
                    SetShaderValue(starShader, timeStarLoc, &timeStar, SHADER_UNIFORM_FLOAT);
                    DrawMeshInstancedCustom(
                            sphereStarMesh, 
                            sphereStarMaterial, 
                            starTransforms, 
                            starsAdded
                    );//rpi5
                    //** ** ** ** ** **** ** ** ** ** **** ** ** ** ** **** ** ** ** ** **** ** ** ** ** **
                }
            }
            for(int te = 0; te < foundTileCount; te++)
            {
                if(!foundTiles[te].isReady){loadedEemTiles=false;continue;}//complete RAM state needs to control if we show the loading bar
                if(!foundTiles[te].isLoaded){continue;}
                //TraceLog(LOG_INFO, "TEST - Maybe - Drawing tile model: chunk %02d_%02d, tile %02d_%02d", foundTiles[te].cx, foundTiles[te].cy, foundTiles[te].tx, foundTiles[te].ty);
                if(chunks[foundTiles[te].cx][foundTiles[te].cy].lod == LOD_64 //this one first because its quick, although it might get removed later
                    && (!IsTileActive(foundTiles[te].cx,foundTiles[te].cy,foundTiles[te].tx,foundTiles[te].ty, closestCX, closestCY, playerTileX, playerTileY) || USE_TILES_ONLY) 
                    && IsBoxInFrustum(foundTiles[te].box , frustumChunk8))
                {
                    if(reportOn){tileBcCount++;tileTriCount+=foundTiles[te].model.meshes[0].triangleCount;};
                    DrawModel(foundTiles[te].model, (Vector3){0,0,0}, 1.0f, lightTileColor);
                    //TraceLog(LOG_INFO, "TEST Drawing tile model: chunk %02d_%02d, tile %02d_%02d", foundTiles[te].cx, foundTiles[te].cy, foundTiles[te].tx, foundTiles[te].ty);
                    if(displayBoxes){DrawBoundingBox(foundTiles[te].box,RED);}
                }
            }
            for (int cy = 0; cy < CHUNK_COUNT; cy++) {
                for (int cx = 0; cx < CHUNK_COUNT; cx++) {
                    if(chunks[cx][cy].isLoaded)
                    {
                        loadCnt++;
                        //if(onLoad && !IsBoxInFrustum(chunks[cx][cy].box, frustum)){continue;}
                        //if(onLoad && (cx!=closestCX||cy!=closestCY) && !ShouldRenderChunk(chunks[cx][cy].center,camera)){continue;}
                        //TraceLog(LOG_INFO, "drawing chunk: %d,%d", cx, cy);
                        if(chunks[cx][cy].lod == LOD_64) 
                        {
                            chunkBcCount++;
                            chunkTriCount+=chunks[cx][cy].model.meshes[0].triangleCount;
                            Matrix mvp = MatrixMultiply(proj, MatrixMultiply(view, chunks[cx][cy].model.transform));
                            SetShaderValueMatrix(heightShaderLight, mvpLocLight, mvp);
                            //SetShaderValueMatrix(heightShaderLight, modelLocLight, MatrixIdentity());
                            Matrix chunkModelMatrix = MatrixTranslate(chunks[cx][cy].position.x, chunks[cx][cy].position.y, chunks[cx][cy].position.z);
                            SetShaderValueMatrix(heightShaderLight, modelLocLight, chunkModelMatrix);
                            Vector3 camPos = camera.position;
                            SetShaderValue(heightShaderLight, GetShaderLocation(heightShaderLight, "cameraPosition"), &camPos, SHADER_UNIFORM_VEC3);
                            BeginShaderMode(heightShaderLight);
                            DrawModel(chunks[cx][cy].model, chunks[cx][cy].position, MAP_SCALE, WHITE);
                            EndShaderMode();
                            if(onLoad)//only once we have fully loaded everything
                            {
                                //handle water first
                                for (int w=0; w<chunks[cx][cy].waterCount; w++)
                                {
                                    glEnable(GL_POLYGON_OFFSET_FILL);
                                    glPolygonOffset(-1.0f, -1.0f); // Push water slightly forward in Z-buffer
                                    rlDisableBackfaceCulling();
                                    //rlDisableDepthMask(); 
                                    //BeginBlendMode(BLEND_ALPHA);
                                    Vector3 cameraPos = camera.position;
                                    Vector3 waterPos = { 0, WATER_Y_OFFSET, 0 };
                                    // Get direction from patch to camera
                                    Vector3 toCamera = Vector3Subtract(waterPos, cameraPos);
                                    // Scale it down to something subtle, like 5%
                                    Vector3 shift = Vector3Scale(toCamera, 0.05f);
                                    // Final draw position is nudged toward the player
                                    Vector3 drawPos = Vector3Add(waterPos, shift);
                                    Vector2 offset = (Vector2){ w * cx, w * cy };
                                    SetShaderValue(waterShader, offsetLoc, &offset, SHADER_UNIFORM_VEC2);
                                    BeginShaderMode(waterShader);
                                    DrawModel(chunks[cx][cy].water[w], drawPos, 1.0f, (Color){ 0, 100, 253, 232 });
                                    EndShaderMode();
                                    //EndBlendMode();
                                    //rlEnableDepthMask(); 
                                    rlEnableBackfaceCulling();
                                    glDisable(GL_POLYGON_OFFSET_FILL);
                                }
                                if(!USE_GPU_INSTANCING)
                                {
                                    for(int pInd = 0; pInd<chunks[cx][cy].treeCount; pInd++)
                                    {
                                        BoundingBox tob = UpdateBoundingBox(treeOrigBox,chunks[cx][cy].props[pInd].pos);
                                        if((!IsTreeInActiveTile(chunks[cx][cy].props[pInd].pos, closestCX,closestCY,playerTileX,playerTileY) || USE_TILES_ONLY)
                                            || !IsBoxInFrustum(tob, frustum)){continue;}
                                        //TraceLog(LOG_INFO, "Drawing (%d,%d) tree at %.2f %.2f %.2f", cx, cy, chunks[cx][cy].treePositions[pInd].x, chunks[cx][cy].treePositions[pInd].y, chunks[cx][cy].treePositions[pInd].z);
                                        if(USE_TREE_CUBES)
                                        {
                                            DrawModelEx(treeCubeModel, chunks[cx][cy].props[pInd].pos, (Vector3){0, 1, 0}, 0.0f, (Vector3){1, 1, 1}, DARKGREEN);
                                        }
                                        else
                                        {
                                            bool close = Vector3Distance(chunks[cx][cy].props[pInd].pos,camera.position) < FULL_TREE_DIST;
                                            Model tree3 = close ? treeModel : bgTreeModel;
                                            tree3 = chunks[cx][cy].props[pInd].type==MODEL_ROCK?rockModel:tree3;
                                            if(reportOn){treeTriCount+=tree3.meshes[0].triangleCount;treeBcCount++;}
                                            DrawModel(tree3, chunks[cx][cy].props[pInd].pos, 1.0f, WHITE);
                                        }
                                        if(displayBoxes){DrawBoundingBox(tob,BLUE);}
                                    }
                                }
                                else //GPU INSTANCING FOR CLOSE STATIC PROPS
                                {
                                    int counter[MODEL_TOTAL_COUNT] = {0,0};
                                    //- loop through all of the static props that are int he active active tile zone
                                    for(int pInd = 0; pInd<chunks[cx][cy].treeCount; pInd++)
                                    {
                                        //culling
                                        BoundingBox tob = UpdateBoundingBox(treeOrigBox,chunks[cx][cy].props[pInd].pos);
                                        if((!IsTreeInActiveTile(chunks[cx][cy].props[pInd].pos, closestCX,closestCY,playerTileX,playerTileY) || USE_TILES_ONLY)
                                            || !IsBoxInFrustum(tob, frustum)){continue;}
                                        //get ready to draw
                                        Vector3 _p = chunks[cx][cy].props[pInd].pos;
                                        Matrix translation = MatrixTranslate(_p.x, _p.y, _p.z);
                                        Vector3 axis = Vector3Normalize((Vector3){ (float)GetRandomValue(0, 360), (float)GetRandomValue(0, 360), (float)GetRandomValue(0, 360) });
                                        float angle = (float)GetRandomValue(0, 180)*DEG2RAD;
                                        Matrix rotation = MatrixRotate(axis, angle);
                                        //transforms[i] = MatrixMultiply(rotation, translation);//todo: add rotations and such
                                        HighFiTransforms[chunks[cx][cy].props[pInd].type][counter[chunks[cx][cy].props[pInd].type]] = translation;//well this is kindof insane
                                        counter[chunks[cx][cy].props[pInd].type]++;
                                        if(displayBoxes){DrawBoundingBox(tob,BLUE);}
                                        if(reportOn){treeTriCount+=HighFiStaticObjectModels[chunks[cx][cy].props[pInd].type].meshes[0].triangleCount;}
                                    }
                                    //draw
                                    for(int mt=0; mt<MODEL_TOTAL_COUNT; mt++)
                                    {
                                        treeBcCount++;
                                        DrawMeshInstancedCustom(
                                            HighFiStaticObjectModels[mt].meshes[0], 
                                            HighFiStaticObjectMaterials[mt], 
                                            HighFiTransforms[mt], 
                                            counter[mt]
                                        );//rpi5
                                    }
                                }
                            }
                        }
                        else if(chunks[cx][cy].lod == LOD_32 && IsBoxInFrustum(chunks[cx][cy].box, frustumChunk8)) {
                            chunkBcCount++;
                            chunkTriCount+=chunks[cx][cy].model32.meshes[0].triangleCount;
                            Matrix mvp = MatrixMultiply(proj, MatrixMultiply(view, chunks[cx][cy].model.transform));
                            SetShaderValueMatrix(heightShaderLight, mvpLocLight, mvp);
                            BeginShaderMode(heightShaderLight);
                            DrawModel(chunks[cx][cy].model32, chunks[cx][cy].position, MAP_SCALE, displayLod?BLUE:WHITE);
                            EndShaderMode();
                            for (int w=0; w<chunks[cx][cy].waterCount; w++)
                            {
                                glEnable(GL_POLYGON_OFFSET_FILL);
                                glPolygonOffset(-1.0f, -1.0f); // Push water slightly forward in Z-buffer
                                rlDisableBackfaceCulling();
                                //rlDisableDepthMask();
                                //BeginBlendMode(BLEND_ALPHA);
                                Vector3 cameraPos = camera.position;
                                Vector3 waterPos = { 0, WATER_Y_OFFSET, 0 };
                                // Get direction from patch to camera
                                Vector3 toCamera = Vector3Subtract(waterPos, cameraPos);
                                // Scale it down to something subtle, like 5%
                                Vector3 shift = Vector3Scale(toCamera, 0.05f);
                                // Final draw position is nudged toward the player
                                Vector3 drawPos = Vector3Add(waterPos, shift);
                                Vector2 offset = (Vector2){ w * cx, w * cy };
                                SetShaderValue(waterShader, offsetLoc, &offset, SHADER_UNIFORM_VEC2);
                                BeginShaderMode(waterShader);
                                DrawModel(chunks[cx][cy].water[w], drawPos, 1.0f, (Color){ 0, 100, 254, 180 });
                                EndShaderMode();
                                //EndBlendMode();
                                //rlEnableDepthMask();
                                rlEnableBackfaceCulling();
                                glDisable(GL_POLYGON_OFFSET_FILL);
                            }
                        }
                        else if(chunks[cx][cy].lod == LOD_16 && IsBoxInFrustum(chunks[cx][cy].box, frustumChunk8)) {
                            chunkBcCount++;
                            chunkTriCount+=chunks[cx][cy].model16.meshes[0].triangleCount;
                            DrawModel(chunks[cx][cy].model16, chunks[cx][cy].position, MAP_SCALE, displayLod?PURPLE:chunk_16_color);
                        }
                        else if(IsBoxInFrustum(chunks[cx][cy].box, frustumChunk8)||!onLoad) {
                            chunkBcCount++;
                            chunkTriCount+=chunks[cx][cy].model8.meshes[0].triangleCount;
                            DrawModel(chunks[cx][cy].model8, chunks[cx][cy].position, MAP_SCALE, displayLod?RED:chunk_08_color);
                        }
                        if(displayBoxes){DrawBoundingBox(chunks[cx][cy].box,YELLOW);}
                    }
                    else {loadedEem = false;}
                }
            }
            //rlEnableBackfaceCulling();
            if(reportOn) //triangle report
            {
                totalBcCount = tileBcCount + chunkBcCount + treeBcCount;
                totalTriCount = tileTriCount + chunkTriCount + treeTriCount;
                printf("Estimated tile triangles this frame  :  %d\n", tileTriCount);
                printf("Estimated batch calls for tiles      :  %d\n", tileBcCount);
                printf("Estimated tree triangles this frame  :  %d\n", treeTriCount);
                printf("Estimated batch calls for trees      :  %d\n", treeBcCount);
                printf("Estimated chunk triangles this frame :  %d\n", chunkTriCount);
                printf("Estimated batch calls for chunks     :  %d\n", chunkBcCount);
                printf("Estimated TOTAL triangles this frame :  %d\n", totalTriCount);
                printf("Estimated TOTAL batch calls          :  %d\n", totalBcCount);
                printf("Current FPS (so you can document)    :  %d\n", GetFPS());
            }
            //DrawGrid(256, 1.0f);
        EndMode3D();
        DrawText("WASD to move, mouse to look", 10, 10, 20, BLACK);
        DrawText(TextFormat("Pitch: %.2f  Yaw: %.2f", pitch, yaw), 10, 30, 20, BLACK);
        DrawText(TextFormat("Next Chunk: (%d,%d)", chosenX, chosenY), 10, 50, 20, BLACK);
        DrawText(TextFormat("Current Chunk: (%d,%d)", closestCX, closestCY), 10, 70, 20, BLACK);
        DrawText(TextFormat("X: %.2f  Y: %.2f Z: %.2f", camera.position.x, camera.position.y, camera.position.z), 10, 90, 20, BLACK);
        if (showMap) {
            // Map drawing area (scaled by zoom)
            //
            Rectangle dest = {
                SCREEN_WIDTH - (GAME_MAP_SIZE * mapZoom) - 10, //just calculate this x value every time
                mapViewport.y,
                mapViewport.width * mapZoom,
                mapViewport.height * mapZoom
            };
            DrawTexturePro(mapTexture,
                (Rectangle){ 0, 0, mapTexture.width, mapTexture.height },
                dest,
                (Vector2){ 0, 0 },
                0.0f,
                WHITE);

            // Player marker (assume position normalized to map range)
            float normalizedX = (camera.position.x + (MAX_WORLD_SIZE/2)) / WORLD_WIDTH;
            float normalizedY = (camera.position.z + (MAX_WORLD_SIZE/2)) / WORLD_HEIGHT;

            Vector2 marker = {
                dest.x + normalizedX * dest.width,
                dest.y + normalizedY * dest.height
            };
            DrawCircleV(marker, 3, RED);
        }
        if(!loadedEem || !loadedEemTiles)
        {
            // Outline
            DrawRectangleLines(500, 350, 204, 10, DARKGRAY);
            // Fill
            float chunkPercent = ((float)loadCnt)/(CHUNK_COUNT * CHUNK_COUNT);
            float tilePercent = ((float)loadTileCnt)/manifestTileCount;
            float totalPercent = (chunkPercent+tilePercent)/2.0f;
            int gc = (int)((totalPercent)*255);
            DrawRectangle(502, 352, (int)((200 - 4) * (totalPercent)), 10 - 4, (Color){100,gc,40,255});
        }
        else if(!onLoad)//this used to do something useful, now it does nothing really but snap the player a bit
        {
            onLoad = true;
            float seaLevel = 0.0f;
            float totalY = 0.0f;
            int totalVerts = 0;
            for (int cy = 0; cy < CHUNK_COUNT; cy++) {
                for (int cx = 0; cx < CHUNK_COUNT; cx++) {
                    Mesh mesh = chunks[cx][cy].model.meshes[0];
                    if (mesh.vertexCount == 0 || mesh.vertices == NULL) continue;
                    float *verts = (float *)mesh.vertices;
                    for (int i = 0; i < mesh.vertexCount; i++) {
                        float y = verts[i * 3 + 1];  // Y component
                        totalY += (y * MAP_SCALE);
                    }
                    totalVerts += mesh.vertexCount;
                    if(chunks[cx][cy].treeCount>0){TraceLog(LOG_INFO, "trees (%d,%d) ->  %d", cx,cy,chunks[cx][cy].treeCount);}
                }
            }
            //TODO: Sealvel off for trees
            seaLevel = (totalY / totalVerts);
            TraceLog(LOG_INFO, "seaLevel: %f", seaLevel);
            //seaLevel = 666.666;
            for (int cy = 0; cy < CHUNK_COUNT; cy++) {
                for (int cx = 0; cx < CHUNK_COUNT; cx++) {
                    // Offset the entire chunk's Y to make sea level align with Y=0
                    //chunks[cx][cy].position.y += seaLevel;
                    //chunks[cx][cy].center.y += seaLevel;
                    //chunks[cx][cy].box = UpdateBoundingBox(chunks[cx][cy].origBox, chunks[cx][cy].center);
                }
            }
            camera.position.x = -16; //3000;//
            camera.position.z = -16; //3000;//
        }
        DrawFPS(10,110);
        EndDrawing();
    }

    //unload skybox
    UnloadTexture(skyTexFront);
    UnloadTexture(skyTexBack);
    UnloadTexture(skyTexLeft);
    UnloadTexture(skyTexRight);
    UnloadTexture(skyTexUp);
    //unload in game map
    UnloadTexture(mapTexture);
    //unload tiles
    free(foundTiles);
    //unload chunks
    for (int cy = 0; cy < CHUNK_COUNT; cy++)
    {
        for (int cx = 0; cx < CHUNK_COUNT; cx++) {
            if(chunks[cx][cy].isLoaded)
            {
                UnloadModel(chunks[cx][cy].model);
                UnloadModel(chunks[cx][cy].model32);
                UnloadModel(chunks[cx][cy].model16);
                UnloadModel(chunks[cx][cy].model8);
                UnloadTexture(chunks[cx][cy].texture);
                UnloadTexture(chunks[cx][cy].textureBig);
                UnloadTexture(chunks[cx][cy].textureFull);
                UnloadTexture(chunks[cx][cy].textureDamn);
                free(chunks[cx][cy].props);
                chunks[cx][cy].props = NULL;
            }
        }
    }
    // for (int y = 0; y < CHUNK_COUNT; y++) { //... nice little report
    //     for (int x = 0; x < CHUNK_COUNT; x++) {
    //         Vector3 min = {
    //             (x - (CHUNK_COUNT / 2 - 1)) * CHUNK_WORLD_SIZE,
    //             0,
    //             (y - (CHUNK_COUNT / 2 - 1)) * CHUNK_WORLD_SIZE
    //         };
    //         TraceLog(LOG_INFO, "Chunk %d,%d covers from %.1f to %.1f", x, y, min.x, min.x + CHUNK_WORLD_SIZE);
    //     }
    // }
    for (int x = 0; x < CHUNK_COUNT; x++) {
        free(chunks[x]);
    }
    free(chunks);
    chunks = NULL;

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
