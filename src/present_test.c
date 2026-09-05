/* Real Vulkan/X11 regression: present one frame, immediately copy its window,
 * then read the copy. No second frame or input event may reveal the update.
 * Resolve Vulkan exactly as Wine does, through a dlopen library handle.
 */
#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>
#include <X11/Xutil.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(call) do { VkResult r=(call); if(r!=VK_SUCCESS&&r!=VK_SUBOPTIMAL_KHR){fprintf(stderr,"%s: %d\n",#call,r);exit(2);} }while(0)
#define INSTANCE(n) PFN_##n n=(PFN_##n)gipa(instance,#n);assert(n)
#define DEVICE(n) PFN_##n n=(PFN_##n)gdpa(device,#n);assert(n)
int main(void) {
 Display *display=XOpenDisplay(NULL);assert(display);
 Window root=DefaultRootWindow(display);
 Window window=XCreateSimpleWindow(display,root,10,800,128,128,0,0,0);
 XStoreName(display,window,"CapCut presentation regression");
 XMapWindow(display,window);XSync(display,False);
 Pixmap copy=XCreatePixmap(display,root,128,128,DefaultDepth(display,DefaultScreen(display)));
 GC gc=XCreateGC(display,copy,0,NULL);
 void *lib=dlopen("libvulkan.so.1",RTLD_NOW|RTLD_LOCAL);assert(lib);
 PFN_vkGetInstanceProcAddr gipa=(PFN_vkGetInstanceProcAddr)dlsym(lib,"vkGetInstanceProcAddr");assert(gipa);
 PFN_vkCreateInstance vkCreateInstance=(PFN_vkCreateInstance)gipa(VK_NULL_HANDLE,"vkCreateInstance");
 const char *iext[]={VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_XLIB_SURFACE_EXTENSION_NAME};
 VkApplicationInfo app={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="capcut-present-test",.apiVersion=VK_API_VERSION_1_2};
 VkInstanceCreateInfo ici={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&app,.enabledExtensionCount=2,.ppEnabledExtensionNames=iext};
 VkInstance instance;CHECK(vkCreateInstance(&ici,NULL,&instance));
 INSTANCE(vkEnumeratePhysicalDevices);INSTANCE(vkGetPhysicalDeviceProperties);
 INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties);INSTANCE(vkCreateDevice);
 INSTANCE(vkCreateXlibSurfaceKHR);INSTANCE(vkGetPhysicalDeviceSurfaceSupportKHR);
 INSTANCE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR);
 INSTANCE(vkDestroySurfaceKHR);INSTANCE(vkDestroyInstance);
 PFN_vkGetDeviceProcAddr gdpa=(PFN_vkGetDeviceProcAddr)gipa(instance,"vkGetDeviceProcAddr");
 VkXlibSurfaceCreateInfoKHR sci={.sType=VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,.dpy=display,.window=window};
 VkSurfaceKHR surface;CHECK(vkCreateXlibSurfaceKHR(instance,&sci,NULL,&surface));
 uint32_t count=16;VkPhysicalDevice physicals[16];CHECK(vkEnumeratePhysicalDevices(instance,&count,physicals));
 VkPhysicalDevice physical=physicals[0];
 for(uint32_t i=0;i<count;i++){VkPhysicalDeviceProperties p;vkGetPhysicalDeviceProperties(physicals[i],&p);if(p.vendorID==0x10de)physical=physicals[i];}
 VkPhysicalDeviceProperties props;vkGetPhysicalDeviceProperties(physical,&props);printf("GPU: %s\n",props.deviceName);
 VkQueueFamilyProperties families[32];count=32;vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,families);
 uint32_t family=UINT32_MAX;
 for(uint32_t i=0;i<count;i++){VkBool32 supported=0;CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physical,i,surface,&supported));if(supported&&(families[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)){family=i;break;}}
 assert(family!=UINT32_MAX);float priority=1;
 VkDeviceQueueCreateInfo qci={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=family,.queueCount=1,.pQueuePriorities=&priority};
 const char *dext[]={VK_KHR_SWAPCHAIN_EXTENSION_NAME,VK_KHR_PRESENT_ID_EXTENSION_NAME,VK_KHR_PRESENT_WAIT_EXTENSION_NAME};
 VkPhysicalDevicePresentIdFeaturesKHR idfeature={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR,.presentId=VK_TRUE};
 VkPhysicalDevicePresentWaitFeaturesKHR waitfeature={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR,.pNext=&idfeature,.presentWait=VK_TRUE};
 VkDeviceCreateInfo dci={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.pNext=&waitfeature,.queueCreateInfoCount=1,.pQueueCreateInfos=&qci,.enabledExtensionCount=3,.ppEnabledExtensionNames=dext};
 VkDevice device;CHECK(vkCreateDevice(physical,&dci,NULL,&device));
 DEVICE(vkGetDeviceQueue);DEVICE(vkCreateSwapchainKHR);DEVICE(vkGetSwapchainImagesKHR);
 DEVICE(vkCreateCommandPool);DEVICE(vkAllocateCommandBuffers);DEVICE(vkBeginCommandBuffer);
 DEVICE(vkCmdPipelineBarrier);DEVICE(vkCmdClearColorImage);DEVICE(vkEndCommandBuffer);
 DEVICE(vkCreateSemaphore);DEVICE(vkCreateFence);DEVICE(vkWaitForFences);DEVICE(vkResetFences);
 DEVICE(vkAcquireNextImageKHR);DEVICE(vkQueueSubmit);DEVICE(vkQueuePresentKHR);DEVICE(vkQueueWaitIdle);
 DEVICE(vkDestroySemaphore);DEVICE(vkDestroyFence);DEVICE(vkDestroyCommandPool);
 DEVICE(vkDestroySwapchainKHR);DEVICE(vkDestroyDevice);
 VkQueue queue;vkGetDeviceQueue(device,family,0,&queue);
 VkSurfaceCapabilitiesKHR caps;CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps));
 VkSurfaceFormatKHR formats[128];count=128;CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&count,formats));
 VkSurfaceFormatKHR format=formats[0];for(uint32_t i=0;i<count;i++)if(formats[i].format==VK_FORMAT_B8G8R8A8_UNORM)format=formats[i];
 assert(caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT);
 VkSwapchainCreateInfoKHR ci={.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,.surface=surface,.minImageCount=caps.minImageCount+1,.imageFormat=format.format,.imageColorSpace=format.colorSpace,.imageExtent=caps.currentExtent,.imageArrayLayers=1,.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT,.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,.preTransform=caps.currentTransform,.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,.presentMode=VK_PRESENT_MODE_FIFO_KHR,.clipped=VK_TRUE};
 if(caps.maxImageCount&&ci.minImageCount>caps.maxImageCount)ci.minImageCount=caps.maxImageCount;
 VkSwapchainKHR swapchain;CHECK(vkCreateSwapchainKHR(device,&ci,NULL,&swapchain));
 VkImage images[16];count=16;CHECK(vkGetSwapchainImagesKHR(device,swapchain,&count,images));
 VkCommandPoolCreateInfo pci={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,.queueFamilyIndex=family};
 VkCommandPool pool;CHECK(vkCreateCommandPool(device,&pci,NULL,&pool));
 VkCommandBufferAllocateInfo cai={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
 VkCommandBuffer command;CHECK(vkAllocateCommandBuffers(device,&cai,&command));
 VkSemaphoreCreateInfo semci={.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
 VkSemaphore ready,done[16];CHECK(vkCreateSemaphore(device,&semci,NULL,&ready));
 for(uint32_t i=0;i<count;i++)CHECK(vkCreateSemaphore(device,&semci,NULL,&done[i]));
 VkFenceCreateInfo fci={.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence fence;CHECK(vkCreateFence(device,&fci,NULL,&fence));
 unsigned stale=0;
 for(uint64_t frame=1;frame<=12;frame++){
  uint32_t index;CHECK(vkAcquireNextImageKHR(device,swapchain,UINT64_MAX,ready,VK_NULL_HANDLE,&index));
  VkCommandBufferBeginInfo bi={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};CHECK(vkBeginCommandBuffer(command,&bi));
  VkImageSubresourceRange range={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,.levelCount=1,.layerCount=1};
  VkImageMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.image=images[index],.subresourceRange=range};
  vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
  VkClearColorValue color={.float32={0,0,0,1}};color.float32[(frame-1)%3]=1;
  vkCmdClearColorImage(command,images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color,1,&range);
  barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=0;barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,NULL,0,NULL,1,&barrier);
  CHECK(vkEndCommandBuffer(command));VkPipelineStageFlags stage=VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.waitSemaphoreCount=1,.pWaitSemaphores=&ready,.pWaitDstStageMask=&stage,.commandBufferCount=1,.pCommandBuffers=&command,.signalSemaphoreCount=1,.pSignalSemaphores=&done[index]};
  CHECK(vkQueueSubmit(queue,1,&submit,fence));
  VkPresentIdKHR id={.sType=VK_STRUCTURE_TYPE_PRESENT_ID_KHR,.swapchainCount=1,.pPresentIds=&frame};
  VkPresentInfoKHR pi={.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,.pNext=&id,.waitSemaphoreCount=1,.pWaitSemaphores=&done[index],.swapchainCount=1,.pSwapchains=&swapchain,.pImageIndices=&index};
  CHECK(vkQueuePresentKHR(queue,&pi));
  XCopyArea(display,window,copy,gc,0,0,128,128,0,0);
  XImage *image=XGetImage(display,copy,64,64,1,1,AllPlanes,ZPixmap);assert(image);
  unsigned long pixel=XGetPixel(image,0,0)&0xffffffUL,expected=0xff0000UL>>(((frame-1)%3)*8);
  if(pixel!=expected)stale++;
  printf("frame=%llu expected=%06lx copied=%06lx %s\n",(unsigned long long)frame,expected,pixel,pixel==expected?"current":"STALE");XDestroyImage(image);
  CHECK(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX));CHECK(vkResetFences(device,1,&fence));
  usleep(50000); /* idle between frames, never before the copy */
 }
 CHECK(vkQueueWaitIdle(queue));vkDestroyFence(device,fence,NULL);vkDestroySemaphore(device,ready,NULL);
 for(uint32_t i=0;i<count;i++)vkDestroySemaphore(device,done[i],NULL);
 vkDestroyCommandPool(device,pool,NULL);vkDestroySwapchainKHR(device,swapchain,NULL);vkDestroyDevice(device,NULL);
 vkDestroySurfaceKHR(instance,surface,NULL);vkDestroyInstance(instance,NULL);
 XFreeGC(display,gc);XFreePixmap(display,copy);XDestroyWindow(display,window);XCloseDisplay(display);
 printf("%s: %u/12 stale copies\n",stale?"FAIL":"PASS",stale);return stale?1:0;
}
