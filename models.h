#ifndef MODELS_H
#define MODELS_H

#include "raylib.h"

#define MAX_PROPS_ALLOWED 512
#define MAX_PROPS_UPPER_BOUND (MAX_PROPS_ALLOWED * 2)  // safety cap

// Enum for all models used in the game
typedef enum {
    MODEL_NONE = -1,
    MODEL_TREE,
    MODEL_ROCK,
    MODEL_TOTAL_COUNT  // Always keep last to get number of models
} Model_Type;

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

Model StaticObjectModels[MODEL_TOTAL_COUNT];

// Optional: Utility function (only if you want it in header)
static inline const char *GetModelName(Model_Type model) {
    if (model >= 0 && model < MODEL_TOTAL_COUNT)
        return ModelNames[model];
    return "none";
}

// Match against specific RGB values
Model_Type GetModelTypeFromColor(Color c) {
    if (c.r > 34 && c.g > 139 && c.b > 34)  // forest green = tree
        return MODEL_TREE;
    else if (c.r < 128 && c.g < 128 && c.b < 128) // gray = rock
        return MODEL_ROCK;
    else
        return MODEL_NONE;
}

void InitStaticGameProps()
{
    for(int i =0; i < MODEL_TOTAL_COUNT; i++)
    {
        StaticObjectModels[i] = LoadModel(ModelPaths[i]);
    }
}

#endif // MODELS_H
