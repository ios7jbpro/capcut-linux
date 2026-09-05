#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdio.h>
int main(void){
 Display*d=XOpenDisplay(NULL);assert(d);void*h=dlopen("libXrender.so.1",RTLD_NOW|RTLD_LOCAL);assert(h);
 __typeof__(&XRenderCreatePicture) create=dlsym(h,"XRenderCreatePicture");
 __typeof__(&XRenderFindStandardFormat) fmt=dlsym(h,"XRenderFindStandardFormat");
 __typeof__(&XRenderComposite) composite=dlsym(h,"XRenderComposite");
 __typeof__(&XRenderFreePicture) release=dlsym(h,"XRenderFreePicture");assert(create&&fmt&&composite&&release);
 for(unsigned dep=24;dep<=32;dep+=8){
  Pixmap src=XCreatePixmap(d,DefaultRootWindow(d),8,8,dep),dst=XCreatePixmap(d,DefaultRootWindow(d),8,8,dep);
  GC gc=XCreateGC(d,src,0,NULL);XSetForeground(d,gc,0xff123456);XFillRectangle(d,src,gc,0,0,8,8);
  XRenderPictFormat*wrong=fmt(d,dep==32?PictStandardRGB24:PictStandardARGB32);
  XRenderPictFormat*right=fmt(d,dep==32?PictStandardARGB32:PictStandardRGB24);
  Picture sp=create(d,src,wrong,0,NULL);XSync(d,False);
  Picture dp=create(d,dst,right,0,NULL);composite(d,PictOpSrc,sp,None,dp,0,0,0,0,0,0,8,8);XSync(d,False);
  XImage*i=XGetImage(d,dst,0,0,8,8,AllPlanes,ZPixmap);assert((XGetPixel(i,2,2)&0xffffff)==0x123456);XDestroyImage(i);
  assert(wrong->depth==(dep==32?24:32));release(d,sp);release(d,dp);XFreeGC(d,gc);XFreePixmap(d,src);XFreePixmap(d,dst);
 }
 XCloseDisplay(d);puts("PASS: dynamically resolved XRender format corrected both ways; pixels preserved; shared formats unchanged");
}
