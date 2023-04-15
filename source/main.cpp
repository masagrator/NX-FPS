#include <switch_min.h>
#include "saltysd/SaltySD_ipc.h"
#include "saltysd/SaltySD_dynamic.h"
#include "saltysd/SaltySD_core.h"
#include "ltoa.h"
#include <cstdlib>
#include "lock.hpp"

extern "C" {
	extern u32 __start__;

	static char g_heap[0x10000];

	void __libnx_init(void* ctx, Handle main_thread, void* saved_lr);
	void __attribute__((weak)) NORETURN __libnx_exit(int rc);
	void __nx_exit(int, void*);
	void __libc_fini_array(void);
	void __libc_init_array(void);
	extern u64 nvnBootstrapLoader(const char * nvnName) LINKABLE;
	extern u64 _ZN2nn2os13GetSystemTickEv() LINKABLE;
	extern int eglSwapBuffers(void* EGLDisplay, void* EGLSurface) LINKABLE;
	extern int eglSwapInterval(void* EGLDisplay, int interval) LINKABLE;
	extern u32 vkQueuePresentKHR(void* vk_unk1, void* vk_unk2) LINKABLE;
}

u32 __nx_applet_type = AppletType_None;
Handle orig_main_thread;
void* orig_ctx;
void* orig_saved_lr;
SharedMemory _sharedmemory = {};
Handle remoteSharedMemory = 0;
ptrdiff_t SharedMemoryOffset = 1234;
uint8_t* configBuffer = 0;
size_t configSize = 0;
Result configRC = 1;


Result readConfig(const char* path, uint8_t** output_buffer) {
	FILE* patch_file = SaltySDCore_fopen(path, "rb");
	SaltySDCore_fseek(patch_file, 0, 2);
	configSize = SaltySDCore_ftell(patch_file);
	SaltySDCore_fclose(patch_file);
	uint8_t* buffer = (uint8_t*)calloc(1, configSize);
	patch_file = SaltySDCore_fopen(path, "r");
	SaltySDCore_fread(buffer, configSize, 1, patch_file);
	SaltySDCore_fclose(patch_file);
	if (!LOCK::isValid(buffer, configSize)) {
		free(buffer);
		return 1;
	}
	*output_buffer = buffer;
	return 0;
}

void __libnx_init(void* ctx, Handle main_thread, void* saved_lr) {
	extern char* fake_heap_start;
	extern char* fake_heap_end;

	fake_heap_start = &g_heap[0];
	fake_heap_end   = &g_heap[sizeof g_heap];
	
	orig_ctx = ctx;
	orig_main_thread = main_thread;
	orig_saved_lr = saved_lr;
	
	// Call constructors.
	//void __libc_init_array(void);
	__libc_init_array();
	virtmemSetup();
}

void __attribute__((weak)) NORETURN __libnx_exit(int rc) {
	// Call destructors.
	//void __libc_fini_array(void);
	__libc_fini_array();

	SaltySDCore_printf("SaltySD Plugin: jumping to %p\n", orig_saved_lr);

	__nx_exit(0, orig_saved_lr);
	while (true);
}

uint8_t* FPS_shared = 0;
float* FPSavg_shared = 0;
bool* pluginActive = 0;
uint8_t FPS = 0xFF;
float FPSavg = 255;
uintptr_t ptr_nvnDeviceGetProcAddress;
uintptr_t ptr_nvnQueuePresentTexture;
uintptr_t addr_nvnGetProcAddress;
uintptr_t addr_nvnPresentTexture;
float systemtickfrequency = 19200000;
typedef void (*nvnQueuePresentTexture_0)(void* _this, void* unk2_1, void* unk3_1);
typedef uintptr_t (*GetProcAddress)(void* unk1_a, const char * nvnFunction_a);

uint8_t* FPSlocked_shared = 0;
uint8_t* FPSmode_shared = 0;
void* ptr_Framebuffer = 0;
bool* ZeroSync_shared = 0;
bool* patchApplied_shared = 0;
uint8_t* API_shared = 0;
bool changeFPS = false;
bool changedFPS = false;
bool FPSmode = 0;
uintptr_t addr_nvnSetPresentInterval;
uintptr_t addr_nvnSyncWait;
uintptr_t ptr_nvnWindowSetPresentInterval;
uintptr_t ptr_nvnWindowGetPresentInterval;
uintptr_t ptr_nvnSyncWait;
typedef void (*nvnSetPresentInterval_0)(void* nvnWindow, int mode);
typedef int (*nvnGetPresentInterval_0)(void* nvnWindow);
typedef void* (*nvnSyncWait_0)(void* _this, uint64_t timeout_ns);

inline void createBuildidPath(uint64_t buildid, char* titleid, char* buffer) {
	strcpy(buffer, "sdmc:/SaltySD/plugins/FPSLocker/patches/0");
	strcat(buffer, &titleid[0]);
	strcat(buffer, "/");
	ltoa(buildid, &titleid[0], 16);
	int zero_count = 16 - strlen(&titleid[0]);
	for (int i = 0; i < zero_count; i++) {
		strcat(buffer, "0");
	}
	strcat(buffer, &titleid[0]);
	strcat(buffer, ".bin");	
}

inline void CheckTitleID(char* buffer) {
    uint64_t titid = 0;
    svcGetInfo(&titid, 18, CUR_PROCESS_HANDLE, 0);	
    ltoa(titid, buffer, 16);
}

inline uint64_t getMainAddress() {
	MemoryInfo memoryinfo = {0};
	u32 pageinfo = 0;

	uint64_t base_address = SaltySDCore_getCodeStart() + 0x4000;
	Result rc = svcQueryMemory(&memoryinfo, &pageinfo, base_address);
	if (R_FAILED(rc)) return 0;
	if ((memoryinfo.addr == base_address) && (memoryinfo.perm & Perm_X))
		return base_address;
	base_address = memoryinfo.addr+memoryinfo.size;
	rc = svcQueryMemory(&memoryinfo, &pageinfo, base_address);
	if (R_FAILED(rc)) return 0;
	if ((memoryinfo.addr == base_address) && (memoryinfo.perm & Perm_X))
		return base_address;
	base_address = memoryinfo.addr+memoryinfo.size;
	rc = svcQueryMemory(&memoryinfo, &pageinfo, base_address);
	if (R_FAILED(rc)) return 0;
	if ((memoryinfo.addr == base_address) && (memoryinfo.perm & Perm_X))
		return base_address;
	else return 0;
}

uint32_t vulkanSwap (void* vk_unk1_1, void* vk_unk2_1) {
	static uint8_t FPS_temp = 0;
	static uint64_t starttick = 0;
	static uint64_t endtick = 0;
	static uint64_t deltatick = 0;
	static uint64_t frameend = 0;
	static uint64_t framedelta = 0;
	static uint64_t frameavg = 0;
	static uint8_t FPSlock = 0;
	static uint32_t FPStiming = 0;
	
	if (!starttick) {
		*API_shared = 3;
		starttick = _ZN2nn2os13GetSystemTickEv();
	}
	if (FPStiming && !LOCK::blockDelayFPS) {
		while ((_ZN2nn2os13GetSystemTickEv() - frameend) < FPStiming) {
			svcSleepThread(100000);
		}
	}

	uint32_t vulkanResult = vkQueuePresentKHR(vk_unk1_1, vk_unk2_1);
	endtick = _ZN2nn2os13GetSystemTickEv();
	framedelta = endtick - frameend;
	frameavg = ((9*frameavg) + framedelta) / 10;
	FPSavg = systemtickfrequency / (float)frameavg;
	frameend = endtick;
	
	FPS_temp++;
	deltatick = endtick - starttick;
	if (deltatick >= 19200000) {
		starttick = _ZN2nn2os13GetSystemTickEv();
		FPS = FPS_temp - 1;
		FPS_temp = 0;
		*FPS_shared = FPS;
		if (changeFPS && !configRC && FPSlock) {
			LOCK::applyPatch(configBuffer, configSize, FPSlock);
			*patchApplied_shared = true;
		}
	}

	*FPSavg_shared = FPSavg;
	*pluginActive = true;

	if (FPSlock != *FPSlocked_shared) {
		if ((*FPSlocked_shared < 60) && (*FPSlocked_shared > 0)) {
			FPStiming = (19200000/(*FPSlocked_shared)) - 7800;
		}
		else FPStiming = 0;
		FPSlock = *FPSlocked_shared;
	}
	
	return vulkanResult;
}

int eglInterval(void* EGLDisplay, int interval) {
	int result = false;
	if (!changeFPS) {
		result = eglSwapInterval(EGLDisplay, interval);
		changedFPS = false;
		*FPSmode_shared = interval;
	}
	else if (interval < 0) {
		interval *= -1;
		if (*FPSmode_shared != interval) {
			result = eglSwapInterval(EGLDisplay, interval);
			*FPSmode_shared = interval;
		}
		changedFPS = true;
	}
	return result;
}

int eglSwap (void* EGLDisplay, void* EGLSurface) {
	static uint8_t FPS_temp = 0;
	static uint64_t starttick = 0;
	static uint64_t endtick = 0;
	static uint64_t deltatick = 0;
	static uint64_t frameend = 0;
	static uint64_t framedelta = 0;
	static uint64_t frameavg = 0;
	static uint8_t FPSlock = 0;
	static uint32_t FPStiming = 0;
	int result = 0;

	if (!starttick) {
		*API_shared = 2;
		starttick = _ZN2nn2os13GetSystemTickEv();
	}
	if (FPStiming && !LOCK::blockDelayFPS) {
		while ((_ZN2nn2os13GetSystemTickEv() - frameend) < FPStiming) {
			svcSleepThread(100000);
		}
	}
	
	result = eglSwapBuffers(EGLDisplay, EGLSurface);
	endtick = _ZN2nn2os13GetSystemTickEv();
	framedelta = endtick - frameend;
	frameavg = ((9*frameavg) + framedelta) / 10;
	FPSavg = systemtickfrequency / (float)frameavg;
	frameend = endtick;
	
	FPS_temp++;
	deltatick = endtick - starttick;
	if (deltatick >= 19200000) {
		starttick = _ZN2nn2os13GetSystemTickEv();
		FPS = FPS_temp - 1;
		FPS_temp = 0;
		*FPS_shared = FPS;
		if (changeFPS && !configRC && FPSlock) {
			LOCK::applyPatch(configBuffer, configSize, FPSlock);
			*patchApplied_shared = true;
		}
	}
	
	*FPSavg_shared = FPSavg;
	*pluginActive = true;

	if (FPSlock != *FPSlocked_shared) {
		changeFPS = true;
		changedFPS = false;
		if (*FPSlocked_shared == 0) {
			FPStiming = 0;
			changeFPS = false;
			FPSlock = *FPSlocked_shared;
		}
		else if (*FPSlocked_shared <= 30) {
			eglInterval(EGLDisplay, -2);
			if (*FPSlocked_shared != 30) {
				FPStiming = (19200000/(*FPSlocked_shared)) - 7800;
			}
			else FPStiming = 0;
		}
		else {
			eglInterval(EGLDisplay, -1);
			if (*FPSlocked_shared != 60) {
				FPStiming = (19200000/(*FPSlocked_shared)) - 7800;
			}
			else FPStiming = 0;
		}
		if (changedFPS) {
			FPSlock = *FPSlocked_shared;
		}
	}

	return result;
}

void nvnSetPresentInterval(void* nvnWindow, int mode) {
	if (!changeFPS) {
		((nvnSetPresentInterval_0)(ptr_nvnWindowSetPresentInterval))(nvnWindow, mode);
		changedFPS = false;
		*FPSmode_shared = mode;
	}
	else if (mode < 0) {
		mode *= -1;
		if (*FPSmode_shared != mode) {
			((nvnSetPresentInterval_0)(ptr_nvnWindowSetPresentInterval))(nvnWindow, mode);
			*FPSmode_shared = mode;
		}
		changedFPS = true;
	}
	return;
}

void* nvnSyncWait0(void* _this, uint64_t timeout_ns) {
	if (*ZeroSync_shared) timeout_ns = 0;
	return ((nvnSyncWait_0)(ptr_nvnSyncWait))(_this, timeout_ns);
}

void nvnPresentTexture(void* _this, void* nvnWindow, void* unk3) {
	static uint8_t FPS_temp = 0;
	static uint64_t starttick = 0;
	static uint64_t endtick = 0;
	static uint64_t deltatick = 0;
	static uint64_t frameend = 0;
	static uint64_t framedelta = 0;
	static uint64_t frameavg = 0;
	static uint8_t FPSlock = 0;
	static uint32_t FPStiming = 0;

	if (!starttick) {
		starttick = _ZN2nn2os13GetSystemTickEv();
		*FPSmode_shared = ((nvnGetPresentInterval_0)(ptr_nvnWindowGetPresentInterval))(nvnWindow);
	}
	if (FPStiming && !LOCK::blockDelayFPS) {
		while ((_ZN2nn2os13GetSystemTickEv() - frameend) < FPStiming) {
			svcSleepThread(100000);
		}
	}
	
	((nvnQueuePresentTexture_0)(ptr_nvnQueuePresentTexture))(_this, nvnWindow, unk3);
	endtick = _ZN2nn2os13GetSystemTickEv();
	framedelta = endtick - frameend;
	frameavg = ((9*frameavg) + framedelta) / 10;
	FPSavg = systemtickfrequency / (float)frameavg;
	frameend = endtick;
	
	FPS_temp++;
	deltatick = endtick - starttick;
	if (deltatick >= 19200000) {
		starttick = _ZN2nn2os13GetSystemTickEv();
		FPS = FPS_temp - 1;
		FPS_temp = 0;
		*FPS_shared = FPS;
		uint8_t FPSmode_old = *FPSmode_shared;
		*FPSmode_shared = ((nvnGetPresentInterval_0)(ptr_nvnWindowGetPresentInterval))(nvnWindow);
		if (*FPSmode_shared != FPSmode_old && *FPSlocked_shared) {
			if (*FPSmode_shared == 2) {
				FPSlock = 30;
			}
			else FPSlock = 60;
		}
		if (changeFPS && !configRC && FPSlock) {
			LOCK::applyPatch(configBuffer, configSize, FPSlock);
			*patchApplied_shared = true;
		}
	}

	*FPSavg_shared = FPSavg;
	*pluginActive = true;

	if (FPSlock != *FPSlocked_shared) {
		changeFPS = true;
		changedFPS = false;
		if (*FPSlocked_shared == 0) {
			FPStiming = 0;
			changeFPS = false;
			FPSlock = *FPSlocked_shared;
		}
		else if (*FPSlocked_shared <= 30) {
			nvnSetPresentInterval(nvnWindow, -2);
			if (*FPSlocked_shared != 30) {
				FPStiming = (19200000/(*FPSlocked_shared)) - 7800;
			}
			else FPStiming = 0;
		}
		else {
			nvnSetPresentInterval(nvnWindow, -1);
			if (*FPSlocked_shared != 60) {
				FPStiming = (19200000/(*FPSlocked_shared)) - 7800;
			}
			else FPStiming = 0;
		}
		if (changedFPS) {
			FPSlock = *FPSlocked_shared;
		}
	}
	
	return;
}

uintptr_t nvnGetProcAddress (void* unk1, const char* nvnFunction) {
	uintptr_t address = ((GetProcAddress)(ptr_nvnDeviceGetProcAddress))(unk1, nvnFunction);
	if (!strcmp("nvnDeviceGetProcAddress", nvnFunction))
		return addr_nvnGetProcAddress;
	else if (!strcmp("nvnQueuePresentTexture", nvnFunction)) {
		ptr_nvnQueuePresentTexture = address;
		return addr_nvnPresentTexture;
	}
	else if (!strcmp("nvnWindowSetPresentInterval", nvnFunction)) {
		ptr_nvnWindowSetPresentInterval = address;
		return addr_nvnSetPresentInterval;
	}
	else if (!strcmp("nvnWindowGetPresentInterval", nvnFunction)) {
		ptr_nvnWindowGetPresentInterval = address;
		return address;
	}
	else if (!strcmp("nvnSyncWait", nvnFunction)) {
		ptr_nvnSyncWait = address;
		return addr_nvnSyncWait;
	}
	else return address;
}

uintptr_t nvnBootstrapLoader_1(const char* nvnName) {
	if (strcmp(nvnName, "nvnDeviceGetProcAddress") == 0) {
		*API_shared = 1;
		ptr_nvnDeviceGetProcAddress = nvnBootstrapLoader("nvnDeviceGetProcAddress");
		return addr_nvnGetProcAddress;
	}
	uintptr_t ptrret = nvnBootstrapLoader(nvnName);
	return ptrret;
}

int main(int argc, char *argv[]) {
	SaltySDCore_printf("NX-FPS: alive\n");
	LOCK::mappings.main_start = getMainAddress();
	SaltySDCore_printf("NX-FPS: found main at: 0x%lX\n", LOCK::mappings.main_start);
	Result ret = SaltySD_CheckIfSharedMemoryAvailable(&SharedMemoryOffset, 15);
	SaltySDCore_printf("NX-FPS: ret: 0x%X\n", ret);
	if (!ret) {
		SaltySDCore_printf("NX-FPS: MemoryOffset: %d\n", SharedMemoryOffset);
		SaltySD_GetSharedMemoryHandle(&remoteSharedMemory);
		shmemLoadRemote(&_sharedmemory, remoteSharedMemory, 0x1000, Perm_Rw);
		ret = shmemMap(&_sharedmemory);
		if (R_SUCCEEDED(ret)) {
			uintptr_t base = (uintptr_t)shmemGetAddr(&_sharedmemory) + SharedMemoryOffset;
			uint32_t* MAGIC = (uint32_t*)base;
			*MAGIC = 0x465053;
			FPS_shared = (uint8_t*)(base + 4);
			FPSavg_shared = (float*)(base + 5);
			pluginActive = (bool*)(base + 9);
			
			addr_nvnGetProcAddress = (uint64_t)&nvnGetProcAddress;
			addr_nvnPresentTexture = (uint64_t)&nvnPresentTexture;
			SaltySDCore_ReplaceImport("nvnBootstrapLoader", (void*)nvnBootstrapLoader_1);
			SaltySDCore_ReplaceImport("eglSwapBuffers", (void*)eglSwap);
			SaltySDCore_ReplaceImport("eglSwapInterval", (void*)eglInterval);
			SaltySDCore_ReplaceImport("vkQueuePresentKHR", (void*)vulkanSwap);

			FPSlocked_shared = (uint8_t*)(base + 10);
			FPSmode_shared = (uint8_t*)(base + 11);
			ZeroSync_shared = (bool*)(base + 12);
			patchApplied_shared = (bool*)(base + 13);
			API_shared = (uint8_t*)(base + 14);
			addr_nvnSetPresentInterval = (uint64_t)&nvnSetPresentInterval;
			addr_nvnSyncWait = (uint64_t)&nvnSyncWait0;

			char titleid[17];
			CheckTitleID(&titleid[0]);
			char path[128];
			strcpy(&path[0], "sdmc:/SaltySD/plugins/FPSLocker/0");
			strcat(&path[0], &titleid[0]);
			strcat(&path[0], ".dat");
			FILE* file_dat = SaltySDCore_fopen(path, "rb");
			if (file_dat) {
				uint8_t temp = 0;
				SaltySDCore_fread(&temp, 1, 1, file_dat);
				*FPSlocked_shared = temp;
				SaltySDCore_fread(&temp, 1, 1, file_dat);
				*ZeroSync_shared = (bool)temp;
				SaltySDCore_fclose(file_dat);
			}

			u64 buildid = SaltySD_GetBID();
			if (!buildid) {
				SaltySDCore_printf("NX-FPS: getBID failed! Err: 0x%x\n", ret);
			}
			else {
				SaltySDCore_printf("NX-FPS: BID: %016lX\n", buildid);
				createBuildidPath(buildid, &titleid[0], &path[0]);
				FILE* patch_file = SaltySDCore_fopen(path, "rb");
				if (patch_file) {
					SaltySDCore_fclose(patch_file);
					SaltySDCore_printf("NX-FPS: FPSLocker: successfully opened: %s\n", path);
					configRC = readConfig(path, &configBuffer);
					SaltySDCore_printf("NX-FPS: FPSLocker: readConfig rc: %d\n", configRC);
					svcGetInfo(&LOCK::mappings.alias_start, InfoType_AliasRegionAddress, CUR_PROCESS_HANDLE, 0);
					svcGetInfo(&LOCK::mappings.heap_start, InfoType_HeapRegionAddress, CUR_PROCESS_HANDLE, 0);
				}
				else SaltySDCore_printf("NX-FPS: FPSLocker: File not found: %s\n", path);
			}
		}
		else {
			SaltySDCore_printf("NX-FPS: shmemMap failed! Err: 0x%x\n", ret);
		}
	}
	SaltySDCore_printf("NX-FPS: injection finished\n");
}
