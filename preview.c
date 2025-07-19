
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> // for usleep
#include <string.h>
#include <time.h> // for seeding rand once if needed

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
#define USE_TREE_CUBES false
#define FULL_TREE_DIST 112.2f

#define GOKU_DASH_DIST 512.333f
#define MOVE_SPEED 16.16f

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 800

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
    bool isLoaded;
    bool isReady;
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
    Texture2D texture;
    Texture2D textureBig;
    Texture2D textureFull;
    Vector3 position;
    Vector3 center;
    Vector3 *treePositions;
    int treeCount;
    int curTreeIdx;
} Chunk;

Chunk **chunks = NULL;
Vector3 cameraVelocity = { 0 };
Mesh skyboxPanelMesh;
Model skyboxPanelFrontModel;
Model skyboxPanelBackModel;
Model skyboxPanelLeftModel;
Model skyboxPanelRightModel;
Model skyboxPanelUpModel;

//int curTreeIdx = 0;
int tree_elf = 0;
//very very important
int chosenX = 7;
int chosenY = 7;
int closestCX = 7;
int closestCY = 7;
bool onLoad = false;

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

bool ShouldRenderChunk(Vector3 chunkCenter, Camera camera)
{
    Vector3 toChunk = Vector3Normalize(Vector3Subtract(chunkCenter, camera.position));
    float dot = Vector3DotProduct(Vector3Normalize(Vector3Subtract(camera.target, camera.position)), toChunk);

    return (dot > 0.4f) && (Vector3Distance(camera.position, chunkCenter) < 2048.0f);
}

void DrawSkyboxPanelFixed(Model model, Vector3 position, float angleDeg, Vector3 axis, float size)
{
    Color tint = WHITE;
    tint.a = 100;
    Vector3 scale = (Vector3){ size, size, 1.0f };
    DrawModelEx(model, position, axis, angleDeg, scale, tint);
}

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

void FindClosestChunkAndAssignLod(Camera3D *camera) 
{
    bool foundChunkWithBox = false;
    //TraceLog(LOG_INFO, "FindClosestChunkAndAssignLod (1): (%d x %d)", closestCX, closestCY);
    // --- First pass: find closest chunk ---
    // - TODO: fix the bounding box method, it would be nice if we have a fall back, but incase that is ever wrong ...
        // - then again, the new version is probably faster, should I fix this?
    // for (int cy = 0; cy < CHUNK_COUNT; cy++) {
    //     for (int cx = 0; cx < CHUNK_COUNT; cx++) {
    //         if (!chunks[cx][cy].isLoaded) continue;
    //         //TraceLog(LOG_INFO, "Chunk %d,%d is loaded", cx, cy);
    //         BoundingBox box = chunks[cx][cy].box;
    //         float x = camera->position.x;
    //         float z = camera->position.z;

    //         //TraceLog(LOG_INFO, "Cam: %.2f %.2f | BoxX: %.2f..%.2f BoxZ: %.2f..%.2f",
    //                 //x, z, box.min.x, box.max.x, box.min.z, box.max.z);
    //         if (camera->position.x >= box.min.x && camera->position.x <= box.max.x &&
    //             camera->position.z >= box.min.z && camera->position.z <= box.max.z) {
    //             //TraceLog(LOG_INFO, "Found Chunk -> cx=%d cy=%d", cx, cy);
    //             closestCX = cx;
    //             closestCY = cy;
    //             foundChunkWithBox = true;
    //             break;
    //         }
    //     }
    //     if (closestCX != -1) break;
    // }
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
        else
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

bool FindNextTreeInChunk(Camera3D *camera, int cx, int cy, float minDistance) {
    if (!chunks[cx][cy].treePositions || chunks[cx][cy].treeCount<=0) {
        TraceLog(LOG_WARNING, "No tree data loaded for chunk_%02d_%02d -> (%d)", cx, cy, chunks[cx][cy].treeCount);
        return false;
    }
    if(!onLoad)
    {
        TraceLog(LOG_WARNING, "onLoad = false");
        return false;
    }

    int count = chunks[cx][cy].treeCount;
    if (count <= 0) return false;
    if (count <= chunks[cx][cy].curTreeIdx){chunks[cx][cy].curTreeIdx = 0;}//if for whatever reason our index is too large, reset
    float minDistSqr = minDistance * minDistance;
    Vector3 camPos = camera->position; //readonly

    for (int i = chunks[cx][cy].curTreeIdx; i < count; i++) {
        Vector3 t = chunks[cx][cy].treePositions[i];
        float dx = t.x - camPos.x;
        float dy = t.y - camPos.y;
        float dz = t.z - camPos.z;
        float distSqr = dx * dx + dy * dy + dz * dz;

        if (distSqr >= minDistSqr) {
            camera->position = (Vector3){t.x, t.y+PLAYER_HEIGHT, t.z};  // Set cam above tree
            FindClosestChunkAndAssignLod(camera);
            camera->target = (Vector3){0, 0, t.z};           // Look at the tree but not really
            chunks[cx][cy].curTreeIdx = i;
            return true;
        }
    }
    TraceLog(LOG_WARNING, "No suitable trees found in this chunk.");
    return false;  // No suitable tree found
}

bool FindAnyTreeInWorld(Camera *camera, float radius) {
    if(!onLoad)
    {
        TraceLog(LOG_WARNING, "onLoad = false (world)");
        return false;
    }
    int attempts = 0;
    const int maxAttempts = MAX_CHUNK_DIM * MAX_CHUNK_DIM;

    while (attempts < maxAttempts) {
        int cx = rand() % MAX_CHUNK_DIM;
        int cy = rand() % MAX_CHUNK_DIM;
        tree_elf++;

        if (FindNextTreeInChunk(camera, cx, cy, radius)) {
            TraceLog(LOG_INFO, "Found tree in chunk_%02d_%02d", cx, cy);
            return true;
        }

        attempts++;
    }

    TraceLog(LOG_WARNING, "No suitable trees found in any chunk.");
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
    //snprintf(colorPath, sizeof(colorPath), "map/chunk_%02d_%02d/color.png", cx, cy);
    //snprintf(colorBigPath, sizeof(colorBigPath), "map/chunk_%02d_%02d/color_big.png", cx, cy);
    //snprintf(slopePath, sizeof(slopePath), "map/chunk_%02d_%02d/slope.png", cx, cy);
    //snprintf(slopeBigPath, sizeof(slopeBigPath), "map/chunk_%02d_%02d/slope_big.png", cx, cy);
    snprintf(avgPath, sizeof(avgPath), "map/chunk_%02d_%02d/avg.png", cx, cy);
    snprintf(avgBigPath, sizeof(avgBigPath), "map/chunk_%02d_%02d/avg_big.png", cx, cy);
    snprintf(avgFullPath, sizeof(avgFullPath), "map/chunk_%02d_%02d/avg_full.png", cx, cy);
    // --- Load images and assign to model material ---
    TraceLog(LOG_INFO, "Loading image in worker thread: %s", avgPath);
    Image img = LoadSafeImage(avgPath); //using slope and color avg right now
    Image imgBig = LoadSafeImage(avgBigPath);
    Image imgFull = LoadSafeImage(avgFullPath);
    chunks[cx][cy].img_tex = img;
    chunks[cx][cy].img_tex_big = imgBig;
    chunks[cx][cy].img_tex_full = imgFull;
    chunks[cx][cy].isTextureReady = true;
    // Texture2D texture = LoadSafeImage(avgPath); //using slope and color avg right now
    // Texture2D textureBig = LoadSafeImage(avgBigPath);
    // Texture2D textureFull = LoadSafeImage(avgFullPath);
    // SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    // SetTextureWrap(textureBig, TEXTURE_WRAP_CLAMP);
    // SetTextureWrap(textureFull, TEXTURE_WRAP_CLAMP);
    // GenTextureMipmaps(&textureFull);  // <-- this generates mipmaps
    // SetTextureFilter(textureFull, TEXTURE_FILTER_TRILINEAR); // use a better filter
    // GenTextureMipmaps(&textureBig);  // <-- this generates mipmaps
    // SetTextureFilter(textureBig, TEXTURE_FILTER_TRILINEAR); // use a better filter
    // GenTextureMipmaps(&texture);  // <-- this generates mipmaps
    // SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR); // use a better filter
    // chunks[cx][cy].texture = texture;  // Copy contents
    // chunks[cx][cy].textureBig = textureBig;
    // chunks[cx][cy].textureFull = textureFull;
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
    Vector3 *treePositions = malloc(sizeof(Vector3) * (1024));//some buffer for these, should never be above 512
    for (int i = 0; i < treeCount; i++) {
        float x, y, z;
        fscanf(fp, "%f %f %f\n", &x, &y, &z);
        treePositions[i] = (Vector3){ x, y, z };
    }

    fclose(fp);

    chunks[cx][cy].treePositions = treePositions;
    chunks[cx][cy].treeCount = treeCount;//treeCount > 1024 ? 1024 : treeCount;//limit it for now
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

    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].textureFull;
    model32.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = chunks[cx][cy].textureBig;
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
    //mark the chunk with identifiers
    chunks[cx][cy].cx = cx;
    chunks[cx][cy].cy = cy;
    chunks[cx][cy].id = (cx*CHUNK_COUNT) + cy;
    //load trees
    LoadTreePositions(cx, cy);
    //report
    TraceLog(LOG_INFO, "Chunk [%02d, %02d] loaded at position (%.1f, %.1f, %.1f)", 
             cx, cy, position.x, position.y, position.z);
}


void *ChunkLoaderThread(void *arg) {
    for (int cy = 0; cy < CHUNK_COUNT; cy++) {
        for (int cx = 0; cx < CHUNK_COUNT; cx++) {
            if (!chunks[cx][cy].isLoaded) {
                PreLoadTexture(cx, cy);
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

int main(void) {
    bool displayBoxes = false;
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
        }
    }
    //----------------------DONE -> init chunks---------------------

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Map Preview with Trees & Grass");
    InitAudioDevice();
    SetTargetFPS(60);
    //tree model
    Model treeCubeModel, treeModel, bgTreeModel;
    char treePath[64];
    char bgTreePath[64];
    snprintf(treePath, sizeof(treePath), "models/tree.glb");
    snprintf(bgTreePath, sizeof(bgTreePath), "models/tree_bg.glb");
    treeModel = LoadModel(treePath);
    bgTreeModel = LoadModel(bgTreePath);
    treeCubeModel = LoadModelFromMesh(GenMeshCube(0.67f, 16.0f, 0.67f));
    treeCubeModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = DARKGREEN;
    BoundingBox treeOrigBox = GetModelBoundingBox(treeCubeModel);
    //game map
    Texture2D mapTexture;
    bool showMap = true;
    float mapZoom = 1.0f;
    Rectangle mapViewport = { SCREEN_WIDTH - GAME_MAP_SIZE - 10, 10, 128, 128 };  // Map position + size
    mapTexture = LoadTexture("map/elevation_color_map.png");


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

    for (int cy = 0; cy < CHUNK_COUNT; cy++) {
        for (int cx = 0; cx < CHUNK_COUNT; cx++) {
            if (!chunks[cx][cy].isLoaded && !chunks[cx][cy].isReady) {
                //PreLoadTexture(cx, cy);
                //LoadChunk(cx, cy);
            }
        }
    }
    StartChunkLoader();

    Camera3D camera = {
        .position = (Vector3){ 0.0f, 2000.0f, 0.0f },  // Higher if needed,
        .target = (Vector3){ 0.0f, 0.0f, 0.0f },  // Centered
        .up = (Vector3){ 0.0f, 1.0f, 0.0f },
        .fovy = 80.0f,
        .projection = CAMERA_PERSPECTIVE
    };
    DisableCursor();
    Camera skyCam = camera;
    skyCam.position = (Vector3){ 0, 0, 0 };
    skyCam.target = (Vector3){ 0, 0, 1 };  // looking forward
    skyCam.up = (Vector3){0, 1, 0};
    skyCam.fovy = 60.0f;
    skyCam.projection = CAMERA_PERSPECTIVE;

    float pitch = 0.0f;
    float yaw = 0.0f;  // Face toward -Z (the terrain is likely laid out in +X, +Z space)

    while (!WindowShouldClose()) {
        bool reportOn = false;
        int treeTriCount = 0;
        int treeBcCount = 0;
        int chunkTriCount = 0;
        int chunkBcCount = 0;
        int totalTriCount = 0;
        int totalBcCount = 0;
        float dt = GetFrameTime();

        //check chunks for anyone that needs mesh upload again
        for (int cy = 0; cy < CHUNK_COUNT; cy++) {
            for (int cx = 0; cx < CHUNK_COUNT; cx++) {
                if(chunks[cx][cy].isTextureReady && !chunks[cx][cy].isTextureLoaded)
                {
                    Texture2D texture = LoadTextureFromImage(chunks[cx][cy].img_tex); //using slope and color avg right now
                    Texture2D textureBig = LoadTextureFromImage(chunks[cx][cy].img_tex_big);
                    Texture2D textureFull = LoadTextureFromImage(chunks[cx][cy].img_tex_full);
                    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
                    SetTextureWrap(textureBig, TEXTURE_WRAP_CLAMP);
                    SetTextureWrap(textureFull, TEXTURE_WRAP_CLAMP);
                    GenTextureMipmaps(&textureFull);  // <-- this generates mipmaps
                    SetTextureFilter(textureFull, TEXTURE_FILTER_TRILINEAR); // use a better filter
                    GenTextureMipmaps(&textureBig);  // <-- this generates mipmaps
                    SetTextureFilter(textureBig, TEXTURE_FILTER_TRILINEAR); // use a better filter
                    GenTextureMipmaps(&texture);  // <-- this generates mipmaps
                    SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR); // use a better filter
                    chunks[cx][cy].texture = texture;  // Copy contents
                    chunks[cx][cy].textureBig = textureBig;
                    chunks[cx][cy].textureFull = textureFull;
                    chunks[cx][cy].isTextureLoaded = true;
                }
                if (chunks[cx][cy].isTextureLoaded && chunks[cx][cy].isReady && !chunks[cx][cy].isLoaded) {
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

                    // Setup bounding box
                    chunks[cx][cy].origBox = ScaleBoundingBox(GetModelBoundingBox(chunks[cx][cy].model), (Vector3){MAP_SCALE, MAP_SCALE, MAP_SCALE});
                    chunks[cx][cy].box = UpdateBoundingBox(chunks[cx][cy].origBox, chunks[cx][cy].center);

                    chunks[cx][cy].isLoaded = true;
                    TraceLog(LOG_INFO, "loaded chunk model -> %d,%d", cx, cy);
                }
            }
        }


        FindClosestChunkAndAssignLod(&camera); //Im not sure If I need this twice here?


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
            if (onLoad && !FindNextTreeInChunk(&camera, closestCX, closestCY, 15.0f)) {
                TraceLog(LOG_INFO, "No suitable tree found in current chunk.");
                if(!FindAnyTreeInWorld(&camera, 15.0f))
                {
                    TraceLog(LOG_INFO, "Tree Error, cant find a tree, time to hug a tree hehe, and blow a whistle");
                }
            }
        }
        //map input
        bool goku = false;
        if (IsKeyPressed(KEY_M)) showMap = !showMap; // Toggle map
        if (IsKeyDown(KEY_EQUAL)) mapZoom += 0.01f;  // Zoom in (+ key)
        if (IsKeyDown(KEY_MINUS)) mapZoom -= 0.01f;  // Zoom out (- key)
        mapZoom = Clamp(mapZoom, 0.5f, 4.0f);
        //end map input
        if (IsKeyDown(KEY_B)) {displayBoxes = !displayBoxes;}
        if (IsKeyDown(KEY_F12)) {TakeScreenshotWithTimestamp();}
        if (IsKeyDown(KEY_F11)) {reportOn = true;}
        //if (IsKeyDown(KEY_M)) {DisableCursor();}
        if (IsKeyDown(KEY_PAGE_UP)) {chosenX = (chosenX+1)%CHUNK_COUNT;}
        if (IsKeyDown(KEY_PAGE_DOWN)) {chosenY = (chosenY+1)%CHUNK_COUNT;}
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {move = Vector3Add(move, forward);goku = true;TraceLog(LOG_INFO, " --> Instant Transmission -->");};
        if (IsKeyDown(KEY_W)) move = Vector3Add(move, forward);
        if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, forward);
        if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);
        if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);
        if (IsKeyDown(KEY_LEFT_SHIFT)) move.y -= (1.0f * MAP_SCALE);
        if (IsKeyDown(KEY_SPACE)) move.y += (1.0f * MAP_SCALE);
        if (IsKeyDown(KEY_ENTER)) {chunks[chosenX][chosenY].curTreeIdx=0;closestCX=chosenX;closestCY=chosenY;camera.position.x=chunks[closestCX][closestCY].center.x;camera.position.z=chunks[closestCX][closestCY].center.z;}

        if (Vector3Length(move) > 0.01f) {
            move = Vector3Normalize(move);
            move = Vector3Scale(move, goku?GOKU_DASH_DIST:MOVE_SPEED * dt);
            camera.position = Vector3Add(camera.position, move);
        }
        camera.target = Vector3Add(camera.position, forward);
        skyCam.target = Vector3Add(skyCam.position, forward);
        FindClosestChunkAndAssignLod(&camera);//this one is definetley needed
        if(onLoad)//simple collision if we have loaded the map
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

        BeginDrawing();
        ClearBackground(SKYBLUE);
        //skybox separate scene
        BeginMode3D(skyCam);
            if(true)
            {
                //skybox stuff
                rlDisableDepthMask();
                Vector3 cam = skyCam.position;
                float dist = 60.0f;
                float size = 120.0f;
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
            //rlDisableBackfaceCulling();
            bool loadedEem = true;
            int loadCnt = 0;
            //get frustum
            Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
            Matrix proj = MatrixPerspective(DEG2RAD * camera.fovy, SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 512.0f);
            Matrix vp = MatrixMultiply(view, proj);
            Frustum frustum = ExtractFrustum(vp);
            for (int cy = 0; cy < CHUNK_COUNT; cy++) {
                for (int cx = 0; cx < CHUNK_COUNT; cx++) {
                    if(chunks[cx][cy].isLoaded)
                    {
                        loadCnt++;
                        //REM: I moved frustum culling onto just the 8models and it seems to work
                        //if(onLoad && !IsBoxInFrustum(chunks[cx][cy].box, frustum)){continue;}
                        //if(onLoad && (cx!=closestCX||cy!=closestCY) && !ShouldRenderChunk(chunks[cx][cy].center,camera)){continue;}
                        //TraceLog(LOG_INFO, "drawing chunk: %d,%d", cx, cy);
                        Vector3 pos = chunks[cx][cy].position;
                        if(chunks[cx][cy].lod == LOD_64) 
                        {
                            chunkBcCount++;
                            chunkTriCount+=chunks[cx][cy].model.meshes[0].triangleCount;
                            DrawModel(chunks[cx][cy].model, pos, MAP_SCALE, WHITE);
                            if(onLoad)//only once we have fully loaded everything
                            {
                                for(int pInd = 0; pInd<chunks[cx][cy].treeCount; pInd++)
                                {
                                    BoundingBox tob = UpdateBoundingBox(treeOrigBox,chunks[cx][cy].treePositions[pInd]);
                                    if(!IsBoxInFrustum(tob, frustum)){continue;}
                                    //TraceLog(LOG_INFO, "Drawing (%d,%d) tree at %.2f %.2f %.2f", cx, cy, chunks[cx][cy].treePositions[pInd].x, chunks[cx][cy].treePositions[pInd].y, chunks[cx][cy].treePositions[pInd].z);
                                    if(USE_TREE_CUBES)
                                    {
                                        DrawModelEx(treeCubeModel, chunks[cx][cy].treePositions[pInd], (Vector3){0, 1, 0}, 0.0f, (Vector3){1, 1, 1}, DARKGREEN);
                                    }
                                    else
                                    {
                                        bool close = Vector3Distance(chunks[cx][cy].treePositions[pInd],camera.position) < FULL_TREE_DIST;
                                        Model tree3 = close ? treeModel : bgTreeModel;
                                        if(reportOn){treeTriCount+=tree3.meshes[0].triangleCount;treeBcCount++;}
                                        DrawModel(tree3, chunks[cx][cy].treePositions[pInd], 1.0f, WHITE);
                                    }
                                    if(displayBoxes){DrawBoundingBox(tob,RED);}
                                }
                            }
                        }
                        else if(chunks[cx][cy].lod == LOD_32) {
                            chunkBcCount++;
                            chunkTriCount+=chunks[cx][cy].model32.meshes[0].triangleCount;
                            DrawModel(chunks[cx][cy].model32, pos, MAP_SCALE, WHITE);
                        }
                        else if(chunks[cx][cy].lod == LOD_16) {
                            chunkBcCount++;
                            chunkTriCount+=chunks[cx][cy].model16.meshes[0].triangleCount;
                            DrawModel(chunks[cx][cy].model16, pos, MAP_SCALE, WHITE);
                        }
                        else if(IsBoxInFrustum(chunks[cx][cy].box, frustum)||!onLoad) {
                            chunkBcCount++;
                            chunkTriCount+=chunks[cx][cy].model8.meshes[0].triangleCount;
                            DrawModel(chunks[cx][cy].model8, pos, MAP_SCALE, WHITE);
                        }
                        if(displayBoxes){DrawBoundingBox(chunks[cx][cy].box,YELLOW);}
                    }
                    else {loadedEem = false;}
                }
            }
            if(reportOn) //triangle report
            {
                totalBcCount = chunkBcCount + treeBcCount;
                totalTriCount = chunkTriCount + treeTriCount;
                printf("Estimated tree triangles this frame  :  %d\n", treeTriCount);
                printf("Estimated batch calls for trees      :  %d\n", treeBcCount);
                printf("Estimated chunk triangles this frame :  %d\n", chunkTriCount);
                printf("Estimated batch calls for chunks     :  %d\n", chunkBcCount);
                printf("Estimated TOTAL triangles this frame :  %d\n", totalTriCount);
                printf("Estimated TOTAL batch calls          :  %d\n", totalBcCount);
                printf("Current FPS (so you can document)    :  %d\n", GetFPS());
            }
            //rlEnableBackfaceCulling();
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
        if(!loadedEem)
        {
            // Outline
            DrawRectangleLines(500, 350, 204, 10, DARKGRAY);
            // Fill
            int gc = (int)(loadCnt/256.0f*255);
            DrawRectangle(502, 352, (int)((200 - 4) * (loadCnt/256.0f)), 10 - 4, (Color){100,gc,40,255});
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
                free(chunks[cx][cy].treePositions);
                chunks[cx][cy].treePositions = NULL;
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
