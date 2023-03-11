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
	extern void eglSwapBuffers(void* egl_unk1, void* egl_unk2) LINKABLE;
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
Result configRC = 0;


Result readConfig(const char* path, uint8_t** output_buffer) {
	FILE* patch_file = SaltySDCore_fopen(path, "rb");
	SaltySDCore_fseek(patch_file, 0, 2);
	configSize = SaltySDCore_ftell(patch_file);
	SaltySDCore_fclose(patch_file);
	uint8_t* buffer = (uint8_t*)calloc(1, configSize);
	patch_file = SaltySDCore_fopen(path, "r");
	SaltySDCore_fread(buffer, configSize, 1, patch_file);
	SaltySDCore_fclose(patch_file);
	if (LOCK::isValid(buffer, configSize)) {
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
void* nvnWindow = 0;
bool* ZeroSync_shared = 0;
bool changeFPS = false;
bool changedFPS = false;
bool FPSmode = 0;
uintptr_t addr_nvnSetPresentInterval;
uintptr_t addr_nvnBuilderSetPresentInterval;
uintptr_t addr_nvnAcquireTexture;
uintptr_t addr_nvnSyncWait;
uintptr_t ptr_nvnWindowSetPresentInterval;
uintptr_t ptr_nvnWindowBuilderSetPresentInterval;
uintptr_t ptr_nvnWindowAcquireTexture;
uintptr_t ptr_nvnSyncWait;
typedef void (*nvnSetPresentInterval_0)(void* _this, int mode);
typedef void (*nvnBuilderSetPresentInterval_0)(void* _this, int mode);
typedef void* (*nvnAcquireTexture_0)(void* _this, void* x1, int w2);
typedef void* (*nvnSyncWait_0)(void* _this, uint64_t timeout_ns);

inline void createBuildidPath(uint64_t buildid, char* titleid, char* buffer) {
	strcpy(buffer, "sdmc:/SaltySD/plugins/FPSLocker/patches/0");
	strcat(buffer, &titleid[0]);
	strcat(buffer, "/");
	ltoa(buildid, &titleid[0], 16);
	if (strlen(&titleid[0]) < 16) {
		size_t length = strlen(&titleid[0]);
		int zero_count = 15 - length;
		char* temp = (char*)calloc(1, 0x11);
		strcpy(&temp[zero_count], &titleid[0]);
		for (int i = 0; i < (zero_count + 1); i++) {
			temp[i] = 0x30;
		}
		strncpy(&titleid[0], temp, 16);
		free(temp);
	}
	strcat(buffer, &titleid[0]);
	strcat(buffer, ".bin");	
}

inline void CheckTitleID(char* buffer) {
    uint64_t titid = 0;
    svcGetInfo(&titid, 18, CUR_PROCESS_HANDLE, 0);	
    ltoa(titid, buffer, 16);
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
	
	if (starttick == 0) starttick = _ZN2nn2os13GetSystemTickEv();
	if (FPStiming) {
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

void eglSwap (void* egl_unk1_1, void* egl_unk2_1) {
	static uint8_t FPS_temp = 0;
	static uint64_t starttick = 0;
	static uint64_t endtick = 0;
	static uint64_t deltatick = 0;
	static uint64_t frameend = 0;
	static uint64_t framedelta = 0;
	static uint64_t frameavg = 0;
	static uint8_t FPSlock = 0;
	static uint32_t FPStiming = 0;

	if (!starttick) starttick = _ZN2nn2os13GetSystemTickEv();
	if (FPStiming) {
		while ((_ZN2nn2os13GetSystemTickEv() - frameend) < FPStiming) {
			svcSleepThread(100000);
		}
	}
	
	eglSwapBuffers(egl_unk1_1, egl_unk2_1);
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

	return;
}

void nvnBuilderSetPresentInterval(void* _this, int mode) {
	*FPSmode_shared = mode;
	((nvnBuilderSetPresentInterval_0)(ptr_nvnWindowBuilderSetPresentInterval))(_this, mode);
}

void nvnSetPresentInterval(void* _this, int mode) {
	if (!changeFPS) {
		((nvnSetPresentInterval_0)(ptr_nvnWindowSetPresentInterval))(_this, mode);
		changedFPS = false;
		*FPSmode_shared = mode;
	}
	else if (nvnWindow && !_this) {
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

void* nvnAcquireTexture(void* _this, void* x1, int w2) {
	nvnWindow = _this;
	return ((nvnAcquireTexture_0)(ptr_nvnWindowAcquireTexture))(_this, x1, w2);
}

void nvnPresentTexture(void* _this, void* unk2, void* unk3) {
	static uint8_t FPS_temp = 0;
	static uint64_t starttick = 0;
	static uint64_t endtick = 0;
	static uint64_t deltatick = 0;
	static uint64_t frameend = 0;
	static uint64_t framedelta = 0;
	static uint64_t frameavg = 0;
	static uint8_t FPSlock = 0;
	static uint32_t FPStiming = 0;

	if (!starttick) starttick = _ZN2nn2os13GetSystemTickEv();
	if (FPStiming) {
		while ((_ZN2nn2os13GetSystemTickEv() - frameend) < FPStiming) {
			svcSleepThread(100000);
		}
	}
	
	((nvnQueuePresentTexture_0)(ptr_nvnQueuePresentTexture))(_this, unk2, unk3);
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
		if (changeFPS && !configRC && FPSlock)
			LOCK::applyPatch(configBuffer, configSize, FPSlock);
	}

	*FPSavg_shared = FPSavg;
	*pluginActive = true;

	if (nvnWindow && FPSlock != *FPSlocked_shared) {
		changeFPS = true;
		changedFPS = false;
		if (*FPSlocked_shared == 0) {
			FPStiming = 0;
			changeFPS = false;
			FPSlock = *FPSlocked_shared;
		}
		else if (*FPSlocked_shared <= 30) {
			nvnSetPresentInterval(nullptr, 2);
			if (*FPSlocked_shared != 30) {
				FPStiming = (19200000/(*FPSlocked_shared)) - 7800;
			}
			else FPStiming = 0;
		}
		else {
			nvnSetPresentInterval(nullptr, 1);
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
	else if (!strcmp("nvnWindowBuilderSetPresentInterval", nvnFunction)) {
		ptr_nvnWindowBuilderSetPresentInterval = address;
		return addr_nvnBuilderSetPresentInterval;
	}
	else if (!strcmp("nvnWindowAcquireTexture", nvnFunction)) {
		ptr_nvnWindowAcquireTexture = address;
		return addr_nvnAcquireTexture;
	}
	else if (!strcmp("nvnSyncWait", nvnFunction)) {
		ptr_nvnSyncWait = address;
		return addr_nvnSyncWait;
	}
	else return address;
}

uintptr_t nvnBootstrapLoader_1(const char* nvnName) {
	if (strcmp(nvnName, "nvnDeviceGetProcAddress") == 0) {
		ptr_nvnDeviceGetProcAddress = nvnBootstrapLoader("nvnDeviceGetProcAddress");
		return addr_nvnGetProcAddress;
	}
	uintptr_t ptrret = nvnBootstrapLoader(nvnName);
	return ptrret;
}

int main(int argc, char *argv[]) {
	SaltySDCore_printf("NX-FPS: alive\n");
	LOCK::main_address = SaltySDCore_getCodeStart() + 0x4000;
	Result ret = SaltySD_CheckIfSharedMemoryAvailable(&SharedMemoryOffset, 14);
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
			SaltySDCore_ReplaceImport("vkQueuePresentKHR", (void*)vulkanSwap);

			FPSlocked_shared = (uint8_t*)(base + 10);
			FPSmode_shared = (uint8_t*)(base + 11);
			ZeroSync_shared = (bool*)(base + 12);
			LOCK::unsafeCheck = (bool*)(base + 13);
			addr_nvnBuilderSetPresentInterval = (uint64_t)&nvnBuilderSetPresentInterval;
			addr_nvnSetPresentInterval = (uint64_t)&nvnSetPresentInterval;
			addr_nvnAcquireTexture = (uint64_t)&nvnAcquireTexture;
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
				FILE* patch_file = SaltySDCore_fopen(path, "r");
				if (patch_file) {
					SaltySDCore_fclose(patch_file);
					SaltySDCore_printf("NX-FPS: FPSLocker: successfully opened: %s\n", path);
					Result configRC = readConfig(path, &configBuffer);
					SaltySDCore_printf("NX-FPS: FPSLocker: readConfig rc: %d\n", configRC);
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
