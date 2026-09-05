/* Deterministic dispatch/lifetime tests; no GPU or X server needed. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef void *(*dlsym_fn)(void*,const char*);
#include "present_sync.inc"
static VkResult present_result=VK_SUCCESS,wait_result=VK_SUCCESS,create_result=VK_SUCCESS;
static unsigned presents,waits;
static uint64_t waited_id;
static VkDevice current_device=(VkDevice)(uintptr_t)1;
static VkSwapchainKHR next_swapchain=(VkSwapchainKHR)(uintptr_t)10;
static VkResult mock_create(VkPhysicalDevice p,const VkDeviceCreateInfo *i,const VkAllocationCallbacks *a,VkDevice *d){(void)p;(void)i;(void)a;*d=current_device;return VK_SUCCESS;}
static void mock_destroy(VkDevice d,const VkAllocationCallbacks *a){(void)d;(void)a;}
static VkResult mock_swapchain(VkDevice d,const VkSwapchainCreateInfoKHR *i,const VkAllocationCallbacks *a,VkSwapchainKHR *s){(void)d;(void)i;(void)a;*s=next_swapchain;return create_result;}
static void mock_destroy_swapchain(VkDevice d,VkSwapchainKHR s,const VkAllocationCallbacks *a){(void)d;(void)s;(void)a;}
static VkResult mock_present(VkQueue q,const VkPresentInfoKHR *i){(void)q;(void)i;presents++;return present_result;}
static VkResult mock_wait(VkDevice d,VkSwapchainKHR s,uint64_t id,uint64_t timeout){
 assert(d==current_device);assert(s);assert(presents>0);assert(timeout==100000000ULL);
 waits++;waited_id=id;return wait_result;
}
static PFN_vkVoidFunction mock_gdpa(VkDevice d,const char *n){(void)d;if(!strcmp(n,"vkWaitForPresentKHR"))return (PFN_vkVoidFunction)mock_wait;if(!strcmp(n,"missing"))return NULL;return (PFN_vkVoidFunction)mock_present;}
static PFN_vkVoidFunction mock_gipa(VkInstance i,const char *n){(void)i;return mock_gdpa(current_device,n);}
static void *mock_lookup(void *h,const char *n){
 (void)h;
 #define LOOKUP(name,fn) if(!strcmp(n,name))return (void*)fn
 LOOKUP("vkGetInstanceProcAddr",mock_gipa);LOOKUP("vkGetDeviceProcAddr",mock_gdpa);
 LOOKUP("vkCreateDevice",mock_create);LOOKUP("vkDestroyDevice",mock_destroy);
 LOOKUP("vkCreateSwapchainKHR",mock_swapchain);LOOKUP("vkDestroySwapchainKHR",mock_destroy_swapchain);
 LOOKUP("vkQueuePresentKHR",mock_present);
 #undef LOOKUP
 return NULL;
}
int main(void){
 assert(present_active());setenv("CAPCUT_PRESENT_COMPAT","0",1);assert(!present_active());unsetenv("CAPCUT_PRESENT_COMPAT");
 assert(present_init(NULL,mock_lookup));
 assert(capcut_gipa(NULL,"vkGetDeviceProcAddr")== (PFN_vkVoidFunction)capcut_gdpa);
 assert(capcut_gipa(NULL,"vkGetInstanceProcAddr")== (PFN_vkVoidFunction)capcut_gipa);
 assert(capcut_gdpa(current_device,"vkQueuePresentKHR")== (PFN_vkVoidFunction)capcut_queue_present);
 assert(capcut_gipa(NULL,"missing")==NULL);
 assert(capcut_gdpa(current_device,"unrelated")== (PFN_vkVoidFunction)mock_present);
 VkDevice device;VkDeviceCreateInfo dci={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
 capcut_create_device(NULL,&dci,NULL,&device);assert(!present_devices);
 const char *extensions[]={VK_KHR_PRESENT_ID_EXTENSION_NAME,VK_KHR_PRESENT_WAIT_EXTENSION_NAME};
 VkPhysicalDevicePresentIdFeaturesKHR idf={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR,.presentId=VK_TRUE};
 VkPhysicalDevicePresentWaitFeaturesKHR wf={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR,.pNext=&idf,.presentWait=VK_FALSE};
 dci.pNext=&wf;dci.enabledExtensionCount=2;dci.ppEnabledExtensionNames=extensions;
 capcut_create_device(NULL,&dci,NULL,&device);assert(!present_devices);
 wf.presentWait=VK_TRUE;dci.enabledExtensionCount=1;
 capcut_create_device(NULL,&dci,NULL,&device);assert(!present_devices);
 dci.enabledExtensionCount=2;capcut_create_device(NULL,&dci,NULL,&device);assert(present_devices);
 VkSwapchainKHR swaps[2];VkSwapchainCreateInfoKHR sci={.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
 capcut_create_swapchain(device,&sci,NULL,&swaps[0]);next_swapchain=(VkSwapchainKHR)(uintptr_t)11;
 capcut_create_swapchain(device,&sci,NULL,&swaps[1]);
 uint64_t ids[]={42,43};uint32_t indices[]={0,0};
 VkPresentIdKHR id={.sType=VK_STRUCTURE_TYPE_PRESENT_ID_KHR,.swapchainCount=2,.pPresentIds=ids};
 VkPresentInfoKHR pi={.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,.pNext=&id,.swapchainCount=2,.pSwapchains=swaps,.pImageIndices=indices};
 assert(capcut_queue_present(NULL,&pi)==VK_SUCCESS);assert(waits==2&&waited_id==43);
 assert(pi.pNext==&id&&pi.pSwapchains==swaps&&ids[0]==42&&ids[1]==43);
 pi.pNext=NULL;capcut_queue_present(NULL,&pi);assert(waits==2);pi.pNext=&id;
 ids[0]=ids[1]=0;capcut_queue_present(NULL,&pi);assert(waits==2);ids[0]=42;ids[1]=43;
 present_result=VK_ERROR_DEVICE_LOST;assert(capcut_queue_present(NULL,&pi)==VK_ERROR_DEVICE_LOST);assert(waits==2);
 VkResult results[]={VK_ERROR_OUT_OF_DATE_KHR,VK_SUCCESS};pi.pResults=results;
 assert(capcut_queue_present(NULL,&pi)==VK_ERROR_DEVICE_LOST);assert(waits==2);
 present_result=VK_ERROR_OUT_OF_DATE_KHR;assert(capcut_queue_present(NULL,&pi)==VK_ERROR_OUT_OF_DATE_KHR);assert(waits==3&&waited_id==43);
 assert(results[0]==VK_ERROR_OUT_OF_DATE_KHR&&results[1]==VK_SUCCESS);
 pi.pResults=NULL;present_result=VK_SUBOPTIMAL_KHR;wait_result=VK_TIMEOUT;
 assert(capcut_queue_present(NULL,&pi)==VK_SUBOPTIMAL_KHR);assert(waits==5);
 wait_result=VK_ERROR_DEVICE_LOST;assert(capcut_queue_present(NULL,&pi)==VK_SUBOPTIMAL_KHR);assert(waits==7);
 wait_result=VK_SUCCESS;present_result=VK_SUCCESS;
 /* Retire even on failed creation; do not wait on destroyed/retired handles. */
 sci.oldSwapchain=swaps[0];create_result=VK_ERROR_OUT_OF_DEVICE_MEMORY;
 VkSwapchainKHR unused;capcut_create_swapchain(device,&sci,NULL,&unused);
 capcut_queue_present(NULL,&pi);assert(waits==8&&waited_id==43);
 capcut_destroy_swapchain(device,swaps[1],NULL);capcut_queue_present(NULL,&pi);assert(waits==8);
 capcut_destroy_device(device,NULL);assert(!present_devices&&!present_swapchains);
 /* Reused handles on a device without the feature must not inherit state. */
 dci.pNext=NULL;capcut_create_device(NULL,&dci,NULL,&device);capcut_queue_present(NULL,&pi);assert(waits==8);
 puts("PASS: wait follows present; feature gating; proc lookup; IDs; mixed results; bounded timeout/errors; retirement; destruction; handle reuse; original results/data preserved");
}
