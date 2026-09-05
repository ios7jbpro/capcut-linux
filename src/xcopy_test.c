#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <assert.h>
int main(void){Display*d=XOpenDisplay(NULL);assert(d);Window root=DefaultRootWindow(d);Pixmap s=XCreatePixmap(d,root,8,8,32),t=XCreatePixmap(d,root,8,8,24);GC sg=XCreateGC(d,s,0,NULL),tg=XCreateGC(d,t,0,NULL);XSetForeground(d,sg,0xff123456);XFillRectangle(d,s,sg,0,0,8,8);XSetForeground(d,tg,0xffffff);XFillRectangle(d,t,tg,0,0,8,8);
XRectangle clip={2,2,2,2};XSetClipRectangles(d,tg,0,0,&clip,1,Unsorted);XCopyArea(d,s,t,tg,0,0,8,8,0,0);XSync(d,False);XImage*i=XGetImage(d,t,0,0,8,8,AllPlanes,ZPixmap);assert((XGetPixel(i,2,2)&0xffffff)==0x123456);assert((XGetPixel(i,0,0)&0xffffff)==0xffffff);XDestroyImage(i);
XSetFunction(d,tg,GXxor);XCopyArea(d,s,t,tg,0,0,8,8,0,0);XSync(d,False);i=XGetImage(d,t,0,0,8,8,AllPlanes,ZPixmap);assert((XGetPixel(i,2,2)&0xffffff)==0);XDestroyImage(i);
XSetClipMask(d,tg,None);XSetFunction(d,tg,GXcopy);XSetForeground(d,tg,0xabcdef);XFillRectangle(d,t,tg,0,0,8,8);XCopyArea(d,t,s,sg,0,0,8,8,0,0);XSync(d,False);i=XGetImage(d,s,0,0,8,8,AllPlanes,ZPixmap);assert(XGetPixel(i,0,0)==0xffabcdef);XDestroyImage(i);XFreeGC(d,sg);XFreeGC(d,tg);XFreePixmap(d,s);XFreePixmap(d,t);XCloseDisplay(d);puts("PASS: 32->24, clip preserved, XOR preserved, 24->32 alpha opaque");}
