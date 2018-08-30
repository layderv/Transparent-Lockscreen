#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <cctype>
#include <string>
#include <iostream>

const std::string &get_password() {
    static std::string p = "aaa";
    return p;
}

static const int VisData[] = {
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

static void die(const char *msg) {
    std::cerr << msg << std::endl;
    exit(1);
}

static Bool MapNotify_event(Display *d, XEvent *e, char *arg) { // NOLINT(readability-non-const-parameter)
    return d && e && arg && (e->type == MapNotify) && (e->xmap.window == *reinterpret_cast<const Window *>(arg));
}

static bool fullscreen(Display *&display, const Window &window) {
    Atom fullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
    Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);

    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = window;
    xev.xclient.message_type = wm_state;
    xev.xclient.format = 32;
    // todo multihead
    xev.xclient.data.l[0] = 1;
    xev.xclient.data.l[1] = fullscreen;
    xev.xclient.data.l[2] = 0;
    XMapWindow(display, window);

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xev);

    XFlush(display);

    return true;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"
static void create_window(Display *&Xdisplay, Window &window, Atom& del_atom, GLXFBConfig &fbconfig) {
    XVisualInfo *visual = nullptr;
    Window root;
    XEvent event;
    int x, y, attr_mask;
    XSizeHints hints;
    XWMHints *startup_state;
    XTextProperty textprop;
    XSetWindowAttributes attr = { 0, };
    int Xscreen;
    const char *title = "";

    Xdisplay = XOpenDisplay(nullptr);
    if (!Xdisplay) {
        die("XOpenDisplay");
    }

    Xscreen = DefaultScreen(Xdisplay);
    root = RootWindow(Xdisplay, Xscreen);

    int n;
    const auto fbconfigs = glXChooseFBConfig(Xdisplay, Xscreen, VisData, &n);
    fbconfig = nullptr;
    for (int i = 0; i < n; i++) {
        visual = glXGetVisualFromFBConfig(Xdisplay, fbconfigs[i]);
        if (!visual) {
            continue;
        }

        fbconfig = fbconfigs[i];
    }

    if (!fbconfig || !visual) {
        die("fbconfig, visual");
    }

    attr.colormap = XCreateColormap(Xdisplay, root, visual->visual, AllocNone);
    attr.background_pixmap = None;
    attr.border_pixmap = None;
    attr.border_pixel = 0;
    attr.event_mask =
            StructureNotifyMask |
            EnterWindowMask |
            LeaveWindowMask |
            ExposureMask |
            KeyPressMask |
            KeyReleaseMask;

    attr_mask =
            CWBackPixmap |
            CWColormap |
            CWBorderPixel |
            CWEventMask;

    attr.override_redirect = true;

    XWindowAttributes win_attrs;
    XGetWindowAttributes(Xdisplay, root, &win_attrs);

    x = y = 0;

    window = XCreateWindow(Xdisplay,
                           root,
                           x, y,
                           static_cast<unsigned int>(win_attrs.width), static_cast<unsigned int>(win_attrs.height),
                           0,
                           visual->depth,
                           InputOutput,
                           visual->visual,
                           static_cast<unsigned long>(attr_mask), &attr);

    if (!window) {
        die("XCreateWindow");
    }

    textprop.value = (unsigned char *) title;
    textprop.encoding = XA_STRING;
    textprop.format = 8;
    textprop.nitems = 0;

    hints.x = x;
    hints.y = y;
    hints.width = win_attrs.width;
    hints.height = win_attrs.height;
    hints.flags = USPosition | USSize;

    startup_state = XAllocWMHints();
    startup_state->initial_state = NormalState;
    startup_state->flags = StateHint;

    XSetWMProperties(Xdisplay, window, &textprop, &textprop,
                     nullptr, 0,
                     &hints,
                     startup_state,
                     nullptr);


    XFree(startup_state);

    XMapWindow(Xdisplay, window);
    XIfEvent(Xdisplay, &event, MapNotify_event, (char *) &window);

    if ((del_atom = XInternAtom(Xdisplay, "WM_DELETE_WINDOW", 0)) != None) {
        XSetWMProtocols(Xdisplay, window, &del_atom, 1);
    }
    fullscreen(Xdisplay, window);
}
#pragma clang diagnostic pop

static void press_key(Display *&display, char c) {
    char buf[2] = {c, '\0'};

    XTestFakeKeyEvent(display,
            XKeysymToKeycode(display, XStringToKeysym(buf)),
            False,
            0);
    XFlush(display);
}

static void create_render_context(Display *&Xdisplay, GLXWindow &glX_window, GLXFBConfig &fbconfig) {
    int _;
    GLXContext render_context;

    if (!glXQueryExtension(Xdisplay, &_, &_)) {
        die("glXQueryExtension");
    }

    render_context = glXCreateNewContext(Xdisplay, fbconfig, GLX_RGBA_TYPE, nullptr, True);
    if (!render_context) {
        die("glXCreateNewContext");
    }

    fullscreen(Xdisplay, glX_window);
    if (!glXMakeContextCurrent(Xdisplay, glX_window, glX_window, render_context)) {
        die("glXMakeContextCurrent");
    }
}

static int message_queue(Display *&Xdisplay, Window &window, Atom& del_atom) {
    static std::string user_input{};
    static std::string pw = get_password();

    XEvent event;
    int len;
    char buf[16 + 1];
    KeySym ks;
    XComposeStatus comp;
    int result = 1;

    XGrabKey(Xdisplay, AnyKey, AnyModifier, window, true, GrabModeAsync, GrabModeAsync);
    while (XPending(Xdisplay) && result) {
        XNextEvent(Xdisplay, &event);
        switch (event.type) {
            case ClientMessage:
                if (event.xclient.data.l[0] == del_atom) {
                    result = 0;
                }
                break;

            case ConfigureNotify:
                // don't resize
                // xc = &(event.xconfigure);
                // width = xc->width;
                // height = xc->height;
                break;

            case KeyPress:
                len = XLookupString(&event.xkey, buf, sizeof(buf) - 1, &ks, &comp);
                if (len > 0 && isprint(*buf)) {
                    buf[len] = 0;
                    user_input += buf;
                    if (user_input.find(pw) != std::string::npos) {
                        std::cout << "Password entered successfully. Unlocking..." << std::endl;
                        result = 0;
                    }
                    if (user_input.size() > pw.size()) {
                        user_input = user_input.substr(user_input.size() - pw.size());
                    }
                    std::cout << "Input: " << buf << "current_user_input is: " << user_input << std::endl;
                } else {
                    auto modifiers_on = event.xkey.state & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);
                    // Mod1/2/3/4/5 = Alt/NumLock/ScrollLock/Windows/?
                    if (modifiers_on & Mod4Mask) {
                        std::cout << "Pressed Windows Key" << std::endl;
                        press_key(Xdisplay, 'a');
                    }
                    if (modifiers_on & Mod1Mask) {
                        std::cout << "Pressed ALT" << std::endl;
                        // forbid ALT F4
                        press_key(Xdisplay, 'a');
                    }
                    std::cout << "Key is: " << ks << std::endl;
                }
                break;

            default:
                break;
        }
    }
    return result;
}

static void redraw(Display *&Xdisplay, GLXWindow &glX_window) {
    glXSwapBuffers(Xdisplay, glX_window);
}

int main(int argc, char *argv[]) {
    Atom del_atom;
    Display *Xdisplay;
    GLXFBConfig fbconfig;
    Window window;

    create_window(Xdisplay, window, del_atom, fbconfig);
    create_render_context(Xdisplay, window, fbconfig);

    while (message_queue(Xdisplay, window, del_atom)) {
        redraw(Xdisplay, window);
    }

    return 0;
}