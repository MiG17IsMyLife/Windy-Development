#pragma once

#include "SDL3/SDL_opengl.h"

class GLUTBridge
{
public:
    static void glutInit(int *argcp, char **argv);
    static void glutInitDisplayMode(unsigned int mode);
    static void glutInitWindowSize(int width, int height);
    static void glutInitWindowPosition(int x, int y);
    static int glutCreateWindow(const char *title);
    static int glutEnterGameMode();
    static void glutLeaveGameMode();
    static void glutMainLoop();
    static void glutDisplayFunc(void (*func)(void));
    static void glutIdleFunc(void (*func)(void));
    static void glutPostRedisplay();
    static void glutSwapBuffers();
    static int glutGet(int state);
    static void glutSetCursor(int cursor);
    static void glutGameModeString(const char *string);
    static void glutBitmapCharacter(void *font, int character);
    static int glutBitmapWidth(void *font, int character);
    static void glutMainLoopEvent();
    static void glutJoystickFunc(void (*callback)(unsigned int, int, int, int), int pollInterval);
    static void glutKeyboardFunc(void (*callback)(unsigned char, int, int));
    static void glutKeyboardUpFunc(void (*callback)(unsigned char, int, int));
    static void glutMouseFunc(void (*callback)(int, int, int, int));
    static void glutMotionFunc(void (*callback)(int, int));
    static void glutSpecialFunc(void (*callback)(int, int, int));
    static void glutSpecialUpFunc(void (*callback)(int, int, int));
    static void glutPassiveMotionFunc(void (*callback)(int, int));
    static void glutEntryFunc(void (*callback)(int));
    static void glutSolidTeapot(double size);
    static void glutWireTeapot(double size);
    static void glutSolidSphere(double radius, GLint slices, GLint stacks);
    static void glutWireSphere(double radius, GLint slices, GLint stacks);
    static void glutWireCone(double base, double height, GLint slices, GLint stacks);
    static void glutSolidCone(double base, double height, GLint slices, GLint stacks);
    static void glutWireCube(double dSize);
    static void glutSolidCube(double dSize);
};
