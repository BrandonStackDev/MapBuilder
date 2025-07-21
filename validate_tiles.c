#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "models.h"
#include <stdio.h>
#include <dirent.h>
#include <string.h>

#define CHUNK_COUNT 16
#define TILE_COUNT 16
//#define TILE_SIZE 64
//#define TILE_TYPES 2  // e.g. "rock", "tree"

// validation function
void ValidateTiles(int cx, int cy)
{
    for (int tx = 0; tx < TILE_COUNT; tx++) {
        for (int ty = 0; ty < TILE_COUNT; ty++) {
            for (int i=0; i < MODEL_TOTAL_COUNT; i++)
            {
                char path[256];
                snprintf(path, sizeof(path),
                        "map/chunk_%02d_%02d/tile_64/%02d_%02d/tile_%s_64.obj",
                        cx, cy, tx, ty, GetModelName(i));

                FILE *f = fopen(path, "r");
                if (f) {
                    fclose(f);
                    Model model = LoadModel(path);
                    if(!IsModelValid(model))
                    {
                        printf("tile not valid => chunk[%d,%d]->Tile[%d,%d] -- %s", cx, cy, tx, ty, GetModelName(i));
                    }
                    if (model.meshCount > 0) {
                        Mesh m = model.meshes[0];
                        //printf("vertices: %d, triangles: %d\n", m.vertexCount, m.triangleCount);
                        if (m.vertexCount == 0 || m.triangleCount == 0) {
                            printf("tile is empty: chunk[%d,%d] tile[%d,%d] model: %s\n",
                                cx, cy, tx, ty, GetModelName(i));
                        }
                    }
                    else
                    {
                        printf("tile has no mesh => chunk[%d,%d]->Tile[%d,%d] -- %s", cx, cy, tx, ty, GetModelName(i));
                    }

                    UnloadModel(model);
                }
            }
        }
    }
}

int main(void) {
    // Required by some mesh/model functions
    SetConfigFlags(FLAG_WINDOW_HIDDEN); // Don't open a visible window
    SetTraceLogLevel(LOG_NONE);
    InitWindow(10,10,"validating tiles");
    for (int cy = 0; cy < CHUNK_COUNT; cy++) {
        for (int cx = 0; cx < CHUNK_COUNT; cx++) {
            ValidateTiles(cx,cy);
        }
    }
    CloseWindow();  // Clean up (even if no window was shown)
    return 0;
}
