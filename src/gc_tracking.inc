/* XCopyArea also requires the GC's creation depth to match the destination.
   Keep the clipping state that XGetGCValues cannot return. Unsupported bitmap
   masks remain untouched rather than risking an unclipped write. */
#include <X11/Xutil.h>
#include <stdint.h>
struct gc_state { GC gc; unsigned dep; int known; Region clip; };
static struct gc_state gcs[4096];
static pthread_mutex_t gc_lock=PTHREAD_MUTEX_INITIALIZER;
static _Atomic unsigned gc_fixes;
static struct gc_state*gc_entry(GC gc,int create){
 unsigned k=((uintptr_t)gc>>4)&4095;
 for(unsigned i=0;i<4096;i++){struct gc_state*s=&gcs[(k+i)&4095];if(s->gc==gc)return s;}
 if(create)for(unsigned i=0;i<4096;i++){struct gc_state*s=&gcs[(k+i)&4095];if(!s->gc){s->gc=gc;return s;}}
 return NULL;
}
static Region clone_region(Region r){if(!r)return NULL;Region n=XCreateRegion();if(n)XUnionRegion(r,r,n);return n;}
static void gc_clip(GC gc,Region clip,int known){
 pthread_mutex_lock(&gc_lock);struct gc_state*s=gc_entry(gc,0);
 if(s){if(s->clip)XDestroyRegion(s->clip);s->clip=clip;s->known=known;clip=NULL;}
 pthread_mutex_unlock(&gc_lock);if(clip)XDestroyRegion(clip);
}
GC XCreateGC(Display*d,Drawable drawable,unsigned long mask,XGCValues*v){
 REAL(XCreateGC);GC gc=real(d,drawable,mask,v);if(!gc||!active())return gc;
 unsigned dep=depth(drawable);pthread_mutex_lock(&gc_lock);struct gc_state*s=gc_entry(gc,1);
 if(s){s->dep=dep;s->known=!(mask&GCClipMask)||v->clip_mask==None;s->clip=NULL;}
 pthread_mutex_unlock(&gc_lock);return gc;
}
int XFreeGC(Display*d,GC gc){
 REAL(XFreeGC);pthread_mutex_lock(&gc_lock);struct gc_state*s=gc_entry(gc,0);
 if(s){if(s->clip)XDestroyRegion(s->clip);memset(s,0,sizeof(*s));}
 pthread_mutex_unlock(&gc_lock);return real(d,gc);
}
int XSetClipMask(Display*d,GC gc,Pixmap mask){REAL(XSetClipMask);int r=real(d,gc,mask);gc_clip(gc,NULL,mask==None);return r;}
int XChangeGC(Display*d,GC gc,unsigned long mask,XGCValues*v){REAL(XChangeGC);int r=real(d,gc,mask,v);if(mask&GCClipMask)gc_clip(gc,NULL,v->clip_mask==None);return r;}
int XSetClipRectangles(Display*d,GC gc,int x,int y,XRectangle*rects,int n,int ordering){
 REAL(XSetClipRectangles);int result=real(d,gc,x,y,rects,n,ordering);
 Region r=XCreateRegion();if(r)for(int i=0;i<n;i++)XUnionRectWithRegion(&rects[i],r,r);
 gc_clip(gc,r,r!=NULL);return result;
}
int XSetRegion(Display*d,GC gc,Region region){REAL(XSetRegion);int result=real(d,gc,region);Region r=clone_region(region);gc_clip(gc,r,r!=NULL);return result;}
int XCopyGC(Display*d,GC src,unsigned long mask,GC dst){
 REAL(XCopyGC);int result=real(d,src,mask,dst);
 if(mask&GCClipMask){Region r=NULL;int known=0;pthread_mutex_lock(&gc_lock);struct gc_state*s=gc_entry(src,0);
  if(s){known=s->known;r=clone_region(s->clip);if(s->clip&&!r)known=0;}
  pthread_mutex_unlock(&gc_lock);gc_clip(dst,r,known);}
 return result;
}
static GC compatible_gc(Display*d,Drawable dst,GC gc,unsigned dd){
 unsigned dep=0;int known=0;Region clip=NULL;
 pthread_mutex_lock(&gc_lock);struct gc_state*s=gc_entry(gc,0);
 if(s){dep=s->dep;known=s->known;if(dep&&dep!=dd){clip=clone_region(s->clip);if(s->clip&&!clip)known=0;}}
 pthread_mutex_unlock(&gc_lock);
 if(!known||!((dep==24&&dd==32)||(dep==32&&dd==24))){if(clip)XDestroyRegion(clip);return NULL;}
 XGCValues v;unsigned long mask=GCFunction|GCPlaneMask|GCSubwindowMode|GCGraphicsExposures|GCClipXOrigin|GCClipYOrigin;
 if(!XGetGCValues(d,gc,mask,&v)){if(clip)XDestroyRegion(clip);return NULL;}
 GC replacement=XCreateGC(d,dst,mask,&v);
 if(replacement&&clip){XSetRegion(d,replacement,clip);XSetClipOrigin(d,replacement,v.clip_x_origin,v.clip_y_origin);}
 if(clip)XDestroyRegion(clip);
 if(replacement){unsigned n=atomic_fetch_add(&gc_fixes,1)+1;if(n<=32||n%1000==0)fprintf(stderr,"CAPCUT-XGC corrected %u->%u dst=%lx seq=%lu total=%u\n",dep,dd,dst,NextRequest(d),n);}
 return replacement;
}
