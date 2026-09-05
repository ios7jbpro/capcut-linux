/* CapCut 1.5.0.230 Wine compatibility prototype. No CRT required. */
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned long long SIZE_T;
typedef int BOOL;
typedef void *HANDLE;
typedef void *HMODULE;
typedef unsigned short WCHAR;
#define API __declspec(dllimport)
API HMODULE GetModuleHandleW(const WCHAR*);
API void *GetProcAddress(HMODULE,const char*);
API DWORD GetModuleFileNameW(HMODULE,WCHAR*,DWORD);
API DWORD GetSystemDirectoryW(WCHAR*,DWORD);
API HMODULE LoadLibraryExW(const WCHAR*,HANDLE,DWORD);
API BOOL VirtualProtect(void*,SIZE_T,DWORD,DWORD*);
API BOOL FlushInstructionCache(HANDLE,const void*,SIZE_T);
API HANDLE GetCurrentProcess(void);
API BOOL DisableThreadLibraryCalls(HMODULE);
API DWORD GetEnvironmentVariableA(const char*,char*,DWORD);
API BOOL SetEnvironmentVariableA(const char*,const char*);
API HANDLE CreateFileW(const WCHAR*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
API BOOL WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
API BOOL CloseHandle(HANDLE);
API void OutputDebugStringA(const char*);
API WCHAR *GetCommandLineW(void);
static HMODULE self,real_version;
static HANDLE logfile;
static void *targets[17];
static BOOL applied,software;
#include "payload.h"
static SIZE_T len(const char*s){SIZE_T n=0;while(s[n])n++;return n;}
static BOOL same(const BYTE*a,const BYTE*b,SIZE_T n){while(n--)if(*a++!=*b++)return 0;return 1;}
static void copy(BYTE*d,const BYTE*s,SIZE_T n){while(n--)*d++=*s++;}
static void logmsg(const char*s){DWORD n;if(logfile&&logfile!=(void*)-1)WriteFile(logfile,s,(DWORD)len(s),&n,0);OutputDebugStringA(s);}
static BOOL match_suffix(WCHAR*s,DWORD n,const char*ascii){SIZE_T l=len(ascii);if(n<l)return 0;for(SIZE_T i=0;i<l;i++){WORD c=s[n-l+i];if(c>='A'&&c<='Z')c+=32;if(c!=(BYTE)ascii[i])return 0;}return 1;}
static BOOL has_type_arg(void){WCHAR*s=GetCommandLineW();const char*t="--type=";for(;*s;s++){int i=0;while(t[i]&&s[i]==t[i])i++;if(!t[i])return 1;}return 0;}
static BOOL patch(void*p,const void*newbytes,SIZE_T n){DWORD old,unused;if(!VirtualProtect(p,n,0x40,&old))return 0;copy(p,newbytes,n);FlushInstructionCache(GetCurrentProcess(),p,n);return VirtualProtect(p,n,old,&unused);}
/* Replaces only the graphics configuration routine found with IDA. Called
   on CapCut's normal initialization thread, never from the loader callback. */
static void configure_software(void){
 HMODULE qt=GetModuleHandleW((const WCHAR*)L"Qt6Quick.dll");
 void (*set_api)(int)=(void(*)(int))GetProcAddress(qt,"?setGraphicsApi@QQuickWindow@@SAXW4GraphicsApi@QSGRendererInterface@@@Z");
 SetEnvironmentVariableA("QT_QUICK_BACKEND","software");
 SetEnvironmentVariableA("QSG_RENDER_LOOP","basic");
 if(set_api){set_api(1);logmsg("graphics: selected Qt Quick software backend\r\n");}
 else logmsg("ERROR: Qt Quick graphics entry point unavailable\r\n");
}
static void apply_image(BYTE*b){
 if(applied||!b)return;
 DWORD peoff=*(DWORD*)(b+0x3c);BYTE*pe=b+peoff;
 if(*(WORD*)b!=0x5a4d||*(DWORD*)pe!=0x4550||*(DWORD*)(pe+8)!=0x63eb4224||*(DWORD*)(pe+24+56)!=0x4a22000){logmsg("REFUSED: unsupported VECreator image\r\n");applied=1;return;}
 BYTE*resource=b+0x400ee30;
 if(!same(resource,original_payload,sizeof(original_payload))){logmsg("REFUSED: overlay bytes differ; original VECreator.dll required\r\n");applied=1;return;}
 BYTE*graphics=b+0xa1d2f0;
 if(software&&!same(graphics,graphics_original,sizeof(graphics_original))){logmsg("REFUSED: graphics signature differs\r\n");applied=1;return;}
 if(!patch(resource,replacement_payload,sizeof(replacement_payload))){logmsg("ERROR: overlay memory patch failed\r\n");return;}
 logmsg("overlay: verified and replaced embedded QML in memory\r\n");
 SetEnvironmentVariableA("QML_DISABLE_DISK_CACHE","1");
 if(software){BYTE jump[14]={0xff,0x25,0,0,0,0};*(void**)(jump+6)=configure_software;
  if(patch(graphics,jump,sizeof(jump)))logmsg("graphics: software-mode hook installed\r\n");
  else logmsg("ERROR: graphics hook installation failed\r\n");}
 applied=1;
}
typedef struct {WORD Length,MaximumLength;WCHAR*Buffer;} USTRING;
typedef struct {DWORD Flags;const USTRING*FullDllName;const USTRING*BaseDllName;void*DllBase;DWORD SizeOfImage;} LOAD_INFO;
static void *notification_cookie;
static void loaded(DWORD reason,const LOAD_INFO*info,void*context){
 (void)context;
 if(reason==1&&info->BaseDllName&&info->BaseDllName->Length==26&&match_suffix(info->BaseDllName->Buffer,13,"vecreator.dll"))apply_image(info->DllBase);
}
static SIZE_T failed_forward(void){return 0;}
void *resolve_export(unsigned index){
 if(index>=17)return failed_forward;
 if(targets[index])return targets[index];
 if(!real_version){WCHAR path[512];DWORD n=GetSystemDirectoryW(path,480);if(!n||n>480)return failed_forward;
  const WCHAR*s=(const WCHAR*)L"\\version.dll";while((* (path+n)=*s)){n++;s++;}
  real_version=LoadLibraryExW(path,0,0x800);
  if(!real_version||real_version==self){logmsg("ERROR: system version.dll forwarding failed\r\n");real_version=0;return failed_forward;}
 }
 void*f=GetProcAddress(real_version,export_names[index]);if(!f){logmsg("ERROR: version export missing\r\n");return failed_forward;}
 targets[index]=f;return f;
}
BOOL DllMain(HMODULE module,DWORD reason,void*reserved){
 (void)reserved;if(reason!=1)return 1;self=module;DisableThreadLibraryCalls(module);
 WCHAR exe[512];DWORD n=GetModuleFileNameW(0,exe,512);
 if(!match_suffix(exe,n,"\\capcut.exe")||has_type_arg())return 1;
 char disabled[8];if(GetEnvironmentVariableA("CAPCUT_WINE_DISABLE",disabled,8)&&disabled[0]=='1')return 1;
 HMODULE ntdll=GetModuleHandleW((const WCHAR*)L"ntdll.dll");if(!GetProcAddress(ntdll,"wine_get_version"))return 1;
 WCHAR path[512];n=GetModuleFileNameW(module,path,450);while(n&&path[n-1]!='\\')n--;
 const WCHAR*s=(const WCHAR*)L"capcut-wine-compat.log";while((path[n++]=*s++));
 logfile=CreateFileW(path,0x40000000,3,0,2,0x80,0);
 char mode[8];software=GetEnvironmentVariableA("CAPCUT_WINE_SOFTWARE",mode,8)&&mode[0]=='1';
 logmsg("CapCut Wine compatibility prototype v0.1\r\n");
 logmsg(software?"mode: software interface (experimental)\r\n":"mode: overlay only; graphics unchanged\r\n");
 long (*register_notify)(DWORD,void*,void*,void**)=(void*)GetProcAddress(ntdll,"LdrRegisterDllNotification");
 if(register_notify&&register_notify(0,loaded,0,&notification_cookie)==0)logmsg("loader: watching for VECreator.dll\r\n");
 else logmsg("ERROR: loader notification unavailable\r\n");
 apply_image(GetModuleHandleW((const WCHAR*)L"VECreator.dll"));
 return 1;
}
