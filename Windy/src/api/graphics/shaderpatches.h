#pragma once

#include "glhooks.h"
#include <SDL3/SDL_opengl.h>

class ShaderPatches
{
  public:
    // NV Vertex/Fragment Program Extensions
    static void glGenProgramsNV(GLsizei n, GLuint *programs);
    static void glDeleteProgramsNV(GLsizei n, const GLuint *programs);
    static void glBindProgramNV(GLenum target, GLuint id);
    static void glLoadProgramNV(GLenum target, GLuint id, GLsizei len, const GLubyte *program);
    static GLboolean glIsProgramNV(GLuint id);
    static void glTrackMatrixNV(GLenum target, GLuint address, GLenum matrix, GLenum transform);
    static void glProgramParameter4fNV(GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
    static void glProgramParameter4fvNV(GLenum target, GLuint index, const GLfloat *v);
    static void glProgramParameters4fvNV(GLenum target, GLuint index, GLsizei count, const GLfloat *v);

    // Occlusion Query (NV)
    static void glGenOcclusionQueriesNV(GLsizei n, GLuint *ids);
    static void glBeginOcclusionQueryNV(GLuint id);
    static void glEndOcclusionQueryNV();
    static void glGetOcclusionQueryuivNV(GLuint id, GLenum pname, GLuint *params);
};
