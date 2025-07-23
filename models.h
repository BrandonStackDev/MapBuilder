#ifndef MODELS_H
#define MODELS_H

#include "raylib.h"
#include <stdlib.h>

#define MAX_PROPS_ALLOWED 1024
#define MAX_PROPS_UPPER_BOUND (MAX_PROPS_ALLOWED * 2)  // safety cap

// Enum for all models used in the game
typedef enum {
    MODEL_NONE = -1,
    MODEL_TREE,
    MODEL_ROCK,
    MODEL_TOTAL_COUNT  // Always keep last to get number of models
} Model_Type;

typedef enum {
    BIOME_NONE = -1,
    BIOME_FOREST,
    BIOME_GRASSLAND,
    BIOME_MOUNTAIN,
    BIOME_TOTAL_COUNT
} Biome_Type;

typedef struct {
    Model_Type type;
    Vector3 pos;
} StaticGameObject;

// Optional: Array of model names, useful for debugging or file loading
static const char *ModelNames[MODEL_TOTAL_COUNT] = {
    "tree",
    "rock",
};

static const char *ModelPaths[MODEL_TOTAL_COUNT] = {
    "models/tree_bg.glb",
    "models/rock1.glb",
};

static const char *ModelPathsFull[MODEL_TOTAL_COUNT] = {
    "models/tree.glb",
    "models/rock1.glb",
};

static const char *ModelPathsFullTextures[MODEL_TOTAL_COUNT] = {
    "textures/tree_skin_small.png",
    "textures/rock1.png",
};

Model StaticObjectModels[MODEL_TOTAL_COUNT];
Model HighFiStaticObjectModels[MODEL_TOTAL_COUNT];
Texture HighFiStaticObjectModelTextures[MODEL_TOTAL_COUNT];
Material HighFiStaticObjectMaterials[MODEL_TOTAL_COUNT];
Matrix HighFiTransforms[MODEL_TOTAL_COUNT][MAX_PROPS_UPPER_BOUND];//meant to be set per draw loop, and then completely overwritten, dynamic so over estimate and test

// Optional: Utility function (only if you want it in header)
static inline const char *GetModelName(Model_Type model) {
    if (model >= 0 && model < MODEL_TOTAL_COUNT)
        return ModelNames[model];
    return "none";
}

//////////////////////////////////////////////DEFINE MODELS FROM PERLIN COLOR NOISE//////////////////////////////////////////////////////
#define TREE_MATCH_DISTANCE_SQ 200  // ±14 RGB range
#define ROCK_MATCH_DISTANCE_SQ 300
Color targetTree = (Color){34, 139, 34};   // Forest green
Color targetRock = (Color){128, 128, 128}; // Mid gray
// const Model_Type forestProps[] = { MODEL_TREE, MODEL_ROCK }; //probably just remove these
// const Model_Type mountainProps[] = { MODEL_ROCK };
// const Model_Type grasslandProps[] = { MODEL_TREE };


int ColorDistanceSquared(Color a, Color b) {
    return (a.r - b.r)*(a.r - b.r) + (a.g - b.g)*(a.g - b.g) + (a.b - b.b)*(a.b - b.b);
}

Model_Type GetRandomModelForBiome(Biome_Type biome) {
    switch (biome) {
        case BIOME_FOREST: {
            const Model_Type props[] = { MODEL_TREE, MODEL_ROCK };
            return props[rand() % 2];
        }
        case BIOME_GRASSLAND: return MODEL_TREE;
        case BIOME_MOUNTAIN: return MODEL_ROCK;
        default: return MODEL_NONE;
    }
}

Biome_Type GetBiomeFromColor(Color c) {
    if (ColorDistanceSquared(c, (Color){34,139,34}) < 8000) return BIOME_FOREST;
    if (ColorDistanceSquared(c, (Color){150,150,150}) < 8000) return BIOME_MOUNTAIN;
    if (ColorDistanceSquared(c, (Color){120,200,120}) < 8000) return BIOME_GRASSLAND;
    return BIOME_NONE;
}

Model_Type GetModelTypeFromColor(Color c, float heightEst) {
    //todo: if height estimate is above something, probably snow. heightEst
    Biome_Type biome = GetBiomeFromColor(c);
    return GetRandomModelForBiome(biome);
}

// Model_Type GetModelTypeFromColor(Color c, float heightEst) {
//     //todo: if height estimate is above something, probably snow. heightEst
//     int distTree = ColorDistanceSquared(c, targetTree);
//     int distRock = ColorDistanceSquared(c, targetRock);

//     int threshold = 8000;  // adjust for fuzziness

//     if (distTree < threshold && distTree < distRock) return MODEL_TREE;
//     if (distRock < threshold && distRock < distTree) return MODEL_ROCK;

//     return MODEL_NONE;
// }

// bool IsForestGreen(Color c) {
//     return ColorDistanceSquared(c, 34, 139, 34) < TREE_MATCH_DISTANCE_SQ;
// }

// bool IsRockGray(Color c) {
//     return ColorDistanceSquared(c, 100, 100, 100) < ROCK_MATCH_DISTANCE_SQ;
// }

// Model_Type GetModelTypeFromColor(Color c) {
//     if (IsForestGreen(c)) return MODEL_TREE;
//     if (IsRockGray(c)) return MODEL_ROCK;
//     return MODEL_NONE;
// }
//////////////////////////////////////////////DEFINE MODELS FROM PERLIN COLOR NOISE//////////////////////////////////////////////////////
// // Match against specific RGB values -- keeping this incase I want to go back to this styl of map, although it doesnt work very well
// Model_Type GetModelTypeFromColor(Color c) {
//     if (c.g > 216)  // forest green = tree
//         return MODEL_TREE;
//     else if (c.r > 216 && c.b > 128) // gray = rock
//         return MODEL_ROCK;
//     else
//         return MODEL_NONE;
// }

void InitStaticGameProps(Shader shader)
{
    for(int i =0; i < MODEL_TOTAL_COUNT; i++)
    {
        StaticObjectModels[i] = LoadModel(ModelPaths[i]);
        HighFiStaticObjectModels[i] = LoadModel(ModelPathsFull[i]);
        HighFiStaticObjectModelTextures[i] = LoadTexture(ModelPathsFullTextures[i]);
        HighFiStaticObjectMaterials[i] = LoadMaterialDefault();
        HighFiStaticObjectMaterials[i].shader = shader;
        HighFiStaticObjectMaterials[i].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
        HighFiStaticObjectMaterials[i].maps[MATERIAL_MAP_DIFFUSE].texture = HighFiStaticObjectModelTextures[i];
        //see if this hack is needed for that tree and rock model, these glb files might put the material at index 1
        HighFiStaticObjectModels[i].materials[1]=LoadMaterialDefault();
        HighFiStaticObjectModels[i].materials[0]=HighFiStaticObjectMaterials[i];
        HighFiStaticObjectModels[i].materialCount = 1;
    }
}

#endif // MODELS_H
