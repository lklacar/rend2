// Filename:-	gl_bits.cpp
//

#include "gl_bits.h"
#include "stdafx.h"

#include "includes.h"
#include <sstream>
#include <string>

int g_iAssertCounter = 0; // just used for debug-outs

std::string csGLVendor;
std::string csGLRenderer;
std::string csGLVersion;
std::string csGLExtensions;

std::string GL_GetInfo()
{
    std::ostringstream ss;

    ss << "\nGL_VENDOR:\t" << csGLVendor << '\n';
    ss << "GL_RENDERER:\t" << csGLRenderer << '\n';
    ss << "GL_VERSION:\t" << csGLVersion << '\n';
    ss << "GL_EXTENSIONS:\t" << csGLExtensions << '\n';

    return ss.str();
}

void GL_CacheDriverInfo()
{
    csGLVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    csGLRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    csGLVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    csGLExtensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
}

//////////// ?????
#define NEAR_GL_PLANE 0.1
#define FAR_GL_PLANE 512

void GL_Enter3D(
    double dFOV, int iWindowWidth, int iWindowDepth, bool bWireFrame,
    bool bCLS /* = true */)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (iWindowDepth > 0)
        gluPerspective(
            dFOV,
            (double)iWindowWidth / (double)iWindowDepth,
            NEAR_GL_PLANE,
            FAR_GL_PLANE);
    glViewport(0, 0, iWindowWidth, iWindowDepth);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_DEPTH_TEST);

    if (bCLS)
    {
        //		glClearColor	(0,0,0,0);
        glClearColor(
            AppVars._R / 255.0f,
            AppVars._G / 255.0f,
            AppVars._B / 255.0f,
            0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if (AppVars.bShowPolysAsDoubleSided && !AppVars.bForceWhite)
    {
        glDisable(GL_CULL_FACE);
    }
    else
    {
        glEnable(GL_CULL_FACE);
    }

    if (bWireFrame)
    {
        //		glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glDisable(GL_LIGHTING);
    }
    else
    {
        //		glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);

        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

        // hitech: not this
        if (AppVars.bBilinear)
        {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); // ?
        }
        else
        {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }
    }

    glColor3f(1, 1, 1);
}

void GL_Enter2D(int iWindowWidth, int iWindowDepth, bool bCLS /* = true */)
{
    glViewport(0, 0, iWindowWidth, iWindowDepth);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, iWindowWidth, iWindowDepth, 0, -99999, 99999);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    if (bCLS)
    {
        //		glClearColor	(0,1,0,0);
        glClearColor(
            AppVars._R / 255.0f,
            AppVars._G / 255.0f,
            AppVars._B / 255.0f,
            0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glColor3f(1, 1, 1);
}

void GL_Exit2D(void)
{
    glDisable(GL_TEXTURE_2D);
    glColor3f(1, 1, 1);
}

#ifdef _DEBUG
void AssertGL(const char* sFile, int iLine)
{
    GLenum glError;

    if ((glError = glGetError()) != GL_NO_ERROR)
    {
        int iReportCount = 0; /* stop crashes via only 32 errors max */

        OutputDebugString(
            va("*** GL_ERROR! *** (File:%s Line:%d\n", sFile, iLine));

        do
        {
            OutputDebugString(va(
                "(%d) %s\n", g_iAssertCounter, (char*)gluErrorString(glError)));
            g_iAssertCounter++;
        } while (iReportCount++ < 32 &&
                 (glError = glGetError()) != GL_NO_ERROR);
        //		ASSERT(0);
    }
}
#endif