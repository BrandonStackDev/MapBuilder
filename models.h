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
    MODEL_TREE_2,
    MODEL_TREE_3,
    MODEL_TREE_4,
    MODEL_TREE_DEAD_01,
    MODEL_TREE_DEAD_02,
    MODEL_TREE_DEAD_03,
    MODEL_TREE_PINE,
    MODEL_ROCK,
    MODEL_ROCK2,
    MODEL_ROCK3,
    MODEL_ROCK4,
    MODEL_ROCK5,
    MODEL_GRASS,
    MODEL_GRASS_LARGE,
    MODEL_GRASS_THICK,
    MODEL_TOTAL_COUNT  // Always keep last to get number of models
} Model_Type;

typedef enum {
    BIOME_NONE = -1,
    BIOME_FOREST,
    BIOME_FOREST_DEAD,
    BIOME_FOREST_PINE,
    BIOME_FOREST_ECCLECTIC,
    BIOME_GRASSLAND_SIMPLE,
    BIOME_GRASSLAND_FULL,
    BIOME_MOUNTAIN_1,
    BIOME_MOUNTAIN_2,
    BIOME_TOTAL_COUNT
} Biome_Type;

typedef struct {
    Model_Type type;
    Vector3 pos;
    float yaw, pitch, roll, scale;
    //below this line, only use in preview.c
    BoundingBox origBox, box;
} StaticGameObject;

// Optional: Array of model names, useful for debugging or file loading
static const char *ModelNames[MODEL_TOTAL_COUNT] = {
    "tree",
    "tree2",
    "tree3",
    "tree4",
    "tree_dead",
    "tree_dead2",
    "tree_dead3",
    "tree_pine",
    "rock",
    "rock2",
    "rock3",
    "rock4",
    "rock5",
    "grass",
    "grass_large",
    "grass_thick",
};

static const char *ModelPaths[MODEL_TOTAL_COUNT] = {
    "models/tree_bg.glb",
    "models/tree_02.obj",
    "models/tree_03.obj",
    "models/tree_04.obj",
    "models/tree_dead_01.obj",
    "models/tree_dead_02.obj",
    "models/tree_dead_03.obj",
    "models/tree_pine_01.obj",
    "models/rock1.glb",
    "models/rock2.obj",
    "models/rock3.obj",
    "models/rock4.obj",
    "models/rock5.obj",
    "models/grass.obj",
    "models/grass_large.obj",
    "models/grass_thick.obj",
};

static const char *ModelPathsFull[MODEL_TOTAL_COUNT] = {
    "models/tree.glb",
    "models/tree_02.obj",
    "models/tree_03.obj",
    "models/tree_04.obj",
    "models/tree_dead_01.obj",
    "models/tree_dead_02.obj",
    "models/tree_dead_03.obj",
    "models/tree_pine_01.obj",
    "models/rock1.glb",
    "models/rock2.obj",
    "models/rock3.obj",
    "models/rock4.obj",
    "models/rock5.obj",
    "models/grass.obj",
    "models/grass_large.obj",
    "models/grass_thick.obj",
};

static const char *ModelPathsFullTextures[MODEL_TOTAL_COUNT] = {
    "textures/tree_skin_small.png",
    "textures/tree_02.png",
    "textures/tree_03.png",
    "textures/tree_04.png",
    "textures/tree_dead_01.png",
    "textures/tree_dead_02.png",
    "textures/tree_dead_03.png",
    "textures/tree_pine_01.png",
    "textures/rock1.png",
    "textures/rock1.png",
    "textures/rock1.png",
    "textures/rock2.png",
    "textures/rock2.png",
    "textures/grass.png",
    "textures/grass.png",
    "textures/grass_yellow.png",
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


int ColorDistanceSquared(Color a, Color b) {
    return (a.r - b.r)*(a.r - b.r) + (a.g - b.g)*(a.g - b.g) + (a.b - b.b)*(a.b - b.b);
}

Model_Type GetRandomModelForBiome(Biome_Type biome) {
    switch (biome) {
        case BIOME_FOREST: {
            const Model_Type props[] = { MODEL_GRASS, MODEL_GRASS_THICK, MODEL_GRASS_THICK,MODEL_GRASS_LARGE, MODEL_TREE, MODEL_TREE,  MODEL_TREE_2, MODEL_ROCK2, MODEL_ROCK3 };
            return props[rand() % 9];
        }
        case BIOME_FOREST_DEAD: {
            const Model_Type props[] = { MODEL_GRASS, MODEL_GRASS_LARGE, MODEL_TREE_DEAD_01, MODEL_TREE_DEAD_02, MODEL_TREE_DEAD_03, MODEL_ROCK, MODEL_ROCK4 };
            return props[rand() % 7];
        }
        case BIOME_FOREST_PINE: {
            const Model_Type props[] = { MODEL_GRASS, MODEL_TREE_PINE, MODEL_GRASS, MODEL_TREE_PINE, MODEL_ROCK5 };
            return props[rand() % 5];
        }
        case BIOME_FOREST_ECCLECTIC: {
            const Model_Type props[] = { MODEL_TREE,    MODEL_TREE_2,    MODEL_TREE_3,    MODEL_TREE_4,    MODEL_TREE_DEAD_01,    MODEL_TREE_DEAD_02,    MODEL_TREE_DEAD_03,    MODEL_TREE_PINE,    MODEL_ROCK,    MODEL_ROCK2,    MODEL_ROCK3,    MODEL_ROCK4,    MODEL_ROCK5,    MODEL_GRASS,    MODEL_GRASS_LARGE,    MODEL_GRASS_THICK};
            return props[rand() % 16];
        }
        case BIOME_GRASSLAND_SIMPLE: {
            const Model_Type props[] = { MODEL_GRASS, MODEL_GRASS_THICK, MODEL_GRASS_LARGE, MODEL_GRASS, MODEL_GRASS_THICK, MODEL_GRASS_LARGE, MODEL_ROCK };
            return props[rand() % 7];
        }
        case BIOME_GRASSLAND_FULL: {
            const Model_Type props[] = { MODEL_GRASS_THICK, MODEL_GRASS_LARGE };
            return props[rand() % 2];
        }
        case BIOME_MOUNTAIN_1: {
            const Model_Type props[] = { MODEL_GRASS, MODEL_GRASS, MODEL_GRASS, MODEL_ROCK2, MODEL_ROCK3 ,MODEL_ROCK4 ,MODEL_ROCK5 ,MODEL_ROCK};
            return props[rand() % 8];
        }
        case BIOME_MOUNTAIN_2: {
            const Model_Type props[] = { MODEL_GRASS, MODEL_GRASS, MODEL_GRASS_THICK, MODEL_ROCK, MODEL_TREE_DEAD_01, MODEL_TREE_3 };
            return props[rand() % 6];
        }
        default: return MODEL_NONE;
    }
}

Biome_Type GetBiomeFromColor(Color c) {
    if (ColorDistanceSquared(c, (Color){ 34,139, 34 }) < 8000) {return BIOME_FOREST;}
    if (ColorDistanceSquared(c, (Color){120,200,120 }) < 8000) {return BIOME_GRASSLAND_SIMPLE;}
    if (ColorDistanceSquared(c, (Color){130,210,130}) < 7000) {return BIOME_GRASSLAND_FULL;}
    if (ColorDistanceSquared(c, (Color){80,100, 60 }) < 7000) {return BIOME_FOREST_DEAD;}
    if (ColorDistanceSquared(c, (Color){ 40,120, 40 }) < 6000) {return BIOME_FOREST_ECCLECTIC;}
    if (ColorDistanceSquared(c, (Color){10, 90, 40}) < 6000) {return BIOME_FOREST_PINE;}
    if (ColorDistanceSquared(c, (Color){150,150,150 }) < 5000) {return BIOME_MOUNTAIN_1;}
    if (ColorDistanceSquared(c, (Color){120,120,120 }) < 5000) {return BIOME_MOUNTAIN_2;}
    
    return BIOME_NONE;
}

Model_Type GetModelTypeFromColor(Color c, float heightEst) {
    //todo: if height estimate is above something, probably snow. heightEst
    Biome_Type biome = GetBiomeFromColor(c);
    return GetRandomModelForBiome(biome);
}

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
        // HighFiStaticObjectModels[i].materials[1]=LoadMaterialDefault();
        // HighFiStaticObjectModels[i].materials[0]=HighFiStaticObjectMaterials[i];
        // HighFiStaticObjectModels[i].materialCount = 1;
    }
}

#endif // MODELS_H
