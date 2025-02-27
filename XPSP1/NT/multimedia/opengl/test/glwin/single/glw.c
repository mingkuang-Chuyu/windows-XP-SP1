/*
 * (c) Copyright 1993, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED 
 * Permission to use, copy, modify, and distribute this software for 
 * any purpose and without fee is hereby granted, provided that the above
 * copyright notice appear in all copies and that both the copyright notice
 * and this permission notice appear in supporting documentation, and that 
 * the name of Silicon Graphics, Inc. not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission. 
 *
 * THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU "AS-IS"
 * AND WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR OTHERWISE,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR
 * FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL SILICON
 * GRAPHICS, INC.  BE LIABLE TO YOU OR ANYONE ELSE FOR ANY DIRECT,
 * SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY
 * KIND, OR ANY DAMAGES WHATSOEVER, INCLUDING WITHOUT LIMITATION,
 * LOSS OF PROFIT, LOSS OF USE, SAVINGS OR REVENUE, OR THE CLAIMS OF
 * THIRD PARTIES, WHETHER OR NOT SILICON GRAPHICS, INC.  HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH LOSS, HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
 * POSSESSION, USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 * US Government Users Restricted Rights 
 * Use, duplication, or disclosure by the Government is subject to
 * restrictions set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 252.227-7013 and/or in similar or successor
 * clauses in the FAR or the DOD or NASA FAR Supplement.
 * Unpublished-- rights reserved under the copyright laws of the
 * United States.  Contractor/manufacturer is Silicon Graphics,
 * Inc., 2011 N.  Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * OpenGL(TM) is a trademark of Silicon Graphics, Inc.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys\types.h>
#include <sys\timeb.h>

#include <glwin.h>
#include <gl/glu.h>


#pragma warning(disable:4244)

#define PI 3.14159265358979323846

#define GETCOORD(frame, x, y) (&(theMesh.coords[frame*theMesh.numCoords+(x)+(y)*(theMesh.widthX+1)]))
#define GETFACET(frame, x, y) (&(theMesh.facets[frame*theMesh.numFacets+(x)+(y)*theMesh.widthX]))


GLenum rgb, doubleBuffer, directRender;
GLenum noSwap, noClear, useMcd;

GLint colorIndexes1[3];
GLint colorIndexes2[3];
GLenum clearMask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;

GLenum smooth = GL_TRUE;
GLenum lighting = GL_TRUE;
GLenum depth = GL_TRUE;
GLenum stepMode = GL_FALSE;
GLenum spinMode = GL_FALSE;
GLint contouring = 0;

GLint widthX, widthY;
GLint checkerSize;
float height;

GLint frames, curFrame = 0, nextFrame = 0;
GLboolean displayFrameRate = GL_FALSE;
static GLint frameCount = 0;

struct facet {
    float color[3];
    float normal[3];
};
struct coord {
    float vertex[3];
    float normal[3];
};
struct mesh {
    GLint widthX, widthY;
    GLint numFacets;
    GLint numCoords;
    GLint frames;
    struct coord *coords;
    struct facet *facets;
} theMesh;

GLubyte contourTexture1[] = {
    255, 255, 255, 255,
    255, 255, 255, 255,
    255, 255, 255, 255,
    127, 127, 127, 127,
};
GLubyte contourTexture2[] = {
    255, 255, 255, 255,
    255, 127, 127, 127,
    255, 127, 127, 127,
    255, 127, 127, 127,
};

#define ANIMATE_ALL
#define STAGGER_WINDOWS
#define SWAP_MULTIPLE
// #define HIDE_UNDER

#define NWIN 4
int nWin;

GLWINDOW gwWin[NWIN];
int iFront;

WGLSWAP wswaps[NWIN];

DWORD adwFlags[NWIN] =
{
    GLWIN_Z_BUFFER_16,
    GLWIN_Z_BUFFER_16 | GLWIN_BACK_BUFFER,
#if NWIN > 2
    GLWIN_Z_BUFFER_32,
#endif
#if NWIN > 3
    GLWIN_Z_BUFFER_32 | GLWIN_BACK_BUFFER
#endif
};

void Quit(void)
{
    int i;

    for (i = 0; i < nWin; i++)
    {
        if (gwWin[i] != NULL)
        {
            glwinDestroyWindow(gwWin[i]);
        }
    }
    exit(1);
}

static void Animate(GLWINDOW gw)
{
    struct coord *coord;
    struct facet *facet;
    float *lastColor;
    float *thisColor;
    GLint i, j;
    static struct _timeb thisTime, baseTime;
    double elapsed, frameRate, deltat;

    if (stepMode)
    {
        return;
    }
       
    glwinMakeCurrent(gw);
    
    if (!noClear)
    {
        glClear(clearMask);
    }

    if (nextFrame || !stepMode) {
	curFrame++;
    }

    if (curFrame >= theMesh.frames) {
	// mf: do frame rate calcs here
    	if( !frameCount ) {
 	    _ftime( &baseTime );
	}
	else {
	    if( displayFrameRate ) {
 	        _ftime( &thisTime );
	        elapsed = thisTime.time + thisTime.millitm/1000.0 -
		          (baseTime.time + baseTime.millitm/1000.0);
	        if( elapsed == 0.0 )
	            printf( "Frame rate = unknown\n" );
	        else {
	            frameRate = frameCount / elapsed;
	            printf( "Frame rate = %5.2f fps\n", frameRate );
	        }
	    }
	}
	frameCount += theMesh.frames;

	curFrame = 0;
    }

    if ((nextFrame || !stepMode) && spinMode) {
	glRotatef(5.0, 0.0, 0.0, 1.0);
    }
    nextFrame = 0;

    for (i = 0; i < theMesh.widthX; i++) {
	glBegin(GL_QUAD_STRIP);
	lastColor = NULL;
	for (j = 0; j < theMesh.widthY; j++) {
	    facet = GETFACET(curFrame, i, j);
	    if (!smooth && lighting) {
		glNormal3fv(facet->normal);
	    }
	    if (lighting) {
		if (rgb) {
		    thisColor = facet->color;
		    glColor3fv(facet->color);
		} else {
		    thisColor = facet->color;
		    glMaterialfv(GL_FRONT_AND_BACK, GL_COLOR_INDEXES, 
				 facet->color);
		}
	    } else {
		if (rgb) {
		    thisColor = facet->color;
		    glColor3fv(facet->color);
		} else {
		    thisColor = facet->color;
		    glIndexf(facet->color[1]);
		}
	    }

	    if (!lastColor || (thisColor[0] != lastColor[0] && smooth)) {
		if (lastColor) {
		    glEnd();
		    glBegin(GL_QUAD_STRIP);
		}
		coord = GETCOORD(curFrame, i, j);
		if (smooth && lighting) {
		    glNormal3fv(coord->normal);
		}
		glVertex3fv(coord->vertex);

		coord = GETCOORD(curFrame, i+1, j);
		if (smooth && lighting) {
		    glNormal3fv(coord->normal);
		}
		glVertex3fv(coord->vertex);
	    }

	    coord = GETCOORD(curFrame, i, j+1);
	    if (smooth && lighting) {
		glNormal3fv(coord->normal);
	    }
	    glVertex3fv(coord->vertex);

	    coord = GETCOORD(curFrame, i+1, j+1);
	    if (smooth && lighting) {
		glNormal3fv(coord->normal);
	    }
	    glVertex3fv(coord->vertex);

	    lastColor = thisColor;
	}
	glEnd();
    }

    glFlush();
#ifndef SWAP_MULTIPLE
    if ((glwinGetFlags(gw) & GLWIN_BACK_BUFFER) && !noSwap) {
	glwinSwapBuffers(gw);
    }
#endif
}

void AnimateN(GLWINDOW gw)
{
#ifdef ANIMATE_ALL
    int i;

    for (i = 0; i < nWin; i++)
    {
        Animate(gwWin[i]);
    }
#ifdef SWAP_MULTIPLE
    wglSwapMultipleBuffers(nWin, wswaps);
#endif
#else
    Animate(gwWin[iFront]);
#ifdef SWAP_MULTIPLE
    wglSwapMultipleBuffers(1, &wswaps[iFront]);
#endif
#endif
}

static void InitMesh(void)
{
    struct coord *coord;
    struct facet *facet;
    float dp1[3], dp2[3];
    float *pt1, *pt2, *pt3;
    float angle, d, x, y;
    GLint numFacets, numCoords, frameNum, i, j;

    theMesh.widthX = widthX;
    theMesh.widthY = widthY;
    theMesh.frames = frames;

    numFacets = widthX * widthY;
    numCoords = (widthX + 1) * (widthY + 1);

    theMesh.numCoords = numCoords;
    theMesh.numFacets = numFacets;

    theMesh.coords = (struct coord *)malloc(frames*numCoords*
					    sizeof(struct coord));
    theMesh.facets = (struct facet *)malloc(frames*numFacets*
					    sizeof(struct facet));
    if (theMesh.coords == NULL || theMesh.facets == NULL) {
	printf("Out of memory.\n");
	exit(1);
    }

    for (frameNum = 0; frameNum < frames; frameNum++) {
	for (i = 0; i <= widthX; i++) {
	    x = i / (float)widthX;
	    for (j = 0; j <= widthY; j++) {
		y = j / (float)widthY;

		d = sqrt(x*x+y*y);
		if (d == 0.0) {
		    d = 0.0001;
		}
		angle = 2 * PI * d + (2 * PI / frames * frameNum);

		coord = GETCOORD(frameNum, i, j);

		coord->vertex[0] = x - 0.5;
		coord->vertex[1] = y - 0.5;
		coord->vertex[2] = (height - height * d) * cos(angle);

		coord->normal[0] = -(height / d) * x * ((1 - d) * 2 * PI *
				   sin(angle) + cos(angle));
		coord->normal[1] = -(height / d) * y * ((1 - d) * 2 * PI *
				   sin(angle) + cos(angle));
		coord->normal[2] = -1;

		d = 1.0 / sqrt(coord->normal[0]*coord->normal[0]+
			       coord->normal[1]*coord->normal[1]+1);
		coord->normal[0] *= d;
		coord->normal[1] *= d;
		coord->normal[2] *= d;
	    }
	}
	for (i = 0; i < widthX; i++) {
	    for (j = 0; j < widthY; j++) {
		facet = GETFACET(frameNum, i, j);
		if (((i/checkerSize)%2)^(j/checkerSize)%2) {
		    if (rgb) {
			facet->color[0] = 1.0;
			facet->color[1] = 0.2;
			facet->color[2] = 0.2;
		    } else {
			facet->color[0] = colorIndexes1[0];
			facet->color[1] = colorIndexes1[1];
			facet->color[2] = colorIndexes1[2];
		    }
		} else {
		    if (rgb) {
			facet->color[0] = 0.2;
			facet->color[1] = 1.0;
			facet->color[2] = 0.2;
		    } else {
			facet->color[0] = colorIndexes2[0];
			facet->color[1] = colorIndexes2[1];
			facet->color[2] = colorIndexes2[2];
		    }
		}
		pt1 = GETCOORD(frameNum, i, j)->vertex;
		pt2 = GETCOORD(frameNum, i, j+1)->vertex;
		pt3 = GETCOORD(frameNum, i+1, j+1)->vertex;

		dp1[0] = pt2[0] - pt1[0];
		dp1[1] = pt2[1] - pt1[1];
		dp1[2] = pt2[2] - pt1[2];

		dp2[0] = pt3[0] - pt2[0];
		dp2[1] = pt3[1] - pt2[1];
		dp2[2] = pt3[2] - pt2[2];

		facet->normal[0] = dp1[1] * dp2[2] - dp1[2] * dp2[1];
		facet->normal[1] = dp1[2] * dp2[0] - dp1[0] * dp2[2];
		facet->normal[2] = dp1[0] * dp2[1] - dp1[1] * dp2[0];

		d = 1.0 / sqrt(facet->normal[0]*facet->normal[0]+
			       facet->normal[1]*facet->normal[1]+
			       facet->normal[2]*facet->normal[2]);

		facet->normal[0] *= d;
		facet->normal[1] *= d;
		facet->normal[2] *= d;
	    }
	}
    }
}

static void InitMaterials(void)
{
    static float ambient[] = {0.1, 0.1, 0.1, 1.0};
    static float diffuse[] = {0.5, 1.0, 1.0, 1.0};
    static float position[] = {90.0, 90.0, 150.0, 0.0};
    static float front_mat_shininess[] = {60.0};
    static float front_mat_specular[] = {0.2, 0.2, 0.2, 1.0};
    static float front_mat_diffuse[] = {0.5, 0.28, 0.38, 1.0};
    static float back_mat_shininess[] = {60.0};
    static float back_mat_specular[] = {0.5, 0.5, 0.2, 1.0};
    static float back_mat_diffuse[] = {1.0, 1.0, 0.2, 1.0};
    static float lmodel_ambient[] = {1.0, 1.0, 1.0, 1.0};
    static float lmodel_twoside[] = {GL_TRUE};

    glMatrixMode(GL_PROJECTION);
    gluPerspective(450, 1.0, 0.5, 10.0);

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
    glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    
    glMaterialfv(GL_FRONT, GL_SHININESS, front_mat_shininess);
    glMaterialfv(GL_FRONT, GL_SPECULAR, front_mat_specular);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, front_mat_diffuse);
    glMaterialfv(GL_BACK, GL_SHININESS, back_mat_shininess);
    glMaterialfv(GL_BACK, GL_SPECULAR, back_mat_specular);
    glMaterialfv(GL_BACK, GL_DIFFUSE, back_mat_diffuse);
    if (rgb) {
	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
    }

    if (rgb) {
	glEnable(GL_COLOR_MATERIAL);
    } else {
        Quit();
    }
}

static void InitTexture(void)
{

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

static void Init(void)
{

    glClearColor(0.0, 0.0, 0.0, 0.0);

    if (smooth) {
        glShadeModel(GL_SMOOTH);
    } else {
        glShadeModel(GL_FLAT);
    }
    
    glFrontFace(GL_CW);

    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);

    InitMaterials();
    InitTexture();

    glMatrixMode(GL_MODELVIEW);
    glTranslatef(0.0, 0.4, -1.8);
    glScalef(2.0, 2.0, 2.0);
    glRotatef(-35.0, 1.0, 0.0, 0.0);
    glRotatef(35.0, 0.0, 0.0, 1.0);
}

static GLenum Key(UINT key)
{
#ifdef HIDE_UNDER
    int iOldFront;
#endif
    
    switch (key) {
      case VK_ESCAPE:
	Quit();
      case 'C':
	contouring++;
	if (contouring == 1) {
	    static GLfloat map[4] = {0, 0, 20, 0};

	    glTexImage2D(GL_TEXTURE_2D, 0, 3, 4, 4, 0, GL_LUMINANCE,
			 GL_UNSIGNED_BYTE, (GLvoid *)contourTexture1);
	    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	    glTexGenfv(GL_S, GL_OBJECT_PLANE, map);
	    glTexGenfv(GL_T, GL_OBJECT_PLANE, map);
	    glEnable(GL_TEXTURE_2D);
	    glEnable(GL_TEXTURE_GEN_S);
	    glEnable(GL_TEXTURE_GEN_T);
	} else if (contouring == 2) {
	    static GLfloat map[4] = {0, 0, 20, 0};

	    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	    glPushMatrix();
	    glMatrixMode(GL_MODELVIEW);
	    glLoadIdentity();
	    glTexGenfv(GL_S, GL_EYE_PLANE, map);
	    glTexGenfv(GL_T, GL_EYE_PLANE, map);
	    glPopMatrix();
	} else {
	    contouring = 0;
	    glDisable(GL_TEXTURE_GEN_S);
	    glDisable(GL_TEXTURE_GEN_T);
	    glDisable(GL_TEXTURE_2D);
	}
	break;
      case 'S':
	smooth = !smooth;
	if (smooth) {
	    glShadeModel(GL_SMOOTH);
	} else {
	    glShadeModel(GL_FLAT);
	}
	break;
      case 'L':
	lighting = !lighting;
	if (lighting) {
	    glEnable(GL_LIGHTING);
	    glEnable(GL_LIGHT0);
	    if (rgb) {
		glEnable(GL_COLOR_MATERIAL);
	    }
	} else {
	    glDisable(GL_LIGHTING);
	    glDisable(GL_LIGHT0);
	    if (rgb) {
		glDisable(GL_COLOR_MATERIAL);
	    }
	}
	break;
      case 'D':
	depth = !depth;
	if (depth) {
	    glEnable(GL_DEPTH_TEST);
	    clearMask |= GL_DEPTH_BUFFER_BIT;
	} else {
	    glDisable(GL_DEPTH_TEST);
	    clearMask &= ~GL_DEPTH_BUFFER_BIT;
	}
	break;
      case VK_SPACE:
	stepMode = !stepMode;
	break;
      case 'N':
	if (stepMode) {
	    nextFrame = 1;
	}
	break;
      case 'F':
	displayFrameRate = !displayFrameRate;
	frameCount = 0;
	break;
      case 'A':
	spinMode = !spinMode;
	break;
      case 'Z':
#ifdef HIDE_UNDER
        iOldFront = iFront;
#endif
        iFront = (iFront+1) % nWin;
#ifdef HIDE_UNDER
        ShowWindow(glwinGetHwnd(gwWin[iFront]), SW_SHOW);
        ShowWindow(glwinGetHwnd(gwWin[iOldFront]), SW_HIDE);
#else
        SetWindowPos(glwinGetHwnd(gwWin[iFront]), HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE);
#endif
        break;
      default:
	return GL_FALSE;
    }
    return GL_TRUE;
}

BOOL Message(GLWINDOW gw, HWND hwnd, UINT uiMsg, WPARAM wpm, LPARAM lpm,
             LRESULT *plr)
{
    switch(uiMsg)
    {
    case WM_KEYDOWN:
        Key((UINT)wpm);
        break;

    default:
        return FALSE;
    }

    *plr = 0;
    return TRUE;
}

static GLenum Args(int argc, char **argv)
{
    GLint i;

    rgb = GL_TRUE;
    doubleBuffer = GL_TRUE;
    directRender = GL_FALSE;
    noSwap = GL_FALSE;
    noClear = GL_FALSE;
    useMcd = GL_FALSE;
    nWin = NWIN;

    frames = 10;
    widthX = 10;
    widthY = 10;
    checkerSize = 2;
    height = 0.2;

    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-ci") == 0) {
	    rgb = GL_FALSE;
	} else if (strcmp(argv[i], "-rgb") == 0) {
	    rgb = GL_TRUE;
	} else if (strcmp(argv[i], "-sb") == 0) {
	    doubleBuffer = GL_FALSE;
	} else if (strcmp(argv[i], "-db") == 0) {
	    doubleBuffer = GL_TRUE;
	} else if (strcmp(argv[i], "-dr") == 0) {
	    directRender = GL_TRUE;
	} else if (strcmp(argv[i], "-ir") == 0) {
	    directRender = GL_FALSE;
	} else if (strcmp(argv[i], "-noswap") == 0) {
	    noSwap = GL_TRUE;
	} else if (strcmp(argv[i], "-noclear") == 0) {
	    noClear = GL_TRUE;
	} else if (strcmp(argv[i], "-mcd") == 0) {
	    useMcd = GL_TRUE;
        } else if (strcmp(argv[i], "-nwin") == 0) {
            if (i+1 >= argc || argv[i+1][0] == '-') {
                printf("-nwin (No numbers).\n");
                return GL_FALSE;
            } else {
                nWin = atoi(argv[++i]);
            }
	} else if (strcmp(argv[i], "-grid") == 0) {
	    if (i+2 >= argc || argv[i+1][0] == '-' || argv[i+2][0] == '-') {
		printf("-grid (No numbers).\n");
		return GL_FALSE;
	    } else {
		widthX = atoi(argv[++i]);
		widthY = atoi(argv[++i]);
	    }
	} else if (strcmp(argv[i], "-size") == 0) {
	    if (i+1 >= argc || argv[i+1][0] == '-') {
		printf("-checker (No number).\n");
		return GL_FALSE;
	    } else {
		checkerSize = atoi(argv[++i]);
	    }
	} else if (strcmp(argv[i], "-wave") == 0) {
	    if (i+1 >= argc || argv[i+1][0] == '-') {
		printf("-wave (No number).\n");
		return GL_FALSE;
	    } else {
		height = atof(argv[++i]);
	    }
	} else if (strcmp(argv[i], "-frames") == 0) {
	    if (i+1 >= argc || argv[i+1][0] == '-') {
		printf("-frames (No number).\n");
		return GL_FALSE;
	    } else {
		frames = atoi(argv[++i]);
	    }
	} else {
	    printf("%s (Bad option).\n", argv[i]);
	    return GL_FALSE;
	}
    }
    return GL_TRUE;
}

void __cdecl main(int argc, char **argv)
{
    DWORD dwBaseFlags;
    DWORD dwFlags;
    GLWINDOW gw;
    int iWidth, iHeight;
    int x, y;
    int i;
    char achName[80];

    if (Args(argc, argv) == GL_FALSE) {
	exit(1);
    }

    InitMesh();

    dwBaseFlags = useMcd ? GLWIN_GENERIC_ACCELERATED : 0;
    
    x = 10;
    y = 30;
    iWidth = 320;
    iHeight = 320;

    for (i = 0; i < nWin; i++)
    {
        dwFlags = (adwFlags[i] | dwBaseFlags) ^
            (doubleBuffer ? GLWIN_BACK_BUFFER : 0);
        sprintf(achName, "Wave Demo Window %d Flags 0x%lX", i, dwFlags);
        gw = glwinCreateWindow(NULL, achName, x, y, iWidth, iHeight, dwFlags);
        if (gw == NULL)
        {
            printf("glwinCreateWindow failed with 0x%08lX\n",
                   glwinGetLastError());
            Quit();
        }
        gwWin[i] = gw;
        wswaps[i].hdc = glwinGetHdc(gw);
        wswaps[i].uiFlags = WGL_SWAP_MAIN_PLANE;
        glwinMakeCurrent(gw);
        Init();
        glwinMessageCallback(gw, Message);

#ifdef HIDE_UNDER
        if (i < nWin-1)
        {
            ShowWindow(glwinGetHwnd(gw), SW_HIDE);
        }
#endif
#ifdef STAGGER_WINDOWS
        x += iWidth/3;
        y += iHeight/3;
#endif
    }

    iFront = i-1;
    glwinRun(AnimateN);

    Quit();
}
