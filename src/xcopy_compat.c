#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
/* Track client-created drawable depths: no synchronous readback on frames,
   and no extra X requests against windows that may have just been destroyed. */
#define CAP 32768
static struct {Drawable id;unsigned depth;} cache[CAP];
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static _Atomic int enabled=-1;
static unsigned conversions;
static _Atomic unsigned unknowns;
static int active(void){if(enabled<0){const char*s=getenv("CAPCUT_XCOPY_COMPAT");enabled=s&&!strcmp(s,"1");}return enabled;}
static unsigned slot(Drawable id){return (unsigned)((id*2654435761u)^(id>>16))&(CAP-1);}
static void remember(Drawable id,unsigned depth){if(!id||!active())return;pthread_mutex_lock(&lock);unsigned k=slot(id);for(unsigned i=0;i<32;i++){unsigned j=(k+i)&(CAP-1);if(!cache[j].id||cache[j].id==id){k=j;break;}}cache[k].id=id;cache[k].depth=depth;pthread_mutex_unlock(&lock);}
static unsigned depth(Drawable id){unsigned d=0;pthread_mutex_lock(&lock);unsigned k=slot(id);for(unsigned i=0;i<32;i++){unsigned j=(k+i)&(CAP-1);if(cache[j].id==id){d=cache[j].depth;break;}}pthread_mutex_unlock(&lock);return d;}
#define REAL(name) static _Atomic(__typeof__(&name)) cached_real; __typeof__(&name) real=atomic_load(&cached_real); if(!real){real=(__typeof__(&name))dlsym(RTLD_NEXT,#name);atomic_store(&cached_real,real);}
Display*XOpenDisplay(const char*name){REAL(XOpenDisplay);Display*d=real(name);if(d&&active())for(int i=0;i<ScreenCount(d);i++)remember(RootWindow(d,i),DefaultDepth(d,i));return d;}
Pixmap XCreatePixmap(Display*d,Drawable parent,unsigned w,unsigned h,unsigned dep){REAL(XCreatePixmap);Pixmap p=real(d,parent,w,h,dep);remember(p,dep);return p;}
Window XCreateWindow(Display*d,Window parent,int x,int y,unsigned w,unsigned h,unsigned border,int dep,unsigned cls,Visual*v,unsigned long mask,XSetWindowAttributes*a){REAL(XCreateWindow);Window r=real(d,parent,x,y,w,h,border,dep,cls,v,mask,a);remember(r,dep?(unsigned)dep:depth(parent));return r;}
Window XCreateSimpleWindow(Display*d,Window parent,int x,int y,unsigned w,unsigned h,unsigned border,unsigned long bp,unsigned long bg){REAL(XCreateSimpleWindow);Window r=real(d,parent,x,y,w,h,border,bp,bg);remember(r,depth(parent));return r;}
int XFreePixmap(Display*d,Pixmap p){REAL(XFreePixmap);remember(p,0);return real(d,p);}
int XDestroyWindow(Display*d,Window w){REAL(XDestroyWindow);remember(w,0);return real(d,w);}
static pthread_once_t render_once=PTHREAD_ONCE_INIT;
static __typeof__(&XRenderFindStandardFormat) find_format;
static __typeof__(&XRenderCreatePicture) create_picture;
static __typeof__(&XRenderComposite) composite;
static __typeof__(&XRenderFreePicture) free_picture;
static void render_init(void){void*h=dlopen("libXrender.so.1",RTLD_LAZY|RTLD_LOCAL);if(!h)return;find_format=dlsym(h,"XRenderFindStandardFormat");create_picture=dlsym(h,"XRenderCreatePicture");composite=dlsym(h,"XRenderComposite");free_picture=dlsym(h,"XRenderFreePicture");}

static _Atomic(__typeof__(&XRenderCreatePicture)) actual_create_picture;
static _Atomic unsigned picture_fixes;
static Picture capcut_create_picture(Display*d,Drawable drawable,const XRenderPictFormat*format,unsigned long mask,const XRenderPictureAttributes*attributes){
 __typeof__(&XRenderCreatePicture) real=atomic_load(&actual_create_picture);
 unsigned dep=depth(drawable);
 if(active()&&format&&dep!=(unsigned)format->depth&&
    ((dep==32&&format->depth==24)||(dep==24&&format->depth==32))&&
    format->type==PictTypeDirect&&format->direct.red==16&&format->direct.green==8&&format->direct.blue==0&&
    format->direct.redMask==255&&format->direct.greenMask==255&&format->direct.blueMask==255){
  pthread_once(&render_once,render_init);
  XRenderPictFormat*replacement=find_format?find_format(d,dep==32?PictStandardARGB32:PictStandardRGB24):NULL;
  if(replacement){unsigned n=atomic_fetch_add(&picture_fixes,1)+1;
   if(n<=32||n%1000==0)fprintf(stderr,"CAPCUT-XRENDER corrected picture format %d->%u drawable=%lx seq=%lu total=%u\n",format->depth,dep,drawable,NextRequest(d),n);
   format=replacement;
  }
 }
 return real(d,drawable,format,mask,attributes);
}
/* Wine obtains XRender entry points using dlsym on a library handle, which
   bypasses ordinary symbol interposition. For every other lookup, tail-call
   libc so RTLD_NEXT retains the original caller's return address. The build
   checks that the compiler preserves this tail jump. */
typedef void *(*dlsym_fn)(void*,const char*);
static _Atomic(dlsym_fn) libc_dlsym;
#include "present_sync.inc"
__attribute__((noinline)) void*dlsym(void*handle,const char*name){
 dlsym_fn real=atomic_load(&libc_dlsym);
 if(!real){real=(dlsym_fn)dlvsym(RTLD_NEXT,"dlsym","GLIBC_2.2.5");atomic_store(&libc_dlsym,real);}
 if(handle!=RTLD_NEXT&&active()&&present_active()&&
    (!strcmp(name,"vkGetInstanceProcAddr")||!strcmp(name,"vkGetDeviceProcAddr"))){
  void*p=real(handle,name);
  if(p&&present_init(handle,real))return !strcmp(name,"vkGetInstanceProcAddr")?(void*)capcut_gipa:(void*)capcut_gdpa;
  return p;
 }
 if(handle!=RTLD_NEXT&&!strcmp(name,"XRenderCreatePicture")&&active()){
  void*p=real(handle,name);
  if(p){atomic_store(&actual_create_picture,(__typeof__(&XRenderCreatePicture))p);return capcut_create_picture;}
  return p;
 }
 return real(handle,name);
}
#include "gc_tracking.inc"
static int copy_area(Display*d,Drawable src,Drawable dst,GC gc,int sx,int sy,unsigned w,unsigned h,int dx,int dy){
 REAL(XCopyArea);
 if(!active()||!w||!h)return real(d,src,dst,gc,sx,sy,w,h,dx,dy);
 unsigned sd=depth(src),dd=depth(dst);
 if(!sd||!dd){if(unknowns++<8)fprintf(stderr,"CAPCUT-XCOPY untracked drawable src=%lx/%u dst=%lx/%u; unchanged\n",src,sd,dst,dd);return real(d,src,dst,gc,sx,sy,w,h,dx,dy);}
 if(sd==dd||!((sd==24&&dd==32)||(sd==32&&dd==24)))return real(d,src,dst,gc,sx,sy,w,h,dx,dy);
 pthread_once(&render_once,render_init);
 if(!find_format||!create_picture||!composite||!free_picture)return real(d,src,dst,gc,sx,sy,w,h,dx,dy);
 XRenderPictFormat*sf=find_format(d,sd==32?PictStandardARGB32:PictStandardRGB24);
 XRenderPictFormat*df=find_format(d,dd==32?PictStandardARGB32:PictStandardRGB24);
 if(!sf||!df)return real(d,src,dst,gc,sx,sy,w,h,dx,dy);
 Pixmap tmp=XCreatePixmap(d,dst,w,h,dd);
 XRenderPictureAttributes attrs={.subwindow_mode=IncludeInferiors};
 Picture sp=create_picture(d,src,sf,CPSubwindowMode,&attrs);
 Picture tp=create_picture(d,tmp,df,0,NULL);
 composite(d,PictOpSrc,sp,None,tp,sx,sy,0,0,0,0,w,h);
 /* Keep the caller's GC, so its raster op, plane mask and clipping survive. */
 int result=real(d,tmp,dst,gc,0,0,w,h,dx,dy);
 free_picture(d,tp);free_picture(d,sp);XFreePixmap(d,tmp);
 unsigned n=__atomic_add_fetch(&conversions,1,__ATOMIC_RELAXED);
 if(n<=32||n%1000==0)fprintf(stderr,"CAPCUT-XCOPY converted %u->%u src=%lx dst=%lx size=%ux%u total=%u\n",sd,dd,src,dst,w,h,n);
 return result;
}

int XCopyArea(Display*d,Drawable src,Drawable dst,GC gc,int sx,int sy,unsigned w,unsigned h,int dx,int dy){
 GC replacement=active()?compatible_gc(d,dst,gc,depth(dst)):NULL;
 int result=copy_area(d,src,dst,replacement?replacement:gc,sx,sy,w,h,dx,dy);
 if(replacement)XFreeGC(d,replacement);
 return result;
}
