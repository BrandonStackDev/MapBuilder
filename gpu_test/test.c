#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_INSTANCES 100
#define MAX_MATERIAL_MAPS 7
#define MAX_MESH_VERTEX_BUFFERS 7

Matrix instanceTransforms[MAX_INSTANCES];
GLuint instanceVBO = 0;

// Helper: upload instance transforms
void UploadInstanceBuffer() {
    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Matrix) * MAX_INSTANCES, instanceTransforms, GL_STATIC_DRAW);

    // Setup 4 vec4 attributes for the mat4 (one per column)
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(3 + i);
        glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Matrix), (const void *)(sizeof(Vector4) * i));
        glVertexAttribDivisor(3 + i, 1);  // Advance per-instance
    }
}

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

void DrawMeshCustomGpt(Mesh mesh, Material material, Matrix transform, Camera3D camera, int instanceCount) {
    Matrix view = GetCameraMatrix(camera);
    Matrix proj = MatrixPerspective(45.0f * DEG2RAD, (float)GetScreenWidth()/GetScreenHeight(), 0.1f, 1000.0f);
    Matrix model = transform;
    Matrix mvp = MatrixMultiply(MatrixMultiply(model, view), proj);

    rlEnableShader(material.shader.id);
    rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], mvp);
    rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MODEL], model);

    rlEnableTexture(material.maps[MATERIAL_MAP_DIFFUSE].texture.id);
    rlEnableVertexArray(mesh.vaoId);

    glDrawElementsInstanced(GL_TRIANGLES, mesh.triangleCount * 3, GL_UNSIGNED_SHORT, 0, instanceCount);

    rlDisableVertexArray();
    rlDisableTexture();
    rlDisableShader();
}


void DrawMeshCustom(Mesh mesh, Material material, Matrix transform)
{
#if defined(GRAPHICS_API_OPENGL_11)
    #define GL_VERTEX_ARRAY         0x8074
    #define GL_NORMAL_ARRAY         0x8075
    #define GL_COLOR_ARRAY          0x8076
    #define GL_TEXTURE_COORD_ARRAY  0x8078

    rlEnableTexture(material.maps[MATERIAL_MAP_DIFFUSE].texture.id);

    if (mesh.animVertices) rlEnableStatePointer(GL_VERTEX_ARRAY, mesh.animVertices);
    else rlEnableStatePointer(GL_VERTEX_ARRAY, mesh.vertices);

    rlEnableStatePointer(GL_TEXTURE_COORD_ARRAY, mesh.texcoords);
    if (mesh.normals) rlEnableStatePointer(GL_VERTEX_ARRAY, mesh.animNormals);
    else rlEnableStatePointer(GL_NORMAL_ARRAY, mesh.normals);

    rlEnableStatePointer(GL_COLOR_ARRAY, mesh.colors);

    rlPushMatrix();
        rlMultMatrixf(MatrixToFloat(transform));
        rlColor4ub(material.maps[MATERIAL_MAP_DIFFUSE].color.r,
                   material.maps[MATERIAL_MAP_DIFFUSE].color.g,
                   material.maps[MATERIAL_MAP_DIFFUSE].color.b,
                   material.maps[MATERIAL_MAP_DIFFUSE].color.a);

        if (mesh.indices != NULL) rlDrawVertexArrayElements(0, mesh.triangleCount*3, mesh.indices);
        else rlDrawVertexArray(0, mesh.vertexCount);
    rlPopMatrix();

    rlDisableStatePointer(GL_VERTEX_ARRAY);
    rlDisableStatePointer(GL_TEXTURE_COORD_ARRAY);
    rlDisableStatePointer(GL_NORMAL_ARRAY);
    rlDisableStatePointer(GL_COLOR_ARRAY);

    rlDisableTexture();
#endif

#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    // Bind shader program
    //rlEnableShader(material.shader.id);

    // Send required data to shader (matrices, values)
    //-----------------------------------------------------
    // Upload to shader material.colDiffuse
    if (material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] != -1)
    {
        float values[4] = {
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.r/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.g/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.b/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.a/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_DIFFUSE], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Upload to shader material.colSpecular (if location available)
    if (material.shader.locs[SHADER_LOC_COLOR_SPECULAR] != -1)
    {
        float values[4] = {
            (float)material.maps[MATERIAL_MAP_SPECULAR].color.r/255.0f,
            (float)material.maps[MATERIAL_MAP_SPECULAR].color.g/255.0f,
            (float)material.maps[MATERIAL_MAP_SPECULAR].color.b/255.0f,
            (float)material.maps[MATERIAL_MAP_SPECULAR].color.a/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_SPECULAR], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Get a copy of current matrices to work with,
    // just in case stereo render is required, and we need to modify them
    // NOTE: At this point the modelview matrix just contains the view matrix (camera)
    // That's because BeginMode3D() sets it and there is no model-drawing function
    // that modifies it, all use rlPushMatrix() and rlPopMatrix()
    Matrix matModel = MatrixIdentity();
    Matrix matView = rlGetMatrixModelview();
    Matrix matModelView = MatrixIdentity();
    Matrix matProjection = rlGetMatrixProjection();

    // Upload view and projection matrices (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_VIEW] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_VIEW], matView);
    if (material.shader.locs[SHADER_LOC_MATRIX_PROJECTION] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_PROJECTION], matProjection);

    // Accumulate several model transformations:
    //    transform: model transformation provided (includes DrawModel() params combined with model.transform)
    //    rlGetMatrixTransform(): rlgl internal transform matrix due to push/pop matrix stack
    matModel = MatrixMultiply(transform, rlGetMatrixTransform());

    // Model transformation matrix is sent to shader uniform location: SHADER_LOC_MATRIX_MODEL
    if (material.shader.locs[SHADER_LOC_MATRIX_MODEL] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MODEL], matModel);

    // Get model-view matrix
    matModelView = MatrixMultiply(matModel, matView);

    // Upload model normal matrix (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_NORMAL] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_NORMAL], MatrixTranspose(MatrixInvert(matModel)));

#ifdef RL_SUPPORT_MESH_GPU_SKINNING
    // Upload Bone Transforms
    if ((material.shader.locs[SHADER_LOC_BONE_MATRICES] != -1) && mesh.boneMatrices)
    {
        rlSetUniformMatrices(material.shader.locs[SHADER_LOC_BONE_MATRICES], mesh.boneMatrices, mesh.boneCount);
    }
#endif
    //-----------------------------------------------------

    // Bind active texture maps (if available)
    for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        if (material.maps[i].texture.id > 0)
        {
            // Select current shader texture slot
            rlActiveTextureSlot(i);

            // Enable texture for active slot
            if ((i == MATERIAL_MAP_IRRADIANCE) ||
                (i == MATERIAL_MAP_PREFILTER) ||
                (i == MATERIAL_MAP_CUBEMAP)) rlEnableTextureCubemap(material.maps[i].texture.id);
            else rlEnableTexture(material.maps[i].texture.id);

            rlSetUniform(material.shader.locs[SHADER_LOC_MAP_DIFFUSE + i], &i, SHADER_UNIFORM_INT, 1);
        }
    }

    // Try binding vertex array objects (VAO) or use VBOs if not possible
    // WARNING: UploadMesh() enables all vertex attributes available in mesh and sets default attribute values
    // for shader expected vertex attributes that are not provided by the mesh (i.e. colors)
    // This could be a dangerous approach because different meshes with different shaders can enable/disable some attributes
    if (!rlEnableVertexArray(mesh.vaoId))
    {
        // Bind mesh VBO data: vertex position (shader-location = 0)
        rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION], 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION]);

        // Bind mesh VBO data: vertex texcoords (shader-location = 1)
        rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01], 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]);

        if (material.shader.locs[SHADER_LOC_VERTEX_NORMAL] != -1)
        {
            // Bind mesh VBO data: vertex normals (shader-location = 2)
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL], 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL]);
        }

        // Bind mesh VBO data: vertex colors (shader-location = 3, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_COLOR] != -1)
        {
            if (mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0)
            {
                rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
                rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR], 4, RL_UNSIGNED_BYTE, 1, 0, 0);
                rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
            else
            {
                // Set default value for defined vertex attribute in shader but not provided by mesh
                // WARNING: It could result in GPU undefined behaviour
                float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                rlSetVertexAttributeDefault(material.shader.locs[SHADER_LOC_VERTEX_COLOR], value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
        }

        // Bind mesh VBO data: vertex tangents (shader-location = 4, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TANGENT] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT], 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT]);
        }

        // Bind mesh VBO data: vertex texcoords2 (shader-location = 5, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02], 2, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02]);
        }

#ifdef RL_SUPPORT_MESH_GPU_SKINNING
        // Bind mesh VBO data: vertex bone ids (shader-location = 6, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_BONEIDS] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEIDS]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEIDS], 4, RL_UNSIGNED_BYTE, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEIDS]);
        }

        // Bind mesh VBO data: vertex bone weights (shader-location = 7, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS], 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS]);
        }
#endif

        if (mesh.indices != NULL) rlEnableVertexBufferElement(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
    }

    int eyeCount = 1;
    if (rlIsStereoRenderEnabled()) eyeCount = 2;

    for (int eye = 0; eye < eyeCount; eye++)
    {
        // Calculate model-view-projection matrix (MVP)
        Matrix matModelViewProjection = MatrixIdentity();
        if (eyeCount == 1) matModelViewProjection = MatrixMultiply(matModelView, matProjection);
        else
        {
            // Setup current eye viewport (half screen width)
            rlViewport(eye*rlGetFramebufferWidth()/2, 0, rlGetFramebufferWidth()/2, rlGetFramebufferHeight());
            matModelViewProjection = MatrixMultiply(MatrixMultiply(matModelView, rlGetMatrixViewOffsetStereo(eye)), rlGetMatrixProjectionStereo(eye));
        }

        // Send combined model-view-projection matrix to shader
        rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matModelViewProjection);

        // Draw mesh
        if (mesh.indices != NULL) rlDrawVertexArrayElements(0, mesh.triangleCount*3, 0);
        else rlDrawVertexArray(0, mesh.vertexCount);
    }

    // Unbind all bound texture maps
    for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        if (material.maps[i].texture.id > 0)
        {
            // Select current shader texture slot
            rlActiveTextureSlot(i);

            // Disable texture for active slot
            if ((i == MATERIAL_MAP_IRRADIANCE) ||
                (i == MATERIAL_MAP_PREFILTER) ||
                (i == MATERIAL_MAP_CUBEMAP)) rlDisableTextureCubemap();
            else rlDisableTexture();
        }
    }

    // Disable all possible vertex array objects (or VBOs)
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Disable shader program
    rlDisableShader();

    // Restore rlgl internal modelview and projection matrices
    rlSetMatrixModelview(matView);
    rlSetMatrixProjection(matProjection);
#endif
}

//////----little helper function
void FillInstanceTransforms(Matrix *transforms, int count) {
    int gridSize = (int)sqrtf(count);
    float spacing = 10.0f;

    for (int i = 0; i < count; i++) {
        int x = i % gridSize;
        int z = i / gridSize;

        Vector3 position = (Vector3){ x * spacing, 0.0f, z * spacing };

        // // Optional: randomize rotation or scale
        // float angle = (float)(i % 360);
        // Vector3 axis = (Vector3){ 0, 1, 0 };

        Matrix translation = MatrixTranslate(position.x, position.y, position.z);
        //Matrix rotation = MatrixRotate(axis, angle * DEG2RAD);
        //Matrix scale = MatrixScale(1.0f, 1.0f, 1.0f);

        transforms[i] = translation;
    }
} // --<-->


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Draw multiple mesh instances with material and different transforms
void DrawMeshInstancedCustom(Mesh mesh, Material material, const Matrix *transforms, int instances)
{
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    // Instancing required variables
    float16 *instanceTransforms = NULL;
    unsigned int instancesVboId = 0;

    // Bind shader program
    rlEnableShader(material.shader.id);

    // Send required data to shader (matrices, values)
    //-----------------------------------------------------
    // Upload to shader material.colDiffuse
    if (material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] != -1)
    {
        float values[4] = {
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.r/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.g/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.b/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.a/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_DIFFUSE], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Upload to shader material.colSpecular (if location available)
    if (material.shader.locs[SHADER_LOC_COLOR_SPECULAR] != -1)
    {
        float values[4] = {
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.r/255.0f,
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.g/255.0f,
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.b/255.0f,
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.a/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_SPECULAR], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Get a copy of current matrices to work with,
    // just in case stereo render is required, and we need to modify them
    // NOTE: At this point the modelview matrix just contains the view matrix (camera)
    // That's because BeginMode3D() sets it and there is no model-drawing function
    // that modifies it, all use rlPushMatrix() and rlPopMatrix()
    Matrix matModel = MatrixIdentity();
    Matrix matView = rlGetMatrixModelview();
    Matrix matModelView = MatrixIdentity();
    Matrix matProjection = rlGetMatrixProjection();

    // Upload view and projection matrices (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_VIEW] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_VIEW], matView);
    if (material.shader.locs[SHADER_LOC_MATRIX_PROJECTION] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_PROJECTION], matProjection);

    // Create instances buffer
    instanceTransforms = (float16 *)RL_MALLOC(instances*sizeof(float16));

    // Fill buffer with instances transformations as float16 arrays
    for (int i = 0; i < instances; i++) instanceTransforms[i] = MatrixToFloatV(transforms[i]);

    // Enable mesh VAO to attach new buffer
    rlEnableVertexArray(mesh.vaoId);

    // This could alternatively use a static VBO and either glMapBuffer() or glBufferSubData()
    // It isn't clear which would be reliably faster in all cases and on all platforms,
    // anecdotally glMapBuffer() seems very slow (syncs) while glBufferSubData() seems
    // no faster, since we're transferring all the transform matrices anyway
    instancesVboId = rlLoadVertexBuffer(instanceTransforms, instances*sizeof(float16), false);

    // Instances transformation matrices are sent to shader attribute location: SHADER_LOC_VERTEX_INSTANCE_TX
    for (unsigned int i = 0; i < 4; i++)
    {
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_INSTANCE_TX] + i);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_INSTANCE_TX] + i, 4, RL_FLOAT, 0, sizeof(Matrix), i*sizeof(Vector4));
        //rlSetVertexAttributeDivisor(material.shader.locs[SHADER_LOC_VERTEX_INSTANCE_TX] + i, 1);
        glVertexAttribDivisor(material.shader.locs[SHADER_LOC_VERTEX_INSTANCE_TX] + i, 1);
    }

    rlDisableVertexBuffer();
    rlDisableVertexArray();

    // Accumulate internal matrix transform (push/pop) and view matrix
    // NOTE: In this case, model instance transformation must be computed in the shader
    matModelView = MatrixMultiply(rlGetMatrixTransform(), matView);

    // Upload model normal matrix (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_NORMAL] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_NORMAL], MatrixTranspose(MatrixInvert(matModel)));

#ifdef RL_SUPPORT_MESH_GPU_SKINNING
    // Upload Bone Transforms
    if ((material.shader.locs[SHADER_LOC_BONE_MATRICES] != -1) && mesh.boneMatrices)
    {
        rlSetUniformMatrices(material.shader.locs[SHADER_LOC_BONE_MATRICES], mesh.boneMatrices, mesh.boneCount);
    }
#endif

    //-----------------------------------------------------

    // Bind active texture maps (if available)
    for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        if (material.maps[i].texture.id > 0)
        {
            // Select current shader texture slot
            rlActiveTextureSlot(i);

            // Enable texture for active slot
            if ((i == MATERIAL_MAP_IRRADIANCE) ||
                (i == MATERIAL_MAP_PREFILTER) ||
                (i == MATERIAL_MAP_CUBEMAP)) rlEnableTextureCubemap(material.maps[i].texture.id);
            else rlEnableTexture(material.maps[i].texture.id);

            rlSetUniform(material.shader.locs[SHADER_LOC_MAP_DIFFUSE + i], &i, SHADER_UNIFORM_INT, 1);
        }
    }

    // Try binding vertex array objects (VAO)
    // or use VBOs if not possible
    if (!rlEnableVertexArray(mesh.vaoId))
    {
        // Bind mesh VBO data: vertex position (shader-location = 0)
        rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION], 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION]);

        // Bind mesh VBO data: vertex texcoords (shader-location = 1)
        rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01], 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]);

        if (material.shader.locs[SHADER_LOC_VERTEX_NORMAL] != -1)
        {
            // Bind mesh VBO data: vertex normals (shader-location = 2)
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL], 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL]);
        }

        // Bind mesh VBO data: vertex colors (shader-location = 3, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_COLOR] != -1)
        {
            if (mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0)
            {
                rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
                rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR], 4, RL_UNSIGNED_BYTE, 1, 0, 0);
                rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
            else
            {
                // Set default value for unused attribute
                // NOTE: Required when using default shader and no VAO support
                float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                rlSetVertexAttributeDefault(material.shader.locs[SHADER_LOC_VERTEX_COLOR], value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
        }

        // Bind mesh VBO data: vertex tangents (shader-location = 4, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TANGENT] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT], 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT]);
        }

        // Bind mesh VBO data: vertex texcoords2 (shader-location = 5, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02], 2, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02]);
        }

#ifdef RL_SUPPORT_MESH_GPU_SKINNING
        // Bind mesh VBO data: vertex bone ids (shader-location = 6, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_BONEIDS] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEIDS]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEIDS], 4, RL_UNSIGNED_BYTE, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEIDS]);
        }

        // Bind mesh VBO data: vertex bone weights (shader-location = 7, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS], 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS]);
        }
#endif

        if (mesh.indices != NULL) rlEnableVertexBufferElement(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
    }

    int eyeCount = 1;
    if (rlIsStereoRenderEnabled()) eyeCount = 2;

    for (int eye = 0; eye < eyeCount; eye++)
    {
        // Calculate model-view-projection matrix (MVP)
        Matrix matModelViewProjection = MatrixIdentity();
        if (eyeCount == 1) matModelViewProjection = MatrixMultiply(matModelView, matProjection);
        else
        {
            // Setup current eye viewport (half screen width)
            rlViewport(eye*rlGetFramebufferWidth()/2, 0, rlGetFramebufferWidth()/2, rlGetFramebufferHeight());
            matModelViewProjection = MatrixMultiply(MatrixMultiply(matModelView, rlGetMatrixViewOffsetStereo(eye)), rlGetMatrixProjectionStereo(eye));
        }

        // Send combined model-view-projection matrix to shader
        rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matModelViewProjection);

        // Draw mesh instanced
        if (mesh.indices != NULL) rlDrawVertexArrayElementsInstanced(0, mesh.triangleCount*3, 0, instances);
        else rlDrawVertexArrayInstanced(0, mesh.vertexCount, instances);
    }

    // Unbind all bound texture maps
    for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        if (material.maps[i].texture.id > 0)
        {
            // Select current shader texture slot
            rlActiveTextureSlot(i);

            // Disable texture for active slot
            if ((i == MATERIAL_MAP_IRRADIANCE) ||
                (i == MATERIAL_MAP_PREFILTER) ||
                (i == MATERIAL_MAP_CUBEMAP)) rlDisableTextureCubemap();
            else rlDisableTexture();
        }
    }

    // Disable all possible vertex array objects (or VBOs)
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Disable shader program
    rlDisableShader();

    // Remove instance transforms buffer
    rlUnloadVertexBuffer(instancesVboId);
    RL_FREE(instanceTransforms);
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(void) {
    InitWindow(1280, 720, "Raw GL Instancing Test (Raylib + OpenGL)");
    SetTargetFPS(60);
    //DisableCursor();

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 10.0f, 10.0f, 10.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Model model = LoadModel("tree.glb");
    printf("Material count: %d\n", model.materialCount);
    Matrix instanceTransforms[MAX_INSTANCES];
    FillInstanceTransforms(instanceTransforms, MAX_INSTANCES);
    //Texture2D tex = LoadTexture("tree_diffuse.png");
    //model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = tex;

    // Setup instance matrices in a grid
    for (int i = 0; i < MAX_INSTANCES; i++) {
        float x = (i % 10) * 3.0f;
        float z = (i / 10) * 3.0f;
        instanceTransforms[i] = MatrixTranslate(x, 0.0f, z);
    }

    //Mesh mesh = model.meshes[0];
    //UploadInstanceBuffer();

    // Bind VAO and attach instance buffer
    //glBindVertexArray(mesh.vaoId);
    //glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);  // Already done in UploadInstanceBuffer

    while (!WindowShouldClose()) {
        UpdateCamera(&camera, CAMERA_FIRST_PERSON);

        //input to take screen shots to show chat gpt
        if(IsKeyDown(KEY_F12))
        {
            TakeScreenshotWithTimestamp();
        }
        
        BeginDrawing();
        //ClearBackground(SKYBLUE);
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);

        // Use same shader as Raylib set
        //rlEnableShader(model.materials[0].shader.id);
        //glBindVertexArray(mesh.vaoId);
        //glDrawElementsInstanced(GL_TRIANGLES, mesh.triangleCount * 3, GL_UNSIGNED_SHORT, 0, MAX_INSTANCES);
        //rlDisableShader();
        
        //if we need to see that the model will actuall render...
        //DrawModel(model, Vector3Zero(), 1.0f, WHITE);
        //DrawMesh(model.meshes[0], model.materials[1], model.transform);

        //can I draw the mesh of the tree with draw mesh custom?
        //DrawMeshCustom(model.meshes[0], model.materials[0], model.transform);
        //DrawMeshCustomGpt(model.meshes[0], model.materials[0], MatrixIdentity(), camera, MAX_INSTANCES);
        //lets just go for it and see if we get that error from before
        //DrawMeshInstancedCustom(model.meshes[0], model.materials[1], instanceTransforms, MAX_INSTANCES);
        DrawMeshInstancedCustom(model.meshes[0], model.materials[1], &model.transform, 1);

        //draw reference points - Ozzy died today so Electric Funeral and 666
        DrawCube((Vector3){0,0,666},120, 120, 120, YELLOW);
        DrawCube((Vector3){0,666,0},120, 120, 120, BLUE);
        DrawCube((Vector3){666,0,0},120, 120, 120, MAROON);
        DrawCube((Vector3){0,0,-666},120, 120, 120, DARKGREEN);
        DrawCube((Vector3){0,-666,0},120, 120, 120, DARKPURPLE);
        DrawCube((Vector3){-666,0,0},120, 120, 120, MAGENTA);

        //end drawing
        EndMode3D();
        DrawFPS(10, 10);
        EndDrawing();
    }

    UnloadModel(model);
    CloseWindow();
    return 0;
}
