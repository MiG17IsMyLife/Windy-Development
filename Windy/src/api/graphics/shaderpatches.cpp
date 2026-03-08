#define _CRT_SECURE_NO_WARNINGS

#include "shaderpatches.h"
#include "../../core/log.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <string.h>
#include <stdlib.h>
#include "shaderWork/common.h"
#include "shaderWork/or2.h"

// Standard OpenGL headers often do not define function pointer types for core OpenGL 1.0/1.1 functions.
#ifndef PFNGLENABLEPROC
typedef void (APIENTRY *PFNGLENABLEPROC)(GLenum);
#endif

#ifndef PFNGLDISABLEPROC
typedef void (APIENTRY *PFNGLDISABLEPROC)(GLenum);
#endif
// Helper macro to lazy-load GL function pointers
#define GL_CALL_VOID(funcName, type, ...) \
    static type p##funcName = (type)SDL_GL_GetProcAddress(#funcName); \
    if (p##funcName) p##funcName(__VA_ARGS__); \
    else log_error("ShaderPatches: Missing symbol %s", #funcName);

#define GL_CALL_RET(funcName, type, failRet, ...) \
    static type p##funcName = (type)SDL_GL_GetProcAddress(#funcName); \
    if (p##funcName) return p##funcName(__VA_ARGS__); \
    else { log_error("ShaderPatches: Missing symbol %s", #funcName); return failRet; }

/**
 *   Search and Replace function.
 */
char *replaceSubstring(const char *buffer, int start, int end, const char *search, const char *replace)
{
    int searchLen = strlen(search);
    int replaceLen = strlen(replace);
    int bufferLen = strlen(buffer);

    if (start < 0 || end > bufferLen || start > end)
    {
        printf("Invalid start or end positions\n");
        return NULL;
    }

    int maxOccurrences = 0;
    const char *tmp = buffer + start;
    while ((tmp = strstr(tmp, search)) && tmp < buffer + end)
    {
        maxOccurrences++;
        tmp += searchLen;
    }
    int maxNewLen =
        (bufferLen + (maxOccurrences * (replaceLen - searchLen))) + 15; // 15 instead of 1 because it crashes with SRTV for no reason??

    char *newBuffer = (char *)malloc(maxNewLen);
    memset(newBuffer, '\0', maxNewLen);

    if (!newBuffer)
    {
        printf("Memory allocation failed\n");
        return NULL;
    }

    const char *pos = buffer + start;
    const char *endPos = buffer + end;
    char *newPos = newBuffer;

    memcpy(newPos, buffer, start);
    newPos += start;

    while (pos < endPos)
    {
        const char *foundPos = strstr(pos, search);
        if (foundPos && foundPos < endPos)
        {
            int chunkLen = foundPos - pos;
            memcpy(newPos, pos, chunkLen);
            newPos += chunkLen;

            memcpy(newPos, replace, replaceLen);
            newPos += replaceLen;
            pos = foundPos + searchLen;
        }
        else
        {
            int remainingLen = endPos - pos;
            memcpy(newPos, pos, remainingLen);
            newPos += remainingLen;
            break;
        }
    }
    strcpy(newPos, buffer + end);

    return newBuffer;
}

char *replaceInBlock(char *source, SearchReplace *searchReplace, int searchReplaceCount, char *startSearch, char *endSearch)
{
    if (source == NULL || searchReplace == NULL)
        return NULL;

    int start_index = 0;
    if (startSearch != NULL && startSearch[0] != '\0')
    {
        char *start_pos = strstr(source, startSearch);
        if (start_pos == NULL)
            return source;
        start_index = start_pos - source + strlen(startSearch);
    }

    char *result = source;

    for (int i = 0; i < searchReplaceCount; i++)
    {
        const char *end_pos = result + strlen(result);

        if (endSearch[0] != '\0')
        {
            char *end_pos_candidate = strstr(result + start_index, endSearch);
            if (end_pos_candidate != NULL)
                end_pos = end_pos_candidate;
        }

        int end_index = end_pos - result;
        const char *target = searchReplace[i].search;
        const char *replacement = searchReplace[i].replacement;
        char *new_result = replaceSubstring(result, start_index, end_index, target, replacement);
        if (new_result != NULL)
        {
            if (result != source)
            {
                free(result);
            }
            result = new_result;
        }
    }
    return result;
}

void ShaderPatches::glEnable(GLenum cap)
{
    if (cap == GL_FRAGMENT_PROGRAM_NV) 
        cap = GL_FRAGMENT_PROGRAM_ARB;
    GL_CALL_VOID(glEnable, PFNGLENABLEPROC, cap);
}

void ShaderPatches::glDisable(GLenum cap)
{
    if (cap == GL_FRAGMENT_PROGRAM_NV) 
        cap = GL_FRAGMENT_PROGRAM_ARB;
    GL_CALL_VOID(glDisable, PFNGLDISABLEPROC, cap);
}

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
        target = GL_FRAGMENT_PROGRAM_ARB;
    GL_CALL_VOID(glBindProgramARB, PFNGLBINDPROGRAMARBPROC, target, id);
}

// Local helper for glProgramStringARB
void ShaderPatches::my_glProgramStringARB(GLenum target, GLenum format, GLsizei len, const GLvoid *string)
{
    // char *newProgram = (char *)string;
    // char *program_to_free = NULL;
    // int newProgramLen;
    // if (target == GL_VERTEX_PROGRAM_ARB)
    // {
    //     newProgram = replaceInBlock((char *)string, orVsMesa, orVsMesaCount, "", "");
    // }
    // else if (target == GL_FRAGMENT_PROGRAM_ARB)
    // {
    //     newProgram = replaceInBlock((char *)string, orFsMesa, orFsMesaCount, "", "");
    // }
    // if (newProgram != string)
    //     program_to_free = newProgram;

    // newProgramLen = strlen(newProgram);
    // GL_CALL_VOID(glProgramStringARB, PFNGLPROGRAMSTRINGARBPROC, target, format, newProgramLen, newProgram);
    // if (program_to_free)
    //     free(program_to_free);
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
    ShaderPatches::my_glProgramStringARB(target, program_fmt, newProgramLen, newProgram);
}

void ShaderPatches::glLoadProgramNV(GLenum target, GLuint id, GLsizei len, const GLubyte *program)
{
    ShaderPatches::glBindProgramNV(target, id);
    my_glProgramStringARB(target, GL_PROGRAM_FORMAT_ASCII_ARB, len, program);
}

GLboolean ShaderPatches::glIsProgramNV(GLuint id)
{
    GL_CALL_RET(glIsProgramARB, PFNGLISPROGRAMARBPROC, GL_FALSE, id);
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


