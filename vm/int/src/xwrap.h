/* xwrap - v0.22

use example:

    #define XWRAP_IMPLEMENTATION
       // Before you include the file in *one* C or C++ file to create the implementation.
    #define XWRAP_AUTO_LINK
        // Enable runtime linking, eliminating the need for manual linking.
    #include "xwrap.h"

    // Create window
    xw_handle* handle = xw_create_window("example", width, height);

    // Enable image mode
    uint32_t image_buffer[height * width];
    if (!xw_image_connect(handle, image_buffer, width, height))
    {
        return 1;
    }

    // Update the image here then use `xw_draw` to draw.
    xw_draw(handle);

    // Alternatively, you can use graphic mode and the `xw_draw_*` family of functions.

    // Key events:
    // X11 uses a queue of pressed keys. Check if the queue is not empty with `xw_event_pending`,
    // and retrieve events using `xw_get_next_event`.

    // Quality of life
    // wait functions:
    xw_wait_for_esc(handle, 100000)
    xw_sleep_ms(33)
    xw_sleep_us(33 * 1000)

    NOTE:
        - As for now, you can only add 1 image to the window.

  */
#ifndef XWRAP_INCLUDE_H
#define XWRAP_INCLUDE_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef XW_DEF
#ifdef XW_STATIC
#define XW_DEF static
#else
#define XW_DEF extern
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _xw_handle xw_handle;

typedef struct {
    int type;
    unsigned int button;
    int x, y;
    int x_root, y_root;
} xw_mouse_event;

typedef struct {
    int type;
    uint16_t key_code;
} xw_button_event;

typedef struct {
    union {
        int type;
        xw_mouse_event mouse;
        xw_button_event button;
    };
    char original_event[192]; // TODO: make it use 'XEvent' struct
} xw_event;

typedef struct {
    int width, height;
    int x_pos, y_pos; // Of the window in the screen
} xw_dimensions;

/**
 * @brief Creates X11 window
 *
 * @param window_name The name of the window
 * @param width Width of the new window
 * @param height Height of the new window
 * @return xw_handle* The handle for the xwrap
 */
XW_DEF xw_handle* xw_create_window(const char* window_name, int width, int height);
/**
 * @brief Free the window
 *
 * @param handle The handle for the xwrap
 */
XW_DEF void xw_free_window(xw_handle* handle);

/**
 * @brief Return the window name
 * @note The return free with the handle free
 *
 * @param handle The handle for the xwrap
 * @return const char* The name of the window
 */
XW_DEF const char* xw_get_window_name(xw_handle* handle);

/**
 * @brief Connect image to the window by pointer
 *
 * @param handle The handle for the xwrap
 * @param buffer The image to be connected
 * @param width Width of the image
 * @param height Height of the image
 * @return bool true if OK, false if failed
 */
XW_DEF bool xw_image_connect(xw_handle* handle, uint32_t* buffer, uint16_t width, uint16_t height);
/**
 * @brief Finish and draw all the shapes that has been queued
 *
 * @param handle The handle for the xwrap
 * @return bool true if OK, false if failed
 */
XW_DEF bool xw_draw(xw_handle* handle);

/**
 * @brief Clears the window with color
 *
 * @param handle The handle for the xwrap
 * @param color Of the cleared background
 * @return bool true if OK, false if failed
 */
XW_DEF bool xw_draw_background(xw_handle* handle, uint32_t color);
/**
 * @brief Draws text on the screen
 *
 * @param handle The handle for the xwrap
 * @param x The x-coordinate of the top-left corner of the text
 * @param y The y-coordinate of the top-left corner of the text
 * @param string The string to write on the screen
 * @param color The color of the text
 * @return bool true if OK, false if failed
 */
XW_DEF bool xw_draw_text(xw_handle* handle, int x, int y, char* string, uint32_t color);
/**
 * @brief Draws rectangle on the screen - use 'xw_draw' to finish the drawing
 *
 * @param handle The handle for the xwrap
 * @param x The x-coordinate of the top-left corner of the rectangle
 * @param y The y-coordinate of the top-left corner of the rectangle
 * @param width The width of the rectangle
 * @param height The height of the rectangle
 * @param fill Set to true for filled rectangle, false for outline
 * @param color The color of the rectangle
 * @return bool true if OK, false if failed
 */
XW_DEF bool xw_draw_rectangle(xw_handle* handle, int x, int y, unsigned int width,
                              unsigned int height, bool fill, uint32_t color);
/**
 * @brief Draws a line on the screen - use 'xw_draw' to finish the drawing
 *
 * @param handle The handle for the xwrap
 * @param x0 The x-coordinate of the starting point of the line
 * @param y0 The y-coordinate of the starting point of the line
 * @param x1 The x-coordinate of the ending point of the line
 * @param y1 The y-coordinate of the ending point of the line
 * @param width The width of the line
 * @param color The color of the line
 * @return bool true if OK, false if failed
 */
XW_DEF bool xw_draw_line(xw_handle* handle, int x0, int y0, int x1, int y1, uint16_t width,
                         uint32_t color);
/**
 * @brief Draws a circle on the screen - use 'xw_draw' to finish the drawing
 *
 * @param handle The handle for the xwrap
 * @param x The x-coordinate of the center of the circle
 * @param y The y-coordinate of the center of the circle
 * @param r The radius of the circle
 * @param fill Set to true for filled circle, false for outline
 * @param color The color of the circle
 * @return bool true if OK, false if failed
 */
XW_DEF bool xw_draw_circle(xw_handle* handle, int x, int y, int r, bool fill, uint32_t color);
/**
 * @brief Draws a pixel on the screen - use 'xw_draw' to finish the drawing
 *
 * @param handle The handle for the xwrap
 * @param x The x-coordinate of the pixel
 * @param y The y-coordinate of the pixel
 * @param color The color of the pixel
 * @return bool true if OK, false if failed
 */
XW_DEF bool xw_draw_pixel(xw_handle* handle, int x, int y, uint32_t color);
/**
 * @brief Draws a triangle on the screen - use 'xw_draw' to finish the drawing
 *
 * @param handle The handle for the xwrap
 * @param x0 The x-coordinate of the first point of the triangle
 * @param y0 The y-coordinate of the first point of the triangle
 * @param x1 The x-coordinate of the second point of the triangle
 * @param y1 The y-coordinate of the second point of the triangle
 * @param x2 The x-coordinate of the third point of the triangle
 * @param y2 The y-coordinate of the third point of the triangle
 * @param color The color of the triangle
 * @return bool true if OK, false if failed
 */
XW_DEF bool xw_draw_triangle(xw_handle* handle, int x0, int y0, int x1, int y1, int x2, int y2,
                             uint32_t color);

/**
 * @brief Checks if there is events in the event queue
 *
 * @param handle the handle for the xwrap
 * @return int number of event that pending
 */
XW_DEF int xw_event_pending(xw_handle* handle);
/**
 * @brief Give the next event in the queue
 *
 * @param handle the handle for the xwrap
 * @param event The event that returns
 * @return bool true if OK, false if failed
 */
XW_DEF bool xw_get_next_event(xw_handle* handle, xw_event* event);
/**
 * @brief Push the event back to the queue
 *
 * @param handle the handle for xwrap
 * @param event The event to push back
 * @return bool true if OK, false if failed
 */
XW_DEF bool xw_push_back_event(xw_handle* handle, xw_event event);

/**
 * @brief Get the dimensions of the opened screen
 *
 * @param handle the handle for the xwrap
 * @return xw_dimensions struct
 */
XW_DEF xw_dimensions xw_get_dimensions(xw_handle* handle);

/**
 * @brief Sleeps for x time
 *
 * @param nanoseconds how many ns to sleep
 */
XW_DEF void xw_sleep_us(unsigned long nanoseconds);
/**
 * @brief Sleeps for x time
 *
 * @param milliseconds
 */
XW_DEF void xw_sleep_ms(unsigned long milliseconds);
/**
 * @brief Wait for ESC to be click
 *
 * @param handle the handle for the xwrap
 * @param timeout in us, 0 for forever wait
 * @return true if ESC was clicked false if not
 */
XW_DEF bool xw_wait_for_esc(xw_handle* handle, uint64_t timeout);

#endif // XWRAP_INCLUDE_H

#ifdef XWRAP_IMPLEMENTATION

#if !defined(XWRAP_AUTO_LINK)
#include <X11/Xlib.h>
#endif // XWRAP_AUTO_LINK

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef XWRAP_AUTO_LINK
#include <dlfcn.h>

/* Typedefs */
typedef struct _XDisplay Display;
typedef struct _XGC* GC;

typedef unsigned long XID;
typedef XID Window;
typedef XID Drawable;
typedef XID Font;
typedef XID Pixmap;
typedef XID Colormap;

typedef unsigned long Time;
typedef char* XPointer;

typedef struct _XExtData XExtData;         // Shortened
typedef struct _XGCValues XGCValues;       // Shortened
typedef struct _ScreenFormat ScreenFormat; // Shortened
typedef struct _Visual Visual;             // Shortened
typedef struct _Depth Depth;               // Shortened
typedef struct _XImage XImage;             // Shortened

typedef struct {
    XExtData* ext_data;
    struct _XDisplay* display;
    Window root;
    int width, height, mwidth, mheight, ndepths;
    Depth* depths;
    int root_depth;
    Visual* root_visual;
    GC default_gc;
    Colormap cmap;
    unsigned long white_pixel, black_pixel;
    int max_maps, min_maps, backing_store, save_unders;
    long root_input_mask;
} Screen;

typedef struct {
    int x, y, width, height, border_width, depth;
    Visual* visual;
    Window root;
    int class, bit_gravity, win_gravity, backing_store;
    unsigned long backing_planes, backing_pixel;
    int save_under;
    Colormap colormap;
    int map_installed, map_state;
    long all_event_masks, your_event_mask, do_not_propagate_mask;
    int override_redirect;
    Screen* screen;
} XWindowAttributes;

typedef struct {
    XExtData* ext_data;
    struct _XPrivate* private1;
    int fd, private2, proto_major_version, proto_minor_version;
    char* vendor;
    XID private3, private4, private5;
    int private6;
    XID (*resource_alloc)(struct _XDisplay*);
    int byte_order, bitmap_unit, bitmap_pad, bitmap_bit_order, nformats;
    ScreenFormat* pixmap_format;
    int private8, release;
    struct _XPrivate *private9, *private10;
    int qlen;
    unsigned long last_request_read, request;
    XPointer private11, private12, private13, private14;
    unsigned max_request_size;
    struct _XrmHashBucketRec* db;
    int (*private15)(struct _XDisplay*);
    char* display_name;
    int default_screen, nscreens;
    Screen* screens;
    unsigned long motion_buffer, private16;
    int min_keycode, max_keycode;
    XPointer private17, private18;
    int private19;
    char* xdefaults;

}* _XPrivDisplay;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    Display* display;
    Window window, root, subwindow;
    Time time;
    int x, y, x_root, y_root;
    unsigned int state, keycode;
    int same_screen;
} XKeyEvent;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    Display* display;
    Window window, root, subwindow;
    Time time;
    int x, y, x_root, y_root;
    unsigned int state, button;
    int same_screen;
} XButtonEvent;

typedef union _XEvent {
    int type;
    XKeyEvent xkey;
    XButtonEvent xbutton;
    long pad[24];
} XEvent;

typedef struct {
    short x, y;
} XPoint;

/* Definitions */
#define ScreenOfDisplay(dpy, scr) (&((_XPrivDisplay)(dpy))->screens[scr])
#define RootWindow(dpy, scr) (ScreenOfDisplay(dpy, scr)->root)
#define DefaultScreen(dpy) (((_XPrivDisplay)(dpy))->default_screen)
#define DefaultVisual(dpy, scr) (ScreenOfDisplay(dpy, scr)->root_visual)
#define WhitePixel(dpy, scr) (ScreenOfDisplay(dpy, scr)->white_pixel)

#define NoEventMask 0L
#define KeyPressMask (1L << 0)
#define KeyReleaseMask (1L << 1)
#define ButtonPressMask (1L << 2)
#define ButtonReleaseMask (1L << 3)
#define PointerMotionMask (1L << 6)

#define ZPixmap 2
#define LineSolid 0
#define CapButt 1
#define JoinMiter 0
#define Nonconvex 1
#define CoordModeOrigin 0

/* Clicks */
#define Button1 1
#define Button2 2
#define Button3 3
#define Button4 4
#define Button5 5

#define KeyPress 2
#define KeyRelease 3
#define ButtonPress 4
#define ButtonRelease 5
#define MotionNotify 6

/* Function declarations */
Display* (*XOpenDisplay)(const char*)                                                   = NULL;
Window (*XCreateSimpleWindow)(Display*, Window, int, int, unsigned int, unsigned int, unsigned int,
                              unsigned long, unsigned long)                             = NULL;
int (*XMapWindow)(Display*, Window)                                                     = NULL;
int (*XSelectInput)(Display*, Window, long)                                             = NULL;
GC (*XCreateGC)(Display*, Drawable, unsigned long, XGCValues*)                          = NULL;
int (*XSetForeground)(Display*, GC, unsigned long)                                      = NULL;
int (*XFillRectangle)(Display*, Drawable, GC, int, int, unsigned int, unsigned int)     = NULL;
int (*XFlush)(Display*)                                                                 = NULL;
int (*XFreeGC)(Display*, GC)                                                            = NULL;
int (*XDestroyWindow)(Display*, Window)                                                 = NULL;
int (*XCloseDisplay)(Display*)                                                          = NULL;
XImage* (*XCreateImage)(Display*, Visual*, unsigned int, int, int, char*, unsigned int,
                        unsigned int, int, int)                                         = NULL;
int (*XPutImage)(Display*, Drawable, GC, XImage*, int, int, int, int, unsigned int,
                 unsigned int)                                                          = NULL;
int (*XDrawRectangle)(Display*, Drawable, GC, int, int, unsigned int, unsigned int)     = NULL;
int (*XSetLineAttributes)(Display*, GC, unsigned int, int, int, int)                    = NULL;
int (*XDrawLine)(Display*, Drawable, GC, int, int, int, int)                            = NULL;
int (*XFillArc)(Display*, Drawable, GC, int, int, unsigned int, unsigned int, int, int) = NULL;
int (*XDrawArc)(Display*, Drawable, GC, int, int, unsigned int, unsigned int, int, int) = NULL;
int (*XDrawPoint)(Display*, Drawable, GC, int, int)                                     = NULL;
int (*XFillPolygon)(Display*, Drawable, GC, XPoint*, int, int, int)                     = NULL;
int (*XPending)(Display*)                                                               = NULL;
int (*XNextEvent)(Display*, XEvent*)                                                    = NULL;
int (*XPutBackEvent)(Display*, XEvent*)                                                 = NULL;
int (*XGetWindowAttributes)(Display*, Window, XWindowAttributes*)                       = NULL;
int (*XClearWindow)(Display*, Window)                                                   = NULL;
int (*XSetWindowBackground)(Display*, Window, unsigned long)                            = NULL;
int (*XDrawString)(Display*, Drawable, GC, int, int, char*, int)                        = NULL;
int (*XStoreName)(Display*, Window, const char*)                                        = NULL;

/* Linker */
void* dl_handle         = NULL;
const char* name_libx11 = "libX11.so";
const struct {
    const char* name;
    void** fun;
} dl_fun[] = {
    {"XOpenDisplay", (void**)&XOpenDisplay},
    {"XCreateSimpleWindow", (void**)&XCreateSimpleWindow},
    {"XMapWindow", (void**)&XMapWindow},
    {"XSelectInput", (void**)&XSelectInput},
    {"XCreateGC", (void**)&XCreateGC},
    {"XSetForeground", (void**)&XSetForeground},
    {"XFillRectangle", (void**)&XFillRectangle},
    {"XFlush", (void**)&XFlush},
    {"XFreeGC", (void**)&XFreeGC},
    {"XDestroyWindow", (void**)&XDestroyWindow},
    {"XCloseDisplay", (void**)&XCloseDisplay},
    {"XCreateImage", (void**)&XCreateImage},
    {"XPutImage", (void**)&XPutImage},
    {"XDrawRectangle", (void**)&XDrawRectangle},
    {"XSetLineAttributes", (void**)&XSetLineAttributes},
    {"XDrawLine", (void**)&XDrawLine},
    {"XFillArc", (void**)&XFillArc},
    {"XDrawArc", (void**)&XDrawArc},
    {"XDrawPoint", (void**)&XDrawPoint},
    {"XFillPolygon", (void**)&XFillPolygon},
    {"XPending", (void**)&XPending},
    {"XNextEvent", (void**)&XNextEvent},
    {"XPutBackEvent", (void**)&XPutBackEvent},
    {"XGetWindowAttributes", (void**)&XGetWindowAttributes},
    {"XClearWindow", (void**)&XClearWindow},
    {"XSetWindowBackground", (void**)&XSetWindowBackground},
    {"XDrawString", (void**)&XDrawString},
    {"XStoreName", (void**)&XStoreName},
};

const size_t dl_fun_len = sizeof(dl_fun) / sizeof(*dl_fun);

void _xw_d_unlink(void* handle)
{
    dlclose(handle);
}

bool _xw_d_link(void** handle)
{
    if (*handle != NULL) {
        fprintf(stderr, "WARNING: double link\n");
        return true;
    }

    *handle = dlopen(name_libx11, RTLD_LAZY | RTLD_GLOBAL);
    for (size_t i = 0; i < dl_fun_len; i++) {
        *dl_fun[i].fun = dlsym(*handle, dl_fun[i].name);
        if (*dl_fun[i].fun == NULL) {
            return false;
        }
    }
    return true;
}
#endif // XWRAP_AUTO_LINK

struct _xw_handle {
    Display* display;
    Window window;
    char* window_name;
    GC gc;
    XImage* image;
    uint16_t width;
    uint16_t height;
};

static size_t windows_open = 0; /* Count how many windows open */
XW_DEF xw_handle* xw_create_window(const char* window_name, int width, int height)
{
#ifdef XWRAP_AUTO_LINK
    if (windows_open == 0 && !_xw_d_link(&dl_handle)) {
        fprintf(stderr, "ERROR: could not link with x11: %s\n", dlerror());
        exit(1);
    }
    windows_open++;
#endif // XWRAP_AUTO_LINK

    xw_handle* handle = (xw_handle*)malloc(sizeof(xw_handle));
    handle->display   = XOpenDisplay(NULL);
    if (handle->display == NULL) {
        fprintf(stderr, "ERROR: Unable to connect X server\n");
        free(handle);
        return NULL;
    }
    handle->window = XCreateSimpleWindow(
        handle->display, RootWindow(handle->display, DefaultScreen(handle->display)), 0, 0, width,
        height, 0, 0x000000, WhitePixel(handle->display, 0));

    // Set the window name
    XStoreName(handle->display, handle->window, window_name);
    handle->window_name = malloc(strlen(window_name) * sizeof(char) + 1);
    if (handle->window_name == NULL) {
        fprintf(stderr, "ERROR: Buy more ram\n");
        free(handle);
        return NULL;
    }
    strcpy(handle->window_name, window_name);

    XMapWindow(handle->display, handle->window);
    XSelectInput(handle->display, handle->window,
                 KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask);

    handle->gc    = XCreateGC(handle->display, handle->window, 0, NULL);
    handle->image = NULL;

    // Busy wait for the screen to open - fixes premature drawing
    XWindowAttributes window_attributes_return = {0};
    while (window_attributes_return.map_state == 0) {
        xw_sleep_us(10);
        XGetWindowAttributes(handle->display, handle->window, &window_attributes_return);
    }
    return handle;
}

XW_DEF void xw_free_window(xw_handle* handle)
{
    XFreeGC(handle->display, handle->gc);
    XDestroyWindow(handle->display, handle->window);
    XCloseDisplay(handle->display);
    free(handle->window_name);
    free(handle);

#ifdef XWRAP_AUTO_LINK
    windows_open--;
    if (windows_open < 1) {
        _xw_d_unlink(dl_handle);
        dl_handle = NULL;
    }
#endif // XWRAP_AUTO_LINK
}

XW_DEF const char* xw_get_window_name(xw_handle* handle)
{
    // TODO: get this information from _XPrivDisplay struct
    return handle->window_name;
}

XW_DEF bool xw_image_connect(xw_handle* handle, uint32_t* buffer, uint16_t width, uint16_t height)
{
    if (handle->image != NULL) {
        fprintf(stderr, "ERROR: cannot reconnect image\n"); // TODO: reconnect image
        return false;
    }
    handle->image = XCreateImage(handle->display,
                                 DefaultVisual(handle->display, DefaultScreen(handle->display)), 24,
                                 ZPixmap, 0, (char*)buffer, width, height, 32, 0);

    if (handle->image == NULL) {
        fprintf(stderr, "ERROR: could not connect image\n");
        return false;
    }

    handle->width  = width;
    handle->height = height;
    return true;
}

XW_DEF bool xw_draw(xw_handle* handle)
{
    if (handle->image != NULL) {
        XPutImage(handle->display, handle->window, handle->gc, handle->image, 0, 0, 0, 0,
                  handle->width, handle->height);
    }
    return XFlush(handle->display);
}

XW_DEF bool xw_draw_background(xw_handle* handle, uint32_t color)
{
    XSetWindowBackground(handle->display, handle->window, color);
    return XClearWindow(handle->display, handle->window);
}

XW_DEF bool xw_draw_text(xw_handle* handle, int x, int y, char* string, uint32_t color)
{
    XSetForeground(handle->display, handle->gc, color);

    int length = strlen(string);
    return XDrawString(handle->display, handle->window, handle->gc, x, y, string, length);
}

XW_DEF bool xw_draw_rectangle(xw_handle* handle, int x, int y, unsigned int width,
                              unsigned int height, bool fill, uint32_t color)
{
    XSetForeground(handle->display, handle->gc, color);
    if (fill) {
        return XFillRectangle(handle->display, handle->window, handle->gc, x, y, width, height);
    }
    return XDrawRectangle(handle->display, handle->window, handle->gc, x, y, width, height);
}

XW_DEF bool xw_draw_line(xw_handle* handle, int x0, int y0, int x1, int y1, uint16_t width,
                         uint32_t color)
{
    XSetLineAttributes(handle->display, handle->gc, width, LineSolid, CapButt, JoinMiter);
    XSetForeground(handle->display, handle->gc, color);

    return XDrawLine(handle->display, handle->window, handle->gc, x0, y0, x1, y1);
}

XW_DEF bool xw_draw_circle(xw_handle* handle, int x, int y, int r, bool fill, uint32_t color)
{
    XSetForeground(handle->display, handle->gc, color);
    if (fill) {
        return XFillArc(handle->display, handle->window, handle->gc, x - r, y - r, 2 * r, 2 * r, 0,
                        360 * 64);
    }
    return XDrawArc(handle->display, handle->window, handle->gc, x - r, y - r, 2 * r, 2 * r, 0,
                    360 * 64);
}

XW_DEF bool xw_draw_pixel(xw_handle* handle, int x, int y, uint32_t color)
{
    XSetForeground(handle->display, handle->gc, color);
    return XDrawPoint(handle->display, handle->window, handle->gc, x, y);
}

XW_DEF bool xw_draw_triangle(xw_handle* handle, int x0, int y0, int x1, int y1, int x2, int y2,
                             uint32_t color)
{
    XSetForeground(handle->display, handle->gc, color);
    XPoint points[3] = {
        [0] = {.x = x0, .y = y0},
        [1] = {.x = x1, .y = y1},
        [2] = {.x = x2, .y = y2},
    };
    int npoints = 3;
    int shape   = Nonconvex;
    int mode    = CoordModeOrigin;

    return XFillPolygon(handle->display, handle->window, handle->gc, points, npoints, shape, mode);
}

XW_DEF int xw_event_pending(xw_handle* handle)
{
    return XPending(handle->display);
}

XW_DEF bool xw_get_next_event(xw_handle* handle, xw_event* event)
{
    XEvent* Xevent = (XEvent*)event->original_event;
    bool ret       = XNextEvent(handle->display, Xevent);
    event->type    = Xevent->type;

    switch (event->type) {
        case MotionNotify:
        case ButtonRelease:
        case ButtonPress: {
            event->mouse.button = Xevent->xbutton.button;
            event->mouse.x      = Xevent->xbutton.x;
            event->mouse.y      = Xevent->xbutton.y;
            event->mouse.y_root = Xevent->xbutton.y_root;
            event->mouse.x_root = Xevent->xbutton.x_root;
        } break;

        case KeyPress:
        case KeyRelease: {
            event->button.key_code = Xevent->xkey.keycode;
        } break;

        default:
            fprintf(stderr, __FILE__ ":%d WARNING: unreachable code\n", __LINE__);
            break;
    }

    return ret;
}

XW_DEF bool xw_push_back_event(xw_handle* handle, xw_event event)
{
    return XPutBackEvent(handle->display, (XEvent*)event.original_event);
}

XW_DEF xw_dimensions xw_get_dimensions(xw_handle* handle)
{
    XWindowAttributes window_attributes_return = {0};
    XGetWindowAttributes(handle->display, handle->window, &window_attributes_return);

    xw_dimensions ret = {.width  = window_attributes_return.width,
                         .height = window_attributes_return.height,
                         .x_pos  = window_attributes_return.x,
                         .y_pos  = window_attributes_return.y};
    return ret;
}

XW_DEF void xw_sleep_us(unsigned long nanoseconds)
{
    struct timespec ts;
    ts.tv_sec  = nanoseconds / 1000000ul;
    ts.tv_nsec = (nanoseconds % 1000000ul) * 1000;
    nanosleep(&ts, NULL);
}

XW_DEF void xw_sleep_ms(unsigned long milliseconds)
{
    xw_sleep_us(1000 * milliseconds);
}

XW_DEF bool xw_wait_for_esc(xw_handle* handle, uint64_t timeout)
{
    uint64_t total = 0;
    uint64_t wait  = 50;
    if (timeout > wait && timeout != 0) {
        wait = timeout;
    }

    for (;;) {
        bool found_event        = false;
        const int event_pending = xw_event_pending(handle);
        xw_event events[event_pending];
        size_t pos = 0;
        for (size_t i = 0; i < event_pending; i++) {
            xw_get_next_event(handle, &events[pos]);
            if (events[pos].type == KeyPress && events[pos].button.key_code == 9) {
                found_event = true;
                break;
            } else {
                pos++;
            }
        }
        // put back on the event stack - (in the doc they said queue but...)
        for (int i = pos - 1; i >= 0; i--) {
            xw_push_back_event(handle, events[i]);
        }
        if (found_event) {
            return true;
        }

        xw_sleep_us(wait);
        total += wait;

        if (total > timeout && timeout != 0) {
            return false;
        }
    }
}
#endif // XWRAP_IMPLEMENTATION

#ifdef __cplusplus
}
#endif