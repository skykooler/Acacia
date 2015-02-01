    /*------------------------------------------------------------------------
 * A demonstration of OpenGL in a  ARGB window
 *    => support for composited window transparency
 *
 * (c) 2011 by Wolfgang 'datenwolf' Draxinger
 *     See me at comp.graphics.api.opengl and StackOverflow.com

 * License agreement: This source code is provided "as is". You
 * can use this source code however you want for your own personal
 * use. If you give this source code to anybody else then you must
 * leave this message in it.
 *
 * This program is based on the simplest possible
 * Linux OpenGL program by FTB (see info below)

  The simplest possible Linux OpenGL program? Maybe...

  (c) 2002 by FTB. See me in comp.graphics.api.opengl

  --
  <\___/>
  / O O \
  \_____/  FTB.

------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <png.h>
#include <assert.h>
#include <dirent.h>
#include <sys/time.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xutil.h>

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include <freetype/ftoutln.h>
#include <freetype/fttrigon.h>

#include <string>
#include <vector>
#include <algorithm>

#include <wctype.h>

#define USE_CHOOSE_FBCONFIG
#define PNG_SIG_BYTES 8

#define TYPE_FILE 0
#define TYPE_FOLDER 1

using namespace std;

static void fatalError(const char *why)
{
    fprintf(stderr, "%s", why);
    exit(0x666);
}

static enum {
    TEXT,
    DOCUMENT,
    IMAGE,
    CODE,
    COMPRESSED,
    AUDIO,
    BINARY
} filetypes;

typedef struct  {
    unsigned long   flags;
    unsigned long   functions;
    unsigned long   decorations;
    long            inputMode;
    unsigned long   status;
} Hints;

Hints rhints;
Atom property;

float Opacity = 0.75;
float Falloff = 0.5;
float scaleFactor = 0.2;
const double pi = 3.14159265359;

float red = 0.5;
float green = 0.75;
float blue = 1.0;

int mouse_x, mouse_y;
int button;
long key;
bool mousecontrol = false;

static int Xscreen;
static Atom del_atom;
static Colormap cmap;
static Display *Xdisplay;
static XVisualInfo *visual;
static XRenderPictFormat *pict_format;
static GLXFBConfig *fbconfigs, fbconfig;
static int numfbconfigs;
static GLXContext render_context;
static Window Xroot, window_handle;
static GLXWindow glX_window_handle;
static int width, height;

static int VisData[] = {
GLX_RENDER_TYPE, GLX_RGBA_BIT,
GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
GLX_DOUBLEBUFFER, True,
GLX_RED_SIZE, 8,
GLX_GREEN_SIZE, 8,
GLX_BLUE_SIZE, 8,
GLX_ALPHA_SIZE, 8,
GLX_DEPTH_SIZE, 16,
None
};
 
// This Holds All Of The Information Related To Any
// FreeType Font That We Want To Create. 
struct font_data {
    float h;                                        // Holds The Height Of The Font.
    GLuint * textures;                                  // Holds The Texture Id's
    GLuint list_base;                                   // Holds The First Display List Id
 
    // The Init Function Will Create A Font With
    // The Height h From The File fname.
    void init(const char * fname, unsigned int h);
 
    // Free All The Resources Associated With The Font.
        void clean();
};

inline int next_p2 (int a )
{
    int rval=1;
    // rval<<=1 Is A Prettier Way Of Writing rval*=2;
    while(rval<a) rval<<=1;
    return rval;
}

// Create A Display List Corresponding To The Given Character.
void make_dlist ( FT_Face face, char ch, GLuint list_base, GLuint * tex_base ) {
 
    // The First Thing We Do Is Get FreeType To Render Our Character
    // Into A Bitmap.  This Actually Requires A Couple Of FreeType Commands:
 
    // Load The Glyph For Our Character.
    if(FT_Load_Glyph( face, FT_Get_Char_Index( face, ch ), FT_LOAD_DEFAULT )) {
        printf("ERROR: FT_Load_Glyph failed\n");
        return;
    }
 
    // Move The Face's Glyph Into A Glyph Object.
    FT_Glyph glyph;
    if(FT_Get_Glyph( face->glyph, &glyph )) {
        printf("ERROR: FT_Get_Glyph failed\n");
        return;
    }
 
    // Convert The Glyph To A Bitmap.
    FT_Glyph_To_Bitmap( &glyph, ft_render_mode_normal, 0, 1 );
    FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)glyph;
 
    // This Reference Will Make Accessing The Bitmap Easier.
    FT_Bitmap& bitmap=bitmap_glyph->bitmap;
    // Use Our Helper Function To Get The Widths Of
    // The Bitmap Data That We Will Need In Order To Create
    // Our Texture.
    int width = next_p2( bitmap.width );
    int height = next_p2( bitmap.rows );
     
    // Allocate Memory For The Texture Data.
    GLubyte* expanded_data = new GLubyte[ 2 * width * height];
     
    // Here We Fill In The Data For The Expanded Bitmap.
    // Notice That We Are Using A Two Channel Bitmap (One For
    // Channel Luminosity And One For Alpha), But We Assign
    // Both Luminosity And Alpha To The Value That We
    // Find In The FreeType Bitmap.
    // We Use The ?: Operator To Say That Value Which We Use
    // Will Be 0 If We Are In The Padding Zone, And Whatever
    // Is The FreeType Bitmap Otherwise.
    for(int j=0; j <height;j++) {
        for(int i=0; i < width; i++){
            expanded_data[2*(i+j*width)]= expanded_data[2*(i+j*width)+1] =
                (i>=bitmap.width || j>=bitmap.rows) ?
                0 : bitmap.buffer[i + bitmap.width*j];
        }
    }
    // Now We Just Setup Some Texture Parameters.
    glBindTexture( GL_TEXTURE_2D, tex_base[ch]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
     
    // Here We Actually Create The Texture Itself, Notice
    // That We Are Using GL_LUMINANCE_ALPHA To Indicate That
    // We Are Using 2 Channel Data.
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
        GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, expanded_data );
     
    // With The Texture Created, We Don't Need The Expanded Data Anymore.
    delete [] expanded_data;
    // Now We Create The Display List
    glNewList(list_base+ch,GL_COMPILE);
 
    glBindTexture(GL_TEXTURE_2D,tex_base[ch]);
 
    glPushMatrix();
 
    // First We Need To Move Over A Little So That
    // The Character Has The Right Amount Of Space
    // Between It And The One Before It.
    glTranslatef(bitmap_glyph->left,0,0);
 
    // Now We Move Down A Little In The Case That The
    // Bitmap Extends Past The Bottom Of The Line
    // This Is Only True For Characters Like 'g' Or 'y'.
    glTranslatef(0,bitmap_glyph->top-bitmap.rows,0);
 
    // Now We Need To Account For The Fact That Many Of
    // Our Textures Are Filled With Empty Padding Space.
    // We Figure What Portion Of The Texture Is Used By
    // The Actual Character And Store That Information In
    // The x And y Variables, Then When We Draw The
    // Quad, We Will Only Reference The Parts Of The Texture
    // That Contains The Character Itself.
    float   x=(float)bitmap.width / (float)width,
    y=(float)bitmap.rows / (float)height;
 
    // Here We Draw The Texturemapped Quads.
    // The Bitmap That We Got From FreeType Was Not
    // Oriented Quite Like We Would Like It To Be,
    // But We Link The Texture To The Quad
    // In Such A Way That The Result Will Be Properly Aligned.
    glBegin(GL_QUADS);
    glTexCoord2d(0,0); glVertex2f(0,bitmap.rows);
    glTexCoord2d(0,y); glVertex2f(0,0);
    glTexCoord2d(x,y); glVertex2f(bitmap.width,0);
    glTexCoord2d(x,0); glVertex2f(bitmap.width,bitmap.rows);
    glEnd();
    glPopMatrix();
    glTranslatef(face->glyph->advance.x >> 6 ,0,0);
 
    // Increment The Raster Position As If We Were A Bitmap Font.
    // (Only Needed If You Want To Calculate Text Length)
    glBitmap(0,0,0,0,face->glyph->advance.x >> 6,0,NULL);
 
    // Finish The Display List
    glEndList();
}

void font_data::init(const char * fname, unsigned int h) {
    // Allocate Some Memory To Store The Texture Ids.
    textures = new GLuint[128];
 
    this->h=h;
 
    // Create And Initilize A FreeType Font Library.
    FT_Library library;
    if (FT_Init_FreeType( &library )) {
        printf("ERROR: FT_Init_FreeType failed\n");
        return;
    }
 
    // The Object In Which FreeType Holds Information On A Given
    // Font Is Called A "face".
    FT_Face face;
 
    // This Is Where We Load In The Font Information From The File.
    // Of All The Places Where The Code Might Die, This Is The Most Likely,
    // As FT_New_Face Will Fail If The Font File Does Not Exist Or Is Somehow Broken.
    if (FT_New_Face( library, fname, 0, &face )) {
        printf("ERROR: FT_New_Face failed (there is probably a problem with your font file)\n");
        return;
    }
 
    // For Some Twisted Reason, FreeType Measures Font Size
    // In Terms Of 1/64ths Of Pixels.  Thus, To Make A Font
    // h Pixels High, We Need To Request A Size Of h*64.
    // (h << 6 Is Just A Prettier Way Of Writing h*64)
    FT_Set_Char_Size( face, h << 6, h << 6, 96, 96);
 
    // Here We Ask OpenGL To Allocate Resources For
    // All The Textures And Display Lists Which We
    // Are About To Create. 
    list_base=glGenLists(128);
    glGenTextures( 128, textures );
 
    // This Is Where We Actually Create Each Of The Fonts Display Lists.
    for(unsigned char i=0;i<128;i++)
        make_dlist(face,i,list_base,textures);
 
    // We Don't Need The Face Information Now That The Display
    // Lists Have Been Created, So We Free The Assosiated Resources.
    FT_Done_Face(face);
 
    // Ditto For The Font Library.
    FT_Done_FreeType(library);
}

void font_data::clean() {
    glDeleteLists(list_base,128);
    glDeleteTextures(128,textures);
    delete [] textures;
}

// A Fairly Straightforward Function That Pushes
// A Projection Matrix That Will Make Object World
// Coordinates Identical To Window Coordinates.
inline void pushScreenCoordinateMatrix() {
    glPushAttrib(GL_TRANSFORM_BIT);
    GLint   viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(viewport[0],viewport[2],viewport[1],viewport[3]);
    glPopAttrib();
}
 
// Pops The Projection Matrix Without Changing The Current
// MatrixMode.
inline void pop_projection_matrix() {
    glPushAttrib(GL_TRANSFORM_BIT);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}

// Much Like NeHe's glPrint Function, But Modified To Work
// With FreeType Fonts.
float gprint(const font_data &ft_font, float x, float y, const char *fmt, ...)  {
    glPushMatrix();
    glTranslatef(x,y,0.0);
    GLuint font=ft_font.list_base;
    // We Make The Height A Little Bigger.  There Will Be Some Space Between Lines.
    //float h=ft_font.h/.63f;                                                
    char    text[256];                                  // Holds Our String
    va_list ap;                                     // Pointer To List Of Arguments
    
    if (fmt == NULL)                                    // If There's No Text
        *text=0;                                    // Do Nothing
    else {
        va_start(ap, fmt);                              // Parses The String For Variables
        vsprintf(text, fmt, ap);                            // And Converts Symbols To Actual Numbers
        va_end(ap);                                 // Results Are Stored In Text
    }
    
    // Here Is Some Code To Split The Text That We Have Been
    // Given Into A Set Of Lines. 
    // This Could Be Made Much Neater By Using
    // A Regular Expression Library Such As The One Available From
    // boost.org (I've Only Done It Out By Hand To Avoid Complicating
    // This Tutorial With Unnecessary Library Dependencies).
    
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);     
    
    glListBase(font);
    
    string line(text);
    
    glRasterPos2f(0,0);
    glCallLists(line.length(), GL_UNSIGNED_BYTE, text);

    float rpos[4];
    glGetFloatv(GL_CURRENT_RASTER_POSITION ,rpos);
    float len=(rpos[0]-x)/16;

    glPopMatrix();

    return len;
}

font_data our_font;

struct timeval earlier;
struct timeval later;

long long delta_t;

long long timeval_diff(struct timeval *difference, struct timeval *end_time, struct timeval *start_time) {
  struct timeval temp_diff;

  if(difference==NULL) {
    difference=&temp_diff;
  }

  difference->tv_sec =end_time->tv_sec -start_time->tv_sec;
  difference->tv_usec=end_time->tv_usec-start_time->tv_usec;

  /* Using while instead of if below makes the code slightly more robust. */

  while(difference->tv_usec<0) {
    difference->tv_usec+=1000000;
    difference->tv_sec -=1;
  }

  return 1000000LL*difference->tv_sec+difference->tv_usec;

} /* timeval_diff() */

void clickRoot();
void pressRoot();

static int isExtensionSupported(const char *extList, const char *extension) {

  const char *start;
  const char *where, *terminator;

  /* Extension names should not have spaces. */
  where = strchr(extension, ' ');
  if ( where || *extension == '\0' )
    return 0;

  /* It takes a bit of care to be fool-proof about parsing the
     OpenGL extensions string. Don't be fooled by sub-strings,
     etc. */
  for ( start = extList; ; ) {
    where = strstr( start, extension );

    if ( !where )
      break;

    terminator = where + strlen( extension );

    if ( where == start || *(where - 1) == ' ' )
      if ( *terminator == ' ' || *terminator == '\0' )
    return 1;

    start = terminator;
  }
  return 0;
}

static Bool WaitForMapNotify(Display *d, XEvent *e, char *arg) {
    return d && e && arg && (e->type == MapNotify) && (e->xmap.window == *(Window*)arg);
}

static void describe_fbconfig(GLXFBConfig fbconfig) {
    int doublebuffer;
    int red_bits, green_bits, blue_bits, alpha_bits, depth_bits;

    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_DOUBLEBUFFER, &doublebuffer);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_RED_SIZE, &red_bits);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_GREEN_SIZE, &green_bits);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_BLUE_SIZE, &blue_bits);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_ALPHA_SIZE, &alpha_bits);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_DEPTH_SIZE, &depth_bits);

    fprintf(stderr, "FBConfig selected:\n"
    "Doublebuffer: %s\n"
    "Red Bits: %d, Green Bits: %d, Blue Bits: %d, Alpha Bits: %d, Depth Bits: %d\n",
    doublebuffer == True ? "Yes" : "No",
    red_bits, green_bits, blue_bits, alpha_bits, depth_bits);
}

static void createTheWindow() {
    XEvent event;
    int x,y, attr_mask;
    XSizeHints hints;
    XWMHints *startup_state;
    XTextProperty textprop;
    XSetWindowAttributes attr = {0,};
    static const char *title = "Acacia";

    Xdisplay = XOpenDisplay(NULL);
    if (!Xdisplay) {
    fatalError("Couldn't connect to X server\n");
    }
    Xscreen = DefaultScreen(Xdisplay);
    Xroot = RootWindow(Xdisplay, Xscreen);

    fbconfigs = glXChooseFBConfig(Xdisplay, Xscreen, VisData, &numfbconfigs);
    fbconfig = 0;
    int i;
    for(i = 0; i<numfbconfigs; i++) {
        visual = (XVisualInfo*) glXGetVisualFromFBConfig(Xdisplay, fbconfigs[i]);
        if(!visual)
            continue;

        pict_format = XRenderFindVisualFormat(Xdisplay, visual->visual);
        if(!pict_format)
            continue;

        fbconfig = fbconfigs[i];
        if(pict_format->direct.alphaMask > 0) {
            break;
        }
    }

    if(!fbconfig) {
    fatalError("No matching FB config found");
    }

    describe_fbconfig(fbconfig);

    /* Create a colormap - only needed on some X clients, eg. IRIX */
    cmap = XCreateColormap(Xdisplay, Xroot, visual->visual, AllocNone);

    attr.colormap = cmap;
    attr.background_pixmap = None;
    attr.border_pixmap = None;
    attr.border_pixel = 0;
    attr.event_mask =
    StructureNotifyMask |
    EnterWindowMask |
    LeaveWindowMask |
    ExposureMask |
    ButtonPressMask |
    ButtonReleaseMask |
    PointerMotionMask |
    Button1MotionMask |
    OwnerGrabButtonMask |
    KeyPressMask |
    KeyReleaseMask;

    attr_mask =
    CWBackPixmap|
    CWColormap|
    CWBorderPixel|
    CWEventMask;

    width = DisplayWidth(Xdisplay, DefaultScreen(Xdisplay));//2;
    height = DisplayHeight(Xdisplay, DefaultScreen(Xdisplay));//2;
    x=0, y=0;

    window_handle = XCreateWindow(  Xdisplay,
            Xroot,
            x, y, width, height,
            0,
            visual->depth,
            InputOutput,
            visual->visual,
            attr_mask, &attr);

    if( !window_handle ) {
    fatalError("Couldn't create the window\n");
    }

    #if USE_GLX_CREATE_WINDOW
        int glXattr[] = { None };
        glX_window_handle = glXCreateWindow(Xdisplay, fbconfig, window_handle, glXattr);
        if( !glX_window_handle ) {
        fatalError("Couldn't create the GLX window\n");
        }
    #else
        glX_window_handle = window_handle;
    #endif

    textprop.value = (unsigned char*)title;
    textprop.encoding = XA_STRING;
    textprop.format = 8;
    textprop.nitems = strlen(title);

    hints.x = x;
    hints.y = y;
    hints.width = width;
    hints.height = height;
    hints.flags = USPosition|USSize;

    startup_state = XAllocWMHints();
    startup_state->initial_state = NormalState;
    startup_state->flags = StateHint;

    XSetWMProperties(Xdisplay, window_handle,&textprop, &textprop,
        NULL, 0,
        &hints,
        startup_state,
        NULL);


    XFree(startup_state);

    XMapWindow(Xdisplay, window_handle);
    XIfEvent(Xdisplay, &event, WaitForMapNotify, (char*)&window_handle);

    /*****************************/
    rhints.flags = 2;        // Specify that we're changing the window decorations.
    rhints.decorations = 0;  // 0 (false) means that window decorations should go bye-bye.
    property = XInternAtom(Xdisplay,"_MOTIF_WM_HINTS",True);
    XChangeProperty(Xdisplay,window_handle,property,property,32,PropModeReplace,(unsigned char *)&rhints,5);

    // XMoveResizeWindow(Xdisplay,window_handle,0,0,width,height);

    XMapRaised(Xdisplay,window_handle);

    XGrabPointer(Xdisplay,window_handle,True,0,GrabModeAsync,GrabModeAsync,window_handle,0L,CurrentTime);

    // XGrabKeyboard(Xdisplay,window_handle,False,GrabModeAsync,GrabModeAsync,CurrentTime);
    /*****************************/


    if ((del_atom = XInternAtom(Xdisplay, "WM_DELETE_WINDOW", 0)) != None) {
    XSetWMProtocols(Xdisplay, window_handle, &del_atom, 1);
    }
}

static int ctxErrorHandler( Display *dpy, XErrorEvent *ev ) {
    fputs("Error at context creation", stderr);
    return 0;
}

static void createTheRenderContext() {
    int dummy;
    if (!glXQueryExtension(Xdisplay, &dummy, &dummy)) {
    fatalError("OpenGL not supported by X server\n");
    }

#if USE_GLX_CREATE_CONTEXT_ATTRIB
    #define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
    #define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
    render_context = NULL;
    if( isExtensionSupported( glXQueryExtensionsString(Xdisplay, DefaultScreen(Xdisplay)), "GLX_ARB_create_context" ) ) {
    typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );
    if( glXCreateContextAttribsARB ) {
        int context_attribs[] =
        {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 0,
        //GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        None
        };

        int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(&ctxErrorHandler);

        render_context = glXCreateContextAttribsARB( Xdisplay, fbconfig, 0, True, context_attribs );

        XSync( Xdisplay, False );
        XSetErrorHandler( oldHandler );

        fputs("glXCreateContextAttribsARB failed", stderr);
    } else {
        fputs("glXCreateContextAttribsARB could not be retrieved", stderr);
    }
    } else {
        fputs("glXCreateContextAttribsARB not supported", stderr);
    }

    if(!render_context)
    {
#else
    {
#endif
    render_context = glXCreateNewContext(Xdisplay, fbconfig, GLX_RGBA_TYPE, 0, True);
    if (!render_context) {
        fatalError("Failed to create a GL context\n");
    }
    }

    if (!glXMakeContextCurrent(Xdisplay, glX_window_handle, glX_window_handle, render_context)) {
    fatalError("glXMakeCurrent failed for window\n");
    }
}

static int updateTheMessageQueue() {
    XEvent event;
    XConfigureEvent *xc;

    while (XPending(Xdisplay))
    {
        XNextEvent(Xdisplay, &event);
        switch (event.type)
        {
        case ClientMessage:
            if (event.xclient.data.l[0] == del_atom)
            {
            return 0;
            }
        break;

        case ButtonPress:
            switch(event.xbutton.button){
                case Button1:
                    mouse_x=event.xbutton.x;
                    mouse_y=height-event.xbutton.y;
                    button=Button1;
                    clickRoot();
                    printf("Mouse position: %i %i Button: %i\n", mouse_x,mouse_y,button);
                    break;
                case Button3:
                    mouse_x=event.xbutton.x;
                    mouse_y=height-event.xbutton.y;
                    button=Button3;
                    break;
                default:
                    break;
            }
            break;

        case MotionNotify:
            mouse_x = event.xbutton.x;
            mouse_y = height-event.xbutton.y;
            mousecontrol = true;
            break;

        case ConfigureNotify:
            xc = &(event.xconfigure);
            width = xc->width;
            height = xc->height;
            break;
        case KeyPress:
            key = XLookupKeysym (&event.xkey, 0);
            mousecontrol = false;
            pressRoot();
            break;
        case KeyRelease:
            printf ("key #%ld was released.\n",
             (long) XLookupKeysym (&event.xkey, 0));
            break;
        }
    }
    return 1;
}

/*  6----7
   /|   /|
  3----2 |
  | 5--|-4
  |/   |/
  0----1

*/

GLfloat cube_vertices[][8] =  {
    /*  X     Y     Z   Nx   Ny   Nz    S    T */
    {-1.0, -1.0,  1.0, 0.0, 0.0, 1.0, 0.0, 0.0}, // 0
    { 1.0, -1.0,  1.0, 0.0, 0.0, 1.0, 1.0, 0.0}, // 1
    { 1.0,  1.0,  1.0, 0.0, 0.0, 1.0, 1.0, 1.0}, // 2
    {-1.0,  1.0,  1.0, 0.0, 0.0, 1.0, 0.0, 1.0}, // 3

    { 1.0, -1.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0}, // 4
    {-1.0, -1.0, -1.0, 0.0, 0.0, -1.0, 1.0, 0.0}, // 5
    {-1.0,  1.0, -1.0, 0.0, 0.0, -1.0, 1.0, 1.0}, // 6
    { 1.0,  1.0, -1.0, 0.0, 0.0, -1.0, 0.0, 1.0}, // 7

    {-1.0, -1.0, -1.0, -1.0, 0.0, 0.0, 0.0, 0.0}, // 5
    {-1.0, -1.0,  1.0, -1.0, 0.0, 0.0, 1.0, 0.0}, // 0
    {-1.0,  1.0,  1.0, -1.0, 0.0, 0.0, 1.0, 1.0}, // 3
    {-1.0,  1.0, -1.0, -1.0, 0.0, 0.0, 0.0, 1.0}, // 6

    { 1.0, -1.0,  1.0,  1.0, 0.0, 0.0, 0.0, 0.0}, // 1
    { 1.0, -1.0, -1.0,  1.0, 0.0, 0.0, 1.0, 0.0}, // 4
    { 1.0,  1.0, -1.0,  1.0, 0.0, 0.0, 1.0, 1.0}, // 7
    { 1.0,  1.0,  1.0,  1.0, 0.0, 0.0, 0.0, 1.0}, // 2

    {-1.0, -1.0, -1.0,  0.0, -1.0, 0.0, 0.0, 0.0}, // 5
    { 1.0, -1.0, -1.0,  0.0, -1.0, 0.0, 1.0, 0.0}, // 4
    { 1.0, -1.0,  1.0,  0.0, -1.0, 0.0, 1.0, 1.0}, // 1
    {-1.0, -1.0,  1.0,  0.0, -1.0, 0.0, 0.0, 1.0}, // 0

    {-1.0, 1.0,  1.0,  0.0,  1.0, 0.0, 0.0, 0.0}, // 3
    { 1.0, 1.0,  1.0,  0.0,  1.0, 0.0, 1.0, 0.0}, // 2
    { 1.0, 1.0, -1.0,  0.0,  1.0, 0.0, 1.0, 1.0}, // 7
    {-1.0, 1.0, -1.0,  0.0,  1.0, 0.0, 0.0, 1.0}, // 6
};

static void draw_cube(void) {
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glVertexPointer(3, GL_FLOAT, sizeof(GLfloat) * 8, &cube_vertices[0][0]);
    glNormalPointer(GL_FLOAT, sizeof(GLfloat) * 8, &cube_vertices[0][3]);
    glTexCoordPointer(2, GL_FLOAT, sizeof(GLfloat) * 8, &cube_vertices[0][6]);

    glDrawArrays(GL_QUADS, 0, 24);
}

float const light0_dir[]={0,1,0,0};
float const light0_color[]={78./255., 80./255., 184./255.,1};

float const light1_dir[]={-1,1,1,0};
float const light1_color[]={255./255., 220./255., 97./255.,1};

float const light2_dir[]={0,-1,0,0};
float const light2_color[]={31./255., 75./255., 16./255.,1};

void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a=1) {
    glColor4f(r,g,b,a);
    glDisable(GL_TEXTURE_2D);
    const GLshort v[8] = { x, y, x+w, y, x+w, y+h, x, y+h };

    glVertexPointer( 2, GL_SHORT, 0, v );
    glEnableClientState( GL_VERTEX_ARRAY );
    
    glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
}

void draw_img(float x, float y, float w, float h, GLuint tex,float r=1,float g=1, float b=1, float a=1) {
        glColor4f(r,g,b,a);
        const GLshort t[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
        const GLshort v[8] = { x, y, x+w, y, x+w, y+h, x, y+h };

        glVertexPointer( 2, GL_SHORT, 0, v );
        glEnableClientState( GL_VERTEX_ARRAY );
    
        glTexCoordPointer( 2, GL_SHORT, 0, t );
        glEnableClientState( GL_TEXTURE_COORD_ARRAY );
        
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex);
        glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );//*/

    }

typedef struct {
        GLuint tex;
        int width;
        int height;
} Img;


char * load_png(char *name, int *width, int *height) {
    FILE *png_file = fopen(name, "rb");
    assert(png_file);

    uint8_t header[PNG_SIG_BYTES];

    fread(header, 1, PNG_SIG_BYTES, png_file);
    assert(!png_sig_cmp(header, 0, PNG_SIG_BYTES));

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    assert(png_ptr);

    png_infop info_ptr = png_create_info_struct(png_ptr);
    assert(info_ptr);

    png_infop end_info = png_create_info_struct(png_ptr);
    assert(end_info);

    assert(!setjmp(png_jmpbuf(png_ptr)));
    png_init_io(png_ptr, png_file);
    png_set_sig_bytes(png_ptr, PNG_SIG_BYTES);
    png_read_info(png_ptr, info_ptr);

    *width = png_get_image_width(png_ptr, info_ptr);
    *height = png_get_image_height(png_ptr, info_ptr);

    png_uint_32 bit_depth, color_type;
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
            
    if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    if (bit_depth == 16)
            png_set_strip_16(png_ptr);
            
    if(color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    else if(color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        {
            png_set_gray_to_rgb(png_ptr);
        }

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);
    else
        png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

    png_read_update_info(png_ptr, info_ptr);

    png_uint_32 rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    png_uint_32 numbytes = rowbytes*(*height);
    png_byte* pixels = (png_byte*)malloc(numbytes);
    png_byte** row_ptrs = (png_byte**)malloc((*height) * sizeof(png_byte*));

    int i;
    for (i=0; i<(*height); i++)
      row_ptrs[i] = pixels + ((*height) - 1 - i)*rowbytes;

    png_read_image(png_ptr, row_ptrs);

    free(row_ptrs);
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    fclose(png_file);

    return (char *)pixels;
}

Img load_img(const char *filename) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    
    int w, h;
    GLubyte *pixels = (GLubyte *)load_png((char*)filename, &w, &h);
    gluBuild2DMipmaps( GL_TEXTURE_2D, 4, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels );
    free(pixels);
    Img img;
    img.tex = tex;
    img.width = w;
    img.height = h;
    return img;
}

Img circle;
Img line;

class File {
    public:
        File();
        File(string);
        void draw(float opacity=1, bool isSelected=false);
        string path;
        string name;
        float toffset;
        int nodetype;
        int filetype;
        float hoverScaleFactor;
        float hoverVelocity;
        float selectedOpacity;
        float selectedVelocity;
    /* data */
};
File::File() {
    name = "";
    toffset = 32767;
    nodetype = TYPE_FILE;
    filetype = BINARY;
    hoverScaleFactor = 0;
    hoverVelocity = 0;
    selectedOpacity = 0;
    selectedVelocity = 0;
}
File::File(string pth) {
    path = pth;
    nodetype = TYPE_FILE;
    toffset = 32767;

    uint idx = pth.rfind('.');

    if(idx != std::string::npos) {
        std::string extension = pth.substr(idx+1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
        if (extension == "txt") {
            filetype = TEXT;
        } else if (extension == "png" ||
                   extension == "jpg" ||
                   extension == "jpeg" ||
                   extension == "gif" ||
                   extension == "bmp" ||
                   extension == "tif" ||
                   extension == "tiff" ||
                   extension == "tga") {
            filetype = IMAGE;
        } else if (extension == "py" ||
                   extension == "c" ||
                   extension == "c++" ||
                   extension == "cpp" ||
                   extension == "rb" ||
                   extension == "php" ||
                   extension == "js" ||
                   extension == "java") {
            filetype = CODE;
        } else if (extension == "doc" ||
                   extension == "docx" ||
                   extension == "pdf" ||
                   extension == "html" ||
                   extension == "odt" ||
                   extension == "rtf") {
            filetype = DOCUMENT;
        } else if (extension == "aif" ||
                   extension == "aiff" ||
                   extension == "mp3" ||
                   extension == "wav" ||
                   extension == "ogg" ||
                   extension == "flac") {
            filetype = AUDIO;
        } else if (extension == "zip" ||
                   extension == "bzip" ||
                   extension == "bzip2" ||
                   extension == "bz2" ||
                   extension == "gz" ||
                   extension == "tar" ||
                   extension == "xz" ||
                   extension == "7z") {
            filetype = COMPRESSED;
        } else {
            filetype = BINARY;
        }
    } else {
        // File has no extension
        filetype = BINARY;
    }
    idx = pth.rfind('/');
    if(idx != std::string::npos) {
        name = pth.substr(idx+1);
    }
    hoverScaleFactor = 0;
    hoverVelocity = 0;
    selectedOpacity = 0;
    selectedVelocity = 0;
}
void File::draw(float opacity, bool isSelected) {
    float fred;
    float fgreen;
    float fblue;
    switch (filetype) {
        case TEXT :
            fred = 1.0;
            fgreen = 0.5;
            fblue = 1.0;
            break;
        case DOCUMENT :
            fred = 0.5;
            fgreen = 0.5;
            fblue = 1.0;
            break;
        case CODE :
            fred = 1.0;
            fgreen = 1.0;
            fblue = 0.5;
            break;
        case COMPRESSED :
            fred = 1.0;
            fgreen = 1.0;
            fblue = 1.0;
            break;
        case IMAGE :
            fred = 0.5;
            fgreen = 1.0;
            fblue = 0.5;
            break;
        case AUDIO :
            fred = 1.0;
            fgreen = 0.5;
            fblue = 0.5;
            break;
        case BINARY :
        default :
            fred = 0.75;
            fgreen = 0.75;
            fblue = 0.5;
    }
    float selectedVelocityTarget;
    if (isSelected) {
        selectedVelocityTarget = ((1-selectedOpacity)/100000);
    } else {
        selectedVelocityTarget = ((opacity-selectedOpacity)/100000);
    }
    // Exponentially ease velocity towards target
    selectedVelocity = selectedVelocity+((selectedVelocityTarget-selectedVelocity)/100000)*delta_t;
    // Yes, I'm using Euler integration. So sue me. It's fast, and the decaying exponentials cancel out any energy gain.
    selectedOpacity = selectedOpacity+selectedVelocity*delta_t;

    draw_img(-128,-128,256,256,circle.tex,fred,fgreen,fblue,selectedOpacity);
}

File FILE_NOT_FOUND;

class Folder : public File {
    public:
        // Get initializer from File. Probably.
        Folder(string);// : File(string) {};
        vector<File *> children;
        // void read();
        void read(int);
        void draw(float opacity=1, int level=1, bool isSelected=false);
        void click(float opacity=1);
        void transform(float opacity=1);
        Folder * openChild;
        int selectedChildIndex;
};

void GetFilesInDirectory(std::vector<File *> &out, const string &directory, int levelsleft) {
    
    DIR *dpdf;
    struct dirent *epdf;

    out.clear();
    dpdf = opendir(directory.c_str());
    if (dpdf != NULL){
       while (epdf = readdir(dpdf)){
            if (epdf->d_name[0] == '.')
                continue;
            string thefile;
            thefile.append(directory);
            thefile.append(epdf->d_name);
            if (epdf->d_type==DT_REG) {
                // printf("%s: %s\n", "File", thefile.c_str());
                out.push_back(new File(thefile));
            } else if (epdf->d_type==DT_DIR) {
                thefile.append("/");
                // printf("%s: %s\n", "Folder",thefile.c_str());
                Folder *f = new Folder(thefile);
                if (levelsleft>0) {
                    (*f).read(levelsleft-1);
                }
                out.push_back(f);
            }
            // printf("Filename: %s\n",epdf->d_name);
       }
    }
}

Folder::Folder(string pth) {
    toffset = 32767;
    path = pth;
    uint idx = pth.rfind('/',pth.size()-2);
    if(idx != std::string::npos) {
        name = pth.substr(idx+1);
    } else {
        name = "";
    }
    nodetype = TYPE_FOLDER;
    hoverScaleFactor = 0;
    hoverVelocity = 0;
    selectedOpacity = 0;
    selectedVelocity = 0;
    openChild = (Folder *)&FILE_NOT_FOUND;
    selectedChildIndex = -1;
}

Folder root("/home/skyler/Dropbox/");
void Folder::read(int levelsleft=1) {
    GetFilesInDirectory(children,path,levelsleft);
}
void Folder::draw(float opacity, int level, bool isSelected) {
    glPushMatrix();
    int last_mouse_x = mouse_x;
    int last_mouse_y = mouse_y;
    
    float selectedVelocityTarget;
    if (isSelected) {
        selectedVelocityTarget = ((1-selectedOpacity)/100000);
    } else {
        selectedVelocityTarget = ((opacity-selectedOpacity)/100000);
    }
    // Exponentially ease velocity towards target
    selectedVelocity = selectedVelocity+((selectedVelocityTarget-selectedVelocity)/100000)*delta_t;
    // Yes, I'm using Euler integration. So sue me. It's fast, and the decaying exponentials cancel out any energy gain.
    selectedOpacity = selectedOpacity+selectedVelocity*delta_t;

    draw_img(-128,-128,256,256,circle.tex,red,green,blue,selectedOpacity);
    
    float mousetheta;
    if (mouse_x != 0) {
        mousetheta = fmod(-atan2 ((float)mouse_y, (float)mouse_x) + 2.5*pi, 2*pi);
    } else {
        mousetheta = mouse_y > 0 ? 0 : pi;
    }

    int sub_last_mouse_x, sub_last_mouse_y;
    int s = children.size();

    int closestChild;
    if (sqrt(mouse_x*mouse_x + mouse_y*mouse_y) < 256*opacity + 32 && s > 0 && mousecontrol) {
        closestChild = ((int)(s*mousetheta/(pi*2)+0.5)) % s;
        selectedChildIndex = closestChild;
    } else {
        closestChild = -1;
    }

    if (level==1) {
        glEnable(GL_TEXTURE_2D);
        glColor3f(1,1,1);
        if (selectedChildIndex == -1) {
            toffset = -gprint(our_font, toffset, -128, name.c_str());
        } else {
            toffset = -gprint(our_font, toffset, -128, (*children.at(selectedChildIndex)).name.c_str());
        }
    }

    if (level<4) {
        for (int i=0; i<s; i++) {
            float theta = ((float)i)/((float)(children.size()))*2*pi;
            float x = 256*opacity*sin(theta);
            float y = 256*opacity*cos(theta);

            glPushMatrix();
            glRotatef(180*theta/pi,0.0f,0.0f,1.0f);
            draw_img(-4,0,8,256*opacity,line.tex,red,green,blue,selectedOpacity*0.25);
            glPopMatrix();
            glPushMatrix();

            sub_last_mouse_x = mouse_x;
            sub_last_mouse_y = mouse_y;
            float localScaleFactor = scaleFactor;
            glTranslatef(x,y,0);

            mouse_x = (mouse_x-x)/localScaleFactor;
            mouse_y = (mouse_y-y)/localScaleFactor;
            float hoverVelocityTarget;
            if (i==selectedChildIndex) {
                // Set a target for the velocity of what it would be if we just used a straight exponential ease
                hoverVelocityTarget = ((1-(*children.at(i)).hoverScaleFactor)/100000);
            } else {
                hoverVelocityTarget = ((0-(*children.at(i)).hoverScaleFactor)/100000);
            }
            // Exponentially ease velocity towards target
            (*children.at(i)).hoverVelocity = (*children.at(i)).hoverVelocity+((hoverVelocityTarget-(*children.at(i)).hoverVelocity)/100000)*delta_t;
            // Yes, I'm using Euler integration. So sue me. It's fast, and the decaying exponentials cancel out any energy gain.
            (*children.at(i)).hoverScaleFactor = (*children.at(i)).hoverScaleFactor+(*children.at(i)).hoverVelocity*delta_t;
            localScaleFactor = scaleFactor+(*children.at(i)).hoverScaleFactor*scaleFactor*0.5;
            glScalef(localScaleFactor,localScaleFactor,localScaleFactor);
            float child_opacity = 1;
            if (children.size()>24) {
                child_opacity = opacity*(24.0/(float)children.size());
            }
            switch ((*children.at(i)).nodetype) {
                case TYPE_FILE:
                    (*(children.at(i))).draw(child_opacity*Falloff, (level==1 && selectedChildIndex==i) ? true : false);
                    break;
                case TYPE_FOLDER:
                    (*(Folder*)(children.at(i))).draw(child_opacity*Falloff, level+1, (level==1 && selectedChildIndex==i) ? true : false);
                    break;
            }
            mouse_y = sub_last_mouse_y;
            mouse_x = sub_last_mouse_x;
            glPopMatrix();
        }
    }
    mouse_y = last_mouse_y;
    mouse_x = last_mouse_x;

    glPopMatrix();
}
void Folder::click(float opacity) {
    float mousetheta;
    if (mouse_x != 0) {
        mousetheta = fmod(-atan2 ((float)mouse_y, (float)mouse_x) + 2.5*pi, 2*pi);
    } else {
        mousetheta = mouse_y > 0 ? 2*pi : pi;
    }

    int sub_last_mouse_x, sub_last_mouse_y;
    int s = children.size();

    int closestChild;
    if (sqrt(mouse_x*mouse_x + mouse_y*mouse_y) < 256*opacity + 32) {
        closestChild = s*mousetheta/(pi*2)+0.5;
    } else {
        closestChild = -1;
        uint idx = path.rfind('/',path.size()-2);
        string parent;
        if(idx != std::string::npos) {
            parent = path.substr(0,idx+1);
        } else {
            parent = "/";
        }
        root = *(new Folder(parent));
        root.read(1);
    }
    for (int i=0; i<s; i++) {
        if (i == closestChild) {
            switch ((*children.at(i)).nodetype) {
                case TYPE_FILE:
                    printf("Clicked %s\n", (*children.at(i)).path.c_str());
                    break;
                case TYPE_FOLDER:
                    printf("Clicked folder %s\n", (*children.at(i)).path.c_str());
                    root = (*((Folder *)(children.at(i))));
                    root.read(1);
            }
            printf("%i\n", selectedChildIndex);
            break;
        }
        mouse_x = sub_last_mouse_x;
        mouse_y = sub_last_mouse_y;
    }
    // mouse_x = last_mouse_x;
    // mouse_y = last_mouse_y;
}

void Folder::transform(float opacity){
    if (not (openChild==(Folder *)&FILE_NOT_FOUND)) {
        float theta = ((float)selectedChildIndex)/(float)(children.size())*2*pi;
        float x = 256*opacity*sin(theta);
        float y = 256*opacity*cos(theta);
        glScalef(1/scaleFactor,1/scaleFactor,1/scaleFactor);
        glTranslatef(-x,-y,0);
        (*openChild).transform(opacity);
    }
}


void clickRoot() {
    int last_mouse_x = mouse_x;
    int last_mouse_y = mouse_y;
    mouse_x = mouse_x-width/2;
    mouse_y = mouse_y-height/2;
    root.click();
    mouse_x = last_mouse_x;
    mouse_y = last_mouse_y;
}

void pressRoot() {
    uint s = root.children.size();
    switch (key) {
    case 65364: // Down
        if (root.selectedChildIndex==-1) {
            root.selectedChildIndex = (int)(s/2);
        } else if (root.selectedChildIndex<s/2.0 && root.selectedChildIndex >= 0) {
            root.selectedChildIndex += 1;
        } else if (root.selectedChildIndex>=s/2.0) {
            root.selectedChildIndex -= 1;
        }
        break;
    case 65362: // Up
        if (root.selectedChildIndex==-1) {
            root.selectedChildIndex = 0;
        } else if (root.selectedChildIndex<s/2.0 && root.selectedChildIndex > 0) {
            root.selectedChildIndex -= 1;
        } else if (root.selectedChildIndex>=s/2.0 && s > 0) {
            root.selectedChildIndex = (root.selectedChildIndex + 1) % s;
        }
        break;
    case 65361: // Left
        if (root.selectedChildIndex==-1) {
            root.selectedChildIndex = (int)(s*3/4);
        } else if (root.selectedChildIndex<s*3/4.0 && root.selectedChildIndex > s/4.0) {
            root.selectedChildIndex += 1;
        } else if (s > 0) {
            root.selectedChildIndex = (root.selectedChildIndex - 1 + s) % s;
        }
        break;
    case 65363: // Right
        if (root.selectedChildIndex==-1) {
            root.selectedChildIndex = (int)(s/4);
        } else if (root.selectedChildIndex<s*3/4.0 && root.selectedChildIndex > s/4.0) {
            root.selectedChildIndex -= 1;
        } else if (s > 0) {
            root.selectedChildIndex = (root.selectedChildIndex + 1) % s;
        }
        break;
    default:
        break;
    }
}

static void redrawTheWindow()
{
    gettimeofday(&later,NULL);
    delta_t = timeval_diff(NULL,&later,&earlier);
    gettimeofday(&earlier,NULL);

    float const aspect = (float)width / (float)height;

    static float a=0;
    static float b=0;
    static float c=0;

    glDrawBuffer(GL_BACK);

    glViewport(0, 0, width, height);

    // Clear with alpha = 0.0, i.e. full transparency
    glClearColor(0.0, 0.0, 0.0, Opacity);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    // glOrtho(0.0, width, 0.0, height, -1.0, 1.0);
    gluOrtho2D(0, width, 0, height);
    

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    glEnable(GL_TEXTURE_2D);
    glColor3f(1,1,1);
    gprint(our_font, 5, 5, root.path.c_str());
    glTranslatef(width/2,height/2,0);
    int last_mouse_x = mouse_x;
    int last_mouse_y = mouse_y;
    mouse_x = mouse_x-width/2;
    mouse_y = mouse_y-height/2;
    root.transform();
    root.draw();
    mouse_x = last_mouse_x;
    mouse_y = last_mouse_y;

    a = fmod(a+0.1, 360.);
    b = fmod(b+0.5, 360.);
    c = fmod(c+0.25, 360.);

    glXSwapBuffers(Xdisplay, glX_window_handle);
}

int main(int argc, char *argv[])
{
    gettimeofday(&earlier,NULL);
    mouse_x = -1;
    mouse_y = -1;
    createTheWindow();
    createTheRenderContext();
    circle = load_img("ring.png");
    line = load_img("line.png");
    our_font.init("/usr/share/fonts/truetype/freefont/FreeSans.ttf", 16); 
    root.read(1);

    while (updateTheMessageQueue()) {
    redrawTheWindow();
    }

    return 0;
}