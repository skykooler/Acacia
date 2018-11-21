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

#include <cstdlib>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <png.h>
#include <assert.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
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

#include <FTGL/ftgl.h>

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <queue>
#include <map>

#include <wctype.h>

#include <chrono>
#include <ctime>

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

static enum {
    LEFT,
    CENTER,
    RIGHT
} textalign;

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
bool playing = false;
bool paused = false;
FILE *mplayer_pipe;

int playingindex = -1;

float isf; // Interface scale factor

string userinput = string("");
bool typing = false;

FTGLPixmapFont ftgl_font("/usr/share/fonts/truetype/freefont/FreeSans.ttf");
FTGLPixmapFont mono_font("/usr/share/fonts/truetype/freefont/FreeMono.ttf");
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

int minsize;

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

int modifiers = 0;
int SHIFT = 1;
int CONTROL = 2;
int ALT = 4;
int META = 8;

FILE *subprocess;
bool subprocessOpen = false;
std::vector<string> outlines;

bool term_input = false;

std::string exec(char const* cmd);

double logbase(double a, double base)
{
   return log(a) / log(base);
}
 

string format(string text, ...) {
    const char *t = text.c_str();
    char formatted_text[256];

    va_list ap;                                     // Pointer To List Of Arguments
    
    if (t == NULL)                                    // If There's No Text
        *formatted_text=0;                                    // Do Nothing
    else {
        va_start(ap, text);                              // Parses The String For Variables
        vsprintf(formatted_text, t, ap);                            // And Converts Symbols To Actual Numbers
        va_end(ap);                                 // Results Are Stored In formatted_text
    }
    return string(formatted_text);
}

void ftprint(FTPixmapFont *font, float x, float y, int alignment, string text) {

    switch (alignment) {
        case CENTER:
            glRasterPos2f(x-(*font).Advance(text.c_str())/2, y);
            break;
        case RIGHT:
            glRasterPos2f(x-(*font).Advance(text.c_str()), y);
            break;
        case LEFT:
        default:
            glRasterPos2f(x,y);
            break;
    }
    (*font).Render(text.c_str());
}

// trim from start
static inline std::string &ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
        return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
        return ltrim(rtrim(s));
}

struct timeval earlier;
struct timeval later;
time_t correctedtime;

struct timezone tz;

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
void releaseRoot();

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

    // XGrabPointer(Xdisplay,window_handle,True,0,GrabModeAsync,GrabModeAsync,window_handle,0L,CurrentTime);

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
            minsize = min(width, height);
            break;
        case KeyPress:
            key = XLookupKeysym (&event.xkey, 0);
            mousecontrol = false;
            pressRoot();
            break;
        case KeyRelease:
            key = XLookupKeysym (&event.xkey, 0);
            releaseRoot();
            break;
        }
    }
    return 1;
}

void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a=1) {
    glColor4f(r,g,b,a);
    glDisable(GL_TEXTURE_2D);
    const GLshort v[8] = { (GLshort)x, (GLshort)y, (GLshort)(x+w), (GLshort)y,
                           (GLshort)(x+w), (GLshort)(y+h), (GLshort)x, (GLshort)(y+h) };

    glVertexPointer( 2, GL_SHORT, 0, v );
    glEnableClientState( GL_VERTEX_ARRAY );
    
    glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
}

void draw_img(float x, float y, float w, float h, GLuint tex,float r=1,float g=1, float b=1, float a=1) {
        glColor4f(r,g,b,a);
        const GLshort t[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
        const GLshort v[8] = { (GLshort)x, (GLshort)y, (GLshort)(x+w), (GLshort)y,
                               (GLshort)(x+w), (GLshort)(y+h), (GLshort)x, (GLshort)(y+h) };

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
Img play_img;
Img pause_img;
Img line;
Img bar;

queue<string> imgcache;
map<string,Img> imgmap;

class File {
    public:
        File();
        File(string, long);
        void draw(float opacity=1, bool isSelected=false);
        string path;
        string name;
        string file_extension;
        long size;
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
    size = 0;
    toffset = 32767;
    nodetype = TYPE_FILE;
    filetype = BINARY;
    hoverScaleFactor = 0;
    hoverVelocity = 0;
    selectedOpacity = 0;
    selectedVelocity = 0;
}
File::File(string pth, long bytes) {
    path = pth;
    size = bytes;
    nodetype = TYPE_FILE;
    toffset = 32767;

    uint idx = pth.rfind('.');

    if(idx != std::string::npos) {
        std::string extension = pth.substr(idx+1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
        file_extension = extension;
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

bool comparefilenames (File * i, File * j) { return ((*i).name<(*j).name); }

File FILE_NOT_FOUND;

class Folder : public File {
    public:
        // Get initializer from File. Probably.
        Folder(string, long);// : File(string) {};
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
                struct stat filestatus;
                stat( thefile.c_str(), &filestatus );
                out.push_back(new File(thefile, filestatus.st_size));
            } else if (epdf->d_type==DT_DIR) {
                thefile.append("/");
                // printf("%s: %s\n", "Folder",thefile.c_str());
                Folder *f = new Folder(thefile, 0);
                if (levelsleft>0) {
                    (*f).read(levelsleft-1);
                }
                out.push_back(f);
            }
            // printf("Filename: %s\n",epdf->d_name);
       }
    }
    sort (out.begin(), out.end(), comparefilenames);
}

Folder::Folder(string pth, long bytes) {
    toffset = 32767;
    size = bytes;
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

Folder root("/home/skyler/", 0);
void Folder::read(int levelsleft=1) {
    GetFilesInDirectory(children,path,levelsleft);
    int s = children.size();
    for (int i=0; i<s; i++) {
        if ((*children.at(i)).filetype == IMAGE) {
            printf("%s\n", (*children.at(i)).path.c_str());
            if (imgmap.find((*children.at(i)).path) == imgmap.end()) {
                imgcache.push((*children.at(i)).path);
                if ((*children.at(i)).file_extension == "png") {
                    imgmap[(*children.at(i)).path] = load_img((*children.at(i)).path.c_str());
                } else {
                    imgmap[(*children.at(i)).path] = circle;
                }
                    
                if (imgcache.size()>30) {
                    string out = imgcache.front();
                    printf("Popping %s\n", out.c_str());
                    imgcache.pop();
                }
            } else {
                printf("%s already in cache.\n", (*children.at(i)).path.c_str());
            }
        }
    }
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
    // Yes, I'm using Euler integration. So sue me. It's fast, and the decaying exponentials cancel out any energy gain. Usually.
    selectedOpacity = selectedOpacity+selectedVelocity*delta_t;

    if (selectedChildIndex!=-1 && (*children.at(selectedChildIndex)).filetype == AUDIO) {
        if (playing && !paused) {
            draw_img(-128,-128,256,256,pause_img.tex,red,green,blue,selectedOpacity);
        } else {
            draw_img(-128,-128,256,256,play_img.tex,red,green,blue,selectedOpacity);
        }
            
    } else if (selectedChildIndex!=-1 && (*children.at(selectedChildIndex)).filetype == IMAGE) {
        if (imgmap.find((*children.at(selectedChildIndex)).path) == imgmap.end()) {
            draw_img(-128,-128,256,256,circle.tex,red,green,blue,selectedOpacity);

        } else {
            // draw preview image instead
            float ar = (float)(imgmap.at((*children.at(selectedChildIndex)).path).width)/(float)(imgmap.at((*children.at(selectedChildIndex)).path).height);
            float img_width = ar>1 ? 256 : 256*ar;
            float img_height = ar>1 ? 256/ar : 256;

            draw_img(-img_width/2,-img_height/2,img_width,img_height,imgmap.at((*children.at(selectedChildIndex)).path).tex,1,1,1,selectedOpacity);
            
        }
    } else {
        draw_img(-128,-128,256,256,circle.tex,red,green,blue,selectedOpacity);
    }
    
    float mousetheta;
    if (mouse_x != 0) {
        mousetheta = fmod(-atan2 ((float)mouse_y, (float)mouse_x) + 2.5*pi, 2*pi);
    } else {
        mousetheta = mouse_y > 0 ? 0 : pi;
    }

    int sub_last_mouse_x, sub_last_mouse_y;
    int s = children.size();

    int closestChild;
    if (sqrt(mouse_x*mouse_x + mouse_y*mouse_y) < (minsize/3)*opacity + 32 && s > 0 && mousecontrol) {
        closestChild = ((int)(s*mousetheta/(pi*2)+0.5)) % s;
        selectedChildIndex = closestChild;
    } else if (mousecontrol) {
        closestChild = -1;
        selectedChildIndex = closestChild;
    } else {
        closestChild = -1;
    }

    if (level==1) {
        glEnable(GL_TEXTURE_2D);
        glColor3f(1,1,1);
        float line1pos = -128*isf;
        float line2pos = -148*isf;
        if (selectedChildIndex == -1) {
            ftprint(&ftgl_font, 0, line1pos, CENTER, name);
        } else {
            ftprint(&ftgl_font, 0, line1pos, CENTER, (*children.at(selectedChildIndex)).name.c_str());
            if ((*children.at(selectedChildIndex)).nodetype==TYPE_FOLDER) {
                ftprint(&ftgl_font, 0, line2pos, CENTER, format("%i Items", (*(Folder*)(children.at(selectedChildIndex))).children.size()));
            } else {
                long childsize = (*children.at(selectedChildIndex)).size;
                string expstr;
                if (childsize>1023) {
                    if (childsize>1048576) {
                        if (childsize>1073741824) {
                            if (childsize>1099511627776) {
                               ftprint (&ftgl_font, 0, line2pos, CENTER, format("%i TB", childsize/1099511627776));
                            } else {
                               ftprint (&ftgl_font, 0, line2pos, CENTER, format("%i GB", childsize/1073741824));
                            }
                        } else {
                           ftprint (&ftgl_font, 0, line2pos, CENTER, format("%i MB", childsize/1048576));
                        }
                    } else {
                       ftprint (&ftgl_font, 0, line2pos, CENTER, format("%i KB", childsize/1024));
                    }
                } else {
                    ftprint (&ftgl_font, 0, line2pos, CENTER, format("%i Bytes", childsize));
                }
            }
        }
    }

    if (level<4) {
        for (int i=0; i<s; i++) {
            float theta = ((float)i)/((float)(children.size()))*2*pi;
            // float x = 256*opacity*sin(theta);
            // float y = 256*opacity*cos(theta);
            float x = (minsize/3)*opacity*sin(theta);
            float y = (minsize/3)*opacity*cos(theta);

            glPushMatrix();
            glRotatef(180*theta/pi,0.0f,0.0f,1.0f);
            // draw_img(-4,0,8,256*opacity,line.tex,red,green,blue,selectedOpacity*0.25);
            draw_img(-4,0,8,(minsize/3)*opacity,line.tex,red,green,blue,selectedOpacity*0.25);
            glPopMatrix();
            glPushMatrix();

            sub_last_mouse_x = mouse_x;
            sub_last_mouse_y = mouse_y;
            float localScaleFactor = scaleFactor;
            glTranslatef(x,y,0);

            // mouse_x = (mouse_x-x)/localScaleFactor;
            // mouse_y = (mouse_y-y)/localScaleFactor;
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
            localScaleFactor = (scaleFactor+(*children.at(i)).hoverScaleFactor*scaleFactor*0.5) * (logbase ((*children.at(i)).size+1000,1000));
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
    if (sqrt(mouse_x*mouse_x + mouse_y*mouse_y) < (minsize/3)*opacity + 32) {
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
        root = *(new Folder(parent, 0));
        root.read(1);
    }
    for (int i=0; i<s; i++) {
        if (i == selectedChildIndex) {
            switch ((*children.at(i)).nodetype) {
                case TYPE_FILE:
                    printf("Clicked %s\n", (*children.at(i)).path.c_str());
                    FILE * proc;
                    char command[255];
                    int len;
                    // setsid makes process independent of parent
                    len = snprintf(command, sizeof(command), "setsid xdg-open \"%s\"",(*children.at(i)).path.c_str());
                    if (len <= sizeof(command))
                    {
                        proc = popen(command, "r");
                    }
                    else
                    {
                        // command buffer too short
                    }
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

void stop() {
    if (playing) {
        fputs("stop\n", mplayer_pipe);
        fflush(mplayer_pipe);
        paused = !false;
        playing = false;
    }
}

void releaseRoot() {
    uint s = root.children.size();
    string parent;
    uint idx;
    switch (key) {
    case 65505: // Shift
        modifiers = modifiers ^ SHIFT;
        break;
    case 65507: // Control
        modifiers = modifiers ^ CONTROL;
        break;
    case 65515: // Meta
        modifiers = modifiers ^ META;
        break;
    case 65513: // Alt
        modifiers = modifiers ^ ALT;
        break;

    default:
        printf("Keycode %i pressed.\n", (int)key);
    }
}

void pressRoot() {
    uint s = root.children.size();
    string parent;
    uint idx;
    // modifiers = 0;
    switch (key) {
    case 65364: // Down
        stop();
        if (root.selectedChildIndex==-1) {
            root.selectedChildIndex = (int)(s/2);
        } else if (root.selectedChildIndex<s/2.0 && root.selectedChildIndex >= 0) {
            root.selectedChildIndex += 1;
        } else if (root.selectedChildIndex>=s/2.0) {
            root.selectedChildIndex -= 1;
        }
        break;
    case 65362: // Up
        stop();
        if (root.selectedChildIndex==-1) {
            root.selectedChildIndex = 0;
        } else if (root.selectedChildIndex<s/2.0 && root.selectedChildIndex > 0) {
            root.selectedChildIndex -= 1;
        } else if (root.selectedChildIndex>=s/2.0 && s > 0) {
            root.selectedChildIndex = (root.selectedChildIndex + 1) % s;
        }
        break;
    case 65361: // Left
        stop();
        if (root.selectedChildIndex==-1) {
            root.selectedChildIndex = (int)(s*3/4);
        } else if (root.selectedChildIndex<s*3/4.0 && root.selectedChildIndex > s/4.0) {
            root.selectedChildIndex += 1;
        } else if (s > 0) {
            root.selectedChildIndex = (root.selectedChildIndex - 1 + s) % s;
        }
        break;
    case 65363: // Right
        stop();
        if (root.selectedChildIndex==-1) {
            root.selectedChildIndex = (int)(s/4);
        } else if (root.selectedChildIndex<s*3/4.0 && root.selectedChildIndex > s/4.0) {
            root.selectedChildIndex -= 1;
        } else if (s > 0) {
            root.selectedChildIndex = (root.selectedChildIndex + 1) % s;
        }
        break;
    case 65293: // Enter
        stop();
        if (term_input) {
            // TODO: pass off to bash shell instead of exec'ing in-line
            // string result = exec(userinput.c_str());
            // userinput.clear();
            // printf("%s\n", result.c_str());
            subprocessOpen = true;
            subprocess = popen(userinput.c_str(), "r");
            userinput.clear();
            outlines.clear();
        } else if (root.selectedChildIndex!=-1) {
            switch ((*root.children.at(root.selectedChildIndex)).nodetype) {
                case TYPE_FILE:
                    printf("Selected %s\n", (*root.children.at(root.selectedChildIndex)).path.c_str());
                    FILE * proc;
                    char command[255];
                    int len;
                    // setsid makes process independent of parent
                    len = snprintf(command, sizeof(command), "setsid xdg-open \"%s\"",(*root.children.at(root.selectedChildIndex)).path.c_str());
                    if (len <= sizeof(command))
                    {
                        proc = popen(command, "r");
                    }
                    else
                    {
                        // command buffer too short
                    }
                    break;
                case TYPE_FOLDER:
                    printf("Selected folder %s\n", (*root.children.at(root.selectedChildIndex)).path.c_str());
                    root = (*((Folder *)(root.children.at(root.selectedChildIndex))));
                    root.read(1);
            }
        }
        break;
    case 65307: // Escape
        stop();
        term_input = false;
        root.selectedChildIndex = -1;
        idx = root.path.rfind('/',root.path.size()-2);
        if(idx != std::string::npos) {
            parent = root.path.substr(0,idx+1);
        } else {
            parent = "/";
        }
        root = *(new Folder(parent, 0));
        root.read(1);
        break;
    case 32: // Space
        if (userinput.length()>0) {
            userinput.append(" ");
        } else if (root.selectedChildIndex!=-1) {
            if (playing and paused or playingindex==root.selectedChildIndex) {
                printf("Pausing\n");
                fputs("pause\n", mplayer_pipe);
                fflush(mplayer_pipe);
                paused = !paused;
            } else {
                switch ((*root.children.at(root.selectedChildIndex)).nodetype) {
                    case TYPE_FILE:
                        if ((*root.children.at(root.selectedChildIndex)).filetype == AUDIO) {
                            printf("Selected %s\n", (*root.children.at(root.selectedChildIndex)).path.c_str());
                            //FILE * proc;
                            char command[255];
                            int len;
                            // setsid makes process independent of parent
                            if (playing) {
                                len = snprintf(command, sizeof(command), "loadfile \"%s\"\n",(*root.children.at(root.selectedChildIndex)).path.c_str());
                                if (len <= sizeof(command))
                                {
                                    fputs(command, mplayer_pipe);
                                    playing = true; 
                                    paused = false;
                                    playingindex = root.selectedChildIndex;
                                }
                                else
                                {
                                    // command buffer too short
                                }
                            } else {
                                len = snprintf(command, sizeof(command), "mplayer -slave -idle -quiet \"%s\"",(*root.children.at(root.selectedChildIndex)).path.c_str());
                                if (len <= sizeof(command))
                                {
                                    mplayer_pipe = popen(command, "w");
                                    playing = true;
                                    paused = false;
                                    playingindex = root.selectedChildIndex;
                                }
                                else
                                {
                                    // command buffer too short
                                }
                            }
                        } else if (playing) {
                            printf("Pausing\n");
                            fputs("pause\n", mplayer_pipe);
                            fflush(mplayer_pipe);
                            paused = !paused;
                        }
                        break;
                    case TYPE_FOLDER:
                        printf("Selected folder %s\n", (*root.children.at(root.selectedChildIndex)).path.c_str());
                        root = (*((Folder *)(root.children.at(root.selectedChildIndex))));
                        root.read(1);
                        break;
                }
            }
        }
        break;
    case 65505: // Shift
        modifiers = modifiers ^ SHIFT;
        break;
    case 65507: // Control
        modifiers = modifiers ^ CONTROL;
        break;
    case 65515: // Meta
        modifiers = modifiers ^ META;
        break;
    case 65513: // Alt
        modifiers = modifiers ^ ALT;
        break;
    case 65288: // Backspace
        if (userinput.length()>0) {
            userinput.erase(userinput.length()-1,1);
            printf("%s\n", userinput.c_str());
        }
        break;
    default:
        if (!term_input && char(key)=='t') {
            term_input = true;
            userinput.clear();
        } else {
            char cey = modifiers & SHIFT ? (char)(toupper(key)) : (char)key;
            userinput.append( &cey, 1);
            printf("%s %i\n", userinput.c_str(), (int)userinput.length());
        }

        break;
    }
}

void handleSubprocess() {
    char buff[128];
    if (subprocessOpen) {
        if (!feof(subprocess)) {
            if (fgets(buff, 128, subprocess) != NULL) {
                printf("%s", buff);
                string outline = string(buff);
                rtrim(outline);
                outlines.push_back(outline);
                if (outlines.size()>30) {
                    outlines.erase(outlines.begin());
                }
            }
        } else {
            pclose(subprocess);
            subprocessOpen = false;
        }
    }
}

void drawClockSeg(float rot, float offset, float red, float green, float blue, float alpha) {
    glPushMatrix();
    glRotatef(90-rot,0,0,1);
    glTranslatef(minsize/3+offset,0,0);
    draw_img(0,0,20*isf,(minsize/3+offset)*pi/1800+1,bar.tex,red,green,blue,alpha);
    glPopMatrix();
}

static void redrawTheWindow()
{
    gettimeofday(&later,NULL);
    delta_t = min(timeval_diff(NULL,&later,&earlier), (long long int)100000);
    gettimeofday(&earlier,NULL);
    correctedtime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    float const aspect = (float)width / (float)height;

    static float a=0;
    static float b=0;
    static float c=0;

    handleSubprocess();

    ftgl_font.FaceSize(24*isf);
    mono_font.FaceSize(24*isf);

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
    ftprint(&ftgl_font, 5, 5, LEFT, root.path.c_str());
    glTranslatef(width/2,height/2,0);
    int last_mouse_x = mouse_x;
    int last_mouse_y = mouse_y;
    mouse_x = mouse_x-width/2;
    mouse_y = mouse_y-height/2;
    root.transform();
    root.draw();

    // float hour = fmod(((float)(earlier.tv_sec%43200)/3600 - ((float)tz.tz_minuteswest)/60)+12, 12);
    float hour = std::localtime(&correctedtime)->tm_hour%12+fmod((float)(earlier.tv_sec%43200)/3600,1.0f);

    float minute = (fmod(earlier.tv_sec + (float)earlier.tv_usec/1000000.0,3600.0)/60.0);

    float sec = (float)(earlier.tv_sec%60 + (float)earlier.tv_usec/1000000);

    // float sred = 0.5+max(0.0f,min(sec/40,1-sec/40));
    // float sgreen = 0.5+max(0.0f,min((sec-20)/40,1-(sec-20)/40));
    // float sblue = 0.5+max(0.0f,min((float)(((int)sec-40)%60)/40,1-(float)(((int)sec-40)%60)/40));

    float sred = max(0.0f,min(min(sec/20,1.0f),2-sec/20))*0.5+0.5;
    float sgreen = max(0.0,min(min(fmod(sec+40,60)/20,1.0),2-fmod(sec+40,60)/20))*0.5+0.5;
    float sblue = max(0.0,min(min(fmod(sec+20,60)/20,1.0),2-fmod(sec+20,60)/20))*0.5+0.5;

    float mred = max(0.0f,min(min(minute/20,1.0f),2-minute/20))*0.5+0.5;
    float mgreen = max(0.0,min(min(fmod(minute+40,60)/20,1.0),2-fmod(minute+40,60)/20))*0.5+0.5;
    float mblue = max(0.0,min(min(fmod(minute+20,60)/20,1.0),2-fmod(minute+20,60)/20))*0.5+0.5;

    float hred = max(0.0f,min(min(hour/4,1.0f),2-hour/4))*0.5+0.5;
    float hgreen = max(0.0,min(min(fmod(hour+8,12)/4,1.0),2-fmod(hour+8,12)/4))*0.5+0.5;
    float hblue = max(0.0,min(min(fmod(hour+4,12)/4,1.0),2-fmod(hour+4,12)/4))*0.5+0.5;

    if (sec<3) {
        for (int i=0; i<360; i++) {
            glPushMatrix();
            glRotatef(90-i,0,0,1);
            glTranslatef(minsize/3+120,0,0);
            draw_img(0,0,20*isf,(minsize/3+120)*pi/1800+1,bar.tex,sred,sgreen,sblue,1-sec/3.0);
            glPopMatrix();
            
        }
        if (minute<1) {
            for (int i=0; i<360; i++) {
                glPushMatrix();
                glRotatef(90-i,0,0,1);
                glTranslatef(minsize/3+150,0,0);
                draw_img(0,0,20*isf,(minsize/3+150)*pi/1800+1,bar.tex,mred,mgreen,mblue,1-sec/3.0);
                glPopMatrix();
            }
            if (hour<1) {
                for (int i=0; i<360; i++) {
                    glPushMatrix();
                    glRotatef(90-i,0,0,1);
                    glTranslatef(minsize/3+180,0,0);
                    draw_img(0,0,20*isf,(minsize/3+180)*pi/1800+1,bar.tex,hred,hgreen,hblue,1-sec/3.0);
                    glPopMatrix();
                }
            }
        }
    }

    for (int i=0; i<=sec*6-1; i++) {

        drawClockSeg(i,120,sred,sgreen,sblue,1);
        
    }
    drawClockSeg((int)(sec*6),120,sred,sgreen,sblue,fmod(sec*6,1));

    for (int i=0; i<=minute*6-1; i++) {

        drawClockSeg(i,150,mred,mgreen,mblue,1);
        
    }
    drawClockSeg((int)(minute*6),150,mred,mgreen,mblue,fmod(minute*6.0,1));

    for (int i=0; i<hour*30-1; i++) {
        
        drawClockSeg(i,180,hred,hgreen,hblue,1);
    }
    drawClockSeg((int)(hour*30),180,hred,hgreen,hblue,fmod(hour*30.0,1));

    mouse_x = last_mouse_x;
    mouse_y = last_mouse_y;

    a = fmod(a+0.1, 360.);
    b = fmod(b+0.5, 360.);
    c = fmod(c+0.25, 360.);

    /************ Terminal drawing ***********/
    if (term_input) {
        glColor4f(1,1,1,1);
        ftprint(&mono_font, -width/2+100,height/2-100, LEFT, userinput);
        draw_img(-width/2,height/2-120, mono_font.Advance(userinput.c_str())+200,2,line.tex);
    }
    for (std::vector<int>::size_type i=0; i<outlines.size(); i++) {
        ftprint(&mono_font, -width/2+100, height/2-150-(float)(30*i), LEFT, outlines[i]);
    }

    glXSwapBuffers(Xdisplay, glX_window_handle);
    // if (window_handle->isActive()) {
    //     printf("Active\n");
    // }
    usleep(20000);
}

std::string exec(char const* cmd) {
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "ERROR";
    char buffer[128];
    std::string result = "";
    while(!feof(pipe)) {
        if(fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);
    return result;
}

int main(int argc, char *argv[])
{
    correctedtime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    gettimeofday(&earlier,&tz);
    std::string scale_factor_string = exec("dconf read /com/ubuntu/user-interface/scale-factor");
    std::string delimiter = ": ";
    scale_factor_string.erase(0, scale_factor_string.find(delimiter) + delimiter.length());
    scale_factor_string = scale_factor_string.substr(0,scale_factor_string.find(','));
    isf = ((float)atoi(scale_factor_string.c_str()))/8;
    // This broke in Ubuntu 18.04  and apparently there's no alternative.
    //Give up and make up something that isn't zero, at least.
    if (isf<0.5) isf = 1;
    printf("Interface scale factor: %f\n", isf);

    printf("Time: %f : %f\n", std::localtime(&correctedtime)->tm_hour+fmod((float)(earlier.tv_sec%43200)/3600,1.0f), ((float)(earlier.tv_sec%3600)/60));

    mouse_x = -1;
    mouse_y = -1;
    createTheWindow();
    createTheRenderContext();
    circle = load_img("ring.png");
    play_img = load_img("play.png");
    pause_img = load_img("pause.png");
    line = load_img("line.png");
    bar = load_img("bar.png");

    // Die if font loading failed
    if (ftgl_font.Error()) return -1;

    root.read(1);

    while (updateTheMessageQueue()) {
    redrawTheWindow();
    }

    return 0;
}
