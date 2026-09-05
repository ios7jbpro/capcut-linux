/* Wine copies offscreen Vulkan X11 windows immediately after host Present.
 * Wait for DXVK's existing present ID before returning to that copy. Queue idle
 * is insufficient: rendering completion is not presentation completion.
 * No injected frames, sleeps, changed present modes, or new Vulkan features.
 */
#include <vulkan/vulkan.h>
#include <stdint.h>

static int present_active(void) {
 const char *s=getenv("CAPCUT_PRESENT_COMPAT");
 return !s||strcmp(s,"0");
}
static pthread_mutex_t present_lock=PTHREAD_MUTEX_INITIALIZER;
static PFN_vkGetInstanceProcAddr host_gipa;
static PFN_vkGetDeviceProcAddr host_gdpa;
static PFN_vkCreateDevice host_create_device;
static PFN_vkDestroyDevice host_destroy_device;
static PFN_vkCreateSwapchainKHR host_create_swapchain;
static PFN_vkDestroySwapchainKHR host_destroy_swapchain;
static PFN_vkQueuePresentKHR host_present;
static _Atomic unsigned present_waits, present_failures;
struct present_device {
 VkDevice device;
 PFN_vkWaitForPresentKHR wait;
 struct present_device *next;
};
struct present_swapchain {
 VkSwapchainKHR swapchain;
 VkDevice device;
 PFN_vkWaitForPresentKHR wait;
 int retired;
 struct present_swapchain *next;
};
static struct present_device *present_devices;
static struct present_swapchain *present_swapchains;

static const void *present_chain(const void *chain,VkStructureType type) {
 for(const VkBaseInStructure *p=chain;p;p=p->pNext)if(p->sType==type)return p;
 return NULL;
}
static int present_extension(const VkDeviceCreateInfo *info,const char *name) {
 for(uint32_t i=0;i<info->enabledExtensionCount;i++)
  if(!strcmp(info->ppEnabledExtensionNames[i],name))return 1;
 return 0;
}
static VkResult capcut_create_device(VkPhysicalDevice physical,const VkDeviceCreateInfo *info,
                                    const VkAllocationCallbacks *alloc,VkDevice *device) {
 VkResult result=host_create_device(physical,info,alloc,device);
 const VkPhysicalDevicePresentWaitFeaturesKHR *wait=present_chain(info->pNext,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR);
 const VkPhysicalDevicePresentIdFeaturesKHR *id=present_chain(info->pNext,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR);
 if(result!=VK_SUCCESS||!wait||!wait->presentWait||!id||!id->presentId||
    !present_extension(info,VK_KHR_PRESENT_WAIT_EXTENSION_NAME)||
    !present_extension(info,VK_KHR_PRESENT_ID_EXTENSION_NAME))return result;
 PFN_vkWaitForPresentKHR fn=(PFN_vkWaitForPresentKHR)host_gdpa(*device,"vkWaitForPresentKHR");
 struct present_device *state=fn?calloc(1,sizeof(*state)):NULL;
 if(state){
  state->device=*device;state->wait=fn;
  pthread_mutex_lock(&present_lock);state->next=present_devices;present_devices=state;pthread_mutex_unlock(&present_lock);
  fprintf(stderr,"CAPCUT-PRESENT enabled completion wait on device=%p\n",(void*)*device);
 }
 return result;
}
static void capcut_destroy_device(VkDevice device,const VkAllocationCallbacks *alloc) {
 pthread_mutex_lock(&present_lock);
 for(struct present_device **p=&present_devices;*p;){
  if((*p)->device==device){struct present_device *old=*p;*p=old->next;free(old);}else p=&(*p)->next;
 }
 for(struct present_swapchain **p=&present_swapchains;*p;){
  if((*p)->device==device){struct present_swapchain *old=*p;*p=old->next;free(old);}else p=&(*p)->next;
 }
 pthread_mutex_unlock(&present_lock);
 host_destroy_device(device,alloc);
}
static VkResult capcut_create_swapchain(VkDevice device,const VkSwapchainCreateInfoKHR *info,
                                       const VkAllocationCallbacks *alloc,VkSwapchainKHR *swapchain) {
 /* oldSwapchain is retired even if replacement creation fails. */
 pthread_mutex_lock(&present_lock);
 for(struct present_swapchain *s=present_swapchains;s;s=s->next)
  if(s->device==device&&s->swapchain==info->oldSwapchain)s->retired=1;
 pthread_mutex_unlock(&present_lock);
 VkResult result=host_create_swapchain(device,info,alloc,swapchain);
 if(result!=VK_SUCCESS)return result;
 pthread_mutex_lock(&present_lock);
 for(struct present_device *d=present_devices;d;d=d->next)if(d->device==device){
  struct present_swapchain *s=calloc(1,sizeof(*s));
  if(s){s->device=device;s->swapchain=*swapchain;s->wait=d->wait;s->next=present_swapchains;present_swapchains=s;}
  break;
 }
 pthread_mutex_unlock(&present_lock);
 return result;
}
static void capcut_destroy_swapchain(VkDevice device,VkSwapchainKHR swapchain,const VkAllocationCallbacks *alloc) {
 pthread_mutex_lock(&present_lock);
 for(struct present_swapchain **p=&present_swapchains;*p;){
  if((*p)->device==device&&(*p)->swapchain==swapchain){struct present_swapchain *old=*p;*p=old->next;free(old);}else p=&(*p)->next;
 }
 pthread_mutex_unlock(&present_lock);
 host_destroy_swapchain(device,swapchain,alloc);
}
static VkResult capcut_queue_present(VkQueue queue,const VkPresentInfoKHR *info) {
 VkResult result=host_present(queue,info);
 /* Fatal queue failures do not establish a presentation to wait for, even
  * when the caller's per-swapchain result array still contains success. */
 if(result!=VK_SUCCESS&&result!=VK_SUBOPTIMAL_KHR&&result!=VK_ERROR_OUT_OF_DATE_KHR&&
    result!=VK_ERROR_SURFACE_LOST_KHR&&result!=VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)return result;
 const VkPresentIdKHR *ids=present_chain(info->pNext,VK_STRUCTURE_TYPE_PRESENT_ID_KHR);
 if(!ids||ids->swapchainCount!=info->swapchainCount)return result;
 for(uint32_t i=0;i<info->swapchainCount;i++){
  VkResult status=info->pResults?info->pResults[i]:result;
  if((status!=VK_SUCCESS&&status!=VK_SUBOPTIMAL_KHR)||!ids->pPresentIds[i])continue;
  VkDevice device=VK_NULL_HANDLE;PFN_vkWaitForPresentKHR wait=NULL;
  pthread_mutex_lock(&present_lock);
  for(struct present_swapchain *s=present_swapchains;s;s=s->next)
   if(s->swapchain==info->pSwapchains[i]&&!s->retired){device=s->device;wait=s->wait;break;}
  pthread_mutex_unlock(&present_lock);
  if(!wait)continue;
  /* Bound the wait so a hidden/lost surface cannot hang CapCut's UI thread.
   * Preserve the driver's present result on timeout/error and report it. */
  VkResult waited=wait(device,info->pSwapchains[i],ids->pPresentIds[i],100000000ULL);
  if(waited==VK_SUCCESS){
   unsigned n=atomic_fetch_add(&present_waits,1)+1;
   if(n<=8||n%1000==0)fprintf(stderr,"CAPCUT-PRESENT completed id=%llu total=%u\n",(unsigned long long)ids->pPresentIds[i],n);
  }else{
   unsigned n=atomic_fetch_add(&present_failures,1)+1;
   if(n<=8||n%1000==0)fprintf(stderr,"CAPCUT-PRESENT wait returned %d id=%llu total=%u\n",waited,(unsigned long long)ids->pPresentIds[i],n);
  }
 }
 return result;
}
static PFN_vkVoidFunction present_proc(const char *name,PFN_vkVoidFunction original) {
 if(!original)return NULL;
 #define PRESENT_PROC(n,f) if(!strcmp(name,n))return (PFN_vkVoidFunction)f
 PRESENT_PROC("vkCreateDevice",capcut_create_device);
 PRESENT_PROC("vkDestroyDevice",capcut_destroy_device);
 PRESENT_PROC("vkCreateSwapchainKHR",capcut_create_swapchain);
 PRESENT_PROC("vkDestroySwapchainKHR",capcut_destroy_swapchain);
 PRESENT_PROC("vkQueuePresentKHR",capcut_queue_present);
 #undef PRESENT_PROC
 return original;
}
static PFN_vkVoidFunction capcut_gdpa(VkDevice device,const char *name) {
 PFN_vkVoidFunction original=host_gdpa(device,name);
 if(original&&!strcmp(name,"vkGetDeviceProcAddr"))return (PFN_vkVoidFunction)capcut_gdpa;
 return present_proc(name,original);
}
static PFN_vkVoidFunction capcut_gipa(VkInstance instance,const char *name) {
 PFN_vkVoidFunction original=host_gipa(instance,name);
 if(original&&!strcmp(name,"vkGetDeviceProcAddr"))return (PFN_vkVoidFunction)capcut_gdpa;
 if(original&&!strcmp(name,"vkGetInstanceProcAddr"))return (PFN_vkVoidFunction)capcut_gipa;
 return present_proc(name,original);
}
static int present_init(void *handle,dlsym_fn real) {
 pthread_mutex_lock(&present_lock);
 if(!host_gipa){
  /* Use the loader's exported dispatch trampolines, not one device's driver
   * pointers: CapCut creates several Vulkan instances and devices. */
  PFN_vkGetInstanceProcAddr gipa=(PFN_vkGetInstanceProcAddr)real(handle,"vkGetInstanceProcAddr");
  PFN_vkGetDeviceProcAddr gdpa=(PFN_vkGetDeviceProcAddr)real(handle,"vkGetDeviceProcAddr");
  PFN_vkCreateDevice cd=(PFN_vkCreateDevice)real(handle,"vkCreateDevice");
  PFN_vkDestroyDevice dd=(PFN_vkDestroyDevice)real(handle,"vkDestroyDevice");
  PFN_vkCreateSwapchainKHR cs=(PFN_vkCreateSwapchainKHR)real(handle,"vkCreateSwapchainKHR");
  PFN_vkDestroySwapchainKHR ds=(PFN_vkDestroySwapchainKHR)real(handle,"vkDestroySwapchainKHR");
  PFN_vkQueuePresentKHR qp=(PFN_vkQueuePresentKHR)real(handle,"vkQueuePresentKHR");
  if(gipa&&gdpa&&cd&&dd&&cs&&ds&&qp){
   host_gipa=gipa;host_gdpa=gdpa;host_create_device=cd;host_destroy_device=dd;
   host_create_swapchain=cs;host_destroy_swapchain=ds;host_present=qp;
  }
 }
 int ready=host_gipa!=NULL;
 pthread_mutex_unlock(&present_lock);
 return ready;
}
