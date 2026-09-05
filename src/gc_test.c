#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <stdio.h>
static unsigned long pixel(Display*d,Pixmap p,int x,int y){XImage*i=XGetImage(d,p,x,y,1,1,AllPlanes,ZPixmap);unsigned long v=XGetPixel(i,0,0);XDestroyImage(i);return v;}
int main(void){Display*d=XOpenDisplay(NULL);assert(d);Window root=DefaultRootWindow(d);
 Pixmap p24=XCreatePixmap(d,root,8,8,24),p32=XCreatePixmap(d,root,8,8,32),src=XCreatePixmap(d,root,8,8,32);
 GC g24=XCreateGC(d,p24,0,NULL),g32=XCreateGC(d,p32,0,NULL),sg=XCreateGC(d,src,0,NULL);
 XSetForeground(d,g32,0xffffffff);XFillRectangle(d,p32,g32,0,0,8,8);XSetForeground(d,sg,0xff123456);XFillRectangle(d,src,sg,0,0,8,8);
 XRectangle rect={1,1,2,2};XSetClipRectangles(d,g24,1,1,&rect,1,Unsorted);XSetFunction(d,g24,GXxor);
 XCopyArea(d,src,p32,g24,0,0,8,8,0,0);XSync(d,False);
 assert((pixel(d,p32,2,2)&0xffffff)==(0xffffff^0x123456));assert(pixel(d,p32,1,1)==0xffffffff);
 XSetClipMask(d,g24,None);XSetFunction(d,g24,GXcopy);XSetPlaneMask(d,g24,0xff);
 XCopyArea(d,src,p32,g24,0,0,8,8,0,0);XSync(d,False);assert(pixel(d,p32,1,1)==0xffffff56);
 XSetPlaneMask(d,g24,AllPlanes);XSetForeground(d,g24,0xabcdef);XFillRectangle(d,p24,g24,0,0,8,8);
 Region empty=XCreateRegion();XSetRegion(d,g32,empty);XCopyArea(d,p24,p24,g32,0,0,8,8,0,0);XSync(d,False);assert(pixel(d,p24,0,0)==0xabcdef);
 XSetClipMask(d,g32,None);XSetFunction(d,g32,GXclear);XCopyArea(d,p24,p24,g32,0,0,8,8,0,0);XSync(d,False);assert(pixel(d,p24,0,0)==0);
 XDestroyRegion(empty);XFreeGC(d,g24);XFreeGC(d,g32);XFreeGC(d,sg);XFreePixmap(d,p24);XFreePixmap(d,p32);XFreePixmap(d,src);XCloseDisplay(d);
 puts("PASS: mismatched GC both directions, same-depth source, clip origin, XOR, plane mask, empty region, clip reset");}
