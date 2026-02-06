#include "shaderpatches.h"
#include "../../core/log.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <string.h> // For strlen

// Helper macro to lazy-load GL function pointers
#define GL_CALL_VOID(funcName, type, ...) \
    static type p##funcName = (type)SDL_GL_GetProcAddress(#funcName); \
    if (p##funcName) p##funcName(__VA_ARGS__); \
    else log_error("ShaderPatches: Missing symbol %s", #funcName);

#define GL_CALL_RET(funcName, type, failRet, ...) \
    static type p##funcName = (type)SDL_GL_GetProcAddress(#funcName); \
    if (p##funcName) return p##funcName(__VA_ARGS__); \
    else { log_error("ShaderPatches: Missing symbol %s", #funcName); return failRet; }


void ShaderPatches::glGenProgramsNV(GLsizei n, GLuint *programs)
{
    GL_CALL_VOID(glGenProgramsARB, PFNGLGENPROGRAMSARBPROC, n, programs);
}

void ShaderPatches::glDeleteProgramsNV(GLsizei n, const GLuint *programs)
{
    GL_CALL_VOID(glDeleteProgramsARB, PFNGLDELETEPROGRAMSARBPROC, n, programs);
}

void ShaderPatches::glBindProgramNV(GLenum target, GLuint id)
{
    // Remap NV fragment program target to ARB
    if (target == GL_FRAGMENT_PROGRAM_NV)
    {
        target = GL_FRAGMENT_PROGRAM_ARB;
    }
    GL_CALL_VOID(glBindProgramARB, PFNGLBINDPROGRAMARBPROC, target, id);
}

// Local helper for glProgramStringARB
static void my_glProgramStringARB(GLenum target, GLenum format, GLsizei len, const GLvoid *string)
{
    GL_CALL_VOID(glProgramStringARB, PFNGLPROGRAMSTRINGARBPROC, target, format, len, string);
}

void gl_ProgramStringARB_Internal(int target, int program_fmt, int program_len, const GLubyte *program)
{
    const GLubyte *newProgram = program;
    int newProgramLen;

    // Logic placeholder for shader replacement/patching
    if (target == GL_VERTEX_PROGRAM_ARB)
    {
        newProgram = program;
    }
    else if (target == GL_FRAGMENT_PROGRAM_ARB)
    {
        // Placeholder
    }
    
    newProgramLen = strlen((const char *)newProgram);
    my_glProgramStringARB(target, program_fmt, newProgramLen, newProgram);
}

void ShaderPatches::glLoadProgramNV(GLenum target, GLuint id, GLsizei len, const GLubyte *program)
{
    ShaderPatches::glBindProgramNV(target, id);
    gl_ProgramStringARB_Internal(target, GL_PROGRAM_FORMAT_ASCII_ARB, len, program);
}

GLboolean ShaderPatches::glIsProgramNV(GLuint id)
{
    GL_CALL_RET(glIsProgramARB, PFNGLISPROGRAMARBPROC, GL_FALSE, id);
}

void ShaderPatches::glTrackMatrixNV(GLenum target, GLuint address, GLenum matrix, GLenum transform)
{
    // No direct ARB equivalent for track matrix in this context, usually stubbed or emulated via uniforms
    log_warn("STUB: glTrackMatrixNV not fully implemented");
}

void ShaderPatches::glProgramParameter4fNV(GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    GL_CALL_VOID(glProgramEnvParameter4fARB, PFNGLPROGRAMENVPARAMETER4FARBPROC, target, index, x, y, z, w);
}

void ShaderPatches::glProgramParameter4fvNV(GLenum target, GLuint index, const GLfloat *v)
{
    GL_CALL_VOID(glProgramEnvParameter4fvARB, PFNGLPROGRAMENVPARAMETER4FVARBPROC, target, index, v);
}

void ShaderPatches::glProgramParameters4fvNV(GLenum target, GLuint index, GLsizei count, const GLfloat *v)
{
    // Check for EXT first, then fallback or looping ARB
    static PFNGLPROGRAMENVPARAMETERS4FVEXTPROC pglProgramEnvParameters4fvEXT = (PFNGLPROGRAMENVPARAMETERS4FVEXTPROC)SDL_GL_GetProcAddress("glProgramEnvParameters4fvEXT");
    if (pglProgramEnvParameters4fvEXT) {
        pglProgramEnvParameters4fvEXT(target, index, count, v);
    } else {
        // Fallback: loop through standard ARB calls
        static PFNGLPROGRAMENVPARAMETER4FVARBPROC pglProgramEnvParameter4fvARB = (PFNGLPROGRAMENVPARAMETER4FVARBPROC)SDL_GL_GetProcAddress("glProgramEnvParameter4fvARB");
        if (pglProgramEnvParameter4fvARB) {
            for(int i=0; i<count; ++i) {
                pglProgramEnvParameter4fvARB(target, index + i, v + (i*4));
            }
        }
    }
}

void ShaderPatches::glGenOcclusionQueriesNV(GLsizei n, GLuint *ids)
{
    GL_CALL_VOID(glGenQueriesARB, PFNGLGENQUERIESARBPROC, n, ids);
}

void ShaderPatches::glBeginOcclusionQueryNV(GLuint id)
{
    GL_CALL_VOID(glBeginQueryARB, PFNGLBEGINQUERYARBPROC, GL_SAMPLES_PASSED_ARB, id);
}

void ShaderPatches::glEndOcclusionQueryNV()
{
    GL_CALL_VOID(glEndQueryARB, PFNGLENDQUERYARBPROC, GL_SAMPLES_PASSED_ARB);
}

void ShaderPatches::glGetOcclusionQueryuivNV(GLuint id, GLenum pname, GLuint *params)
{
    GL_CALL_VOID(glGetQueryObjectuivARB, PFNGLGETQUERYOBJECTUIVARBPROC, id, pname, params);
}

