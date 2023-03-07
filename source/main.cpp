#include <switch_min.h>
#include "saltysd/SaltySD_ipc.h"
#include "saltysd/SaltySD_dynamic.h"
#include "saltysd/SaltySD_core.h"

extern "C" {
	extern u32 __start__;

	static char g_heap[0x8000];

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
bool changeFPS = false;
bool changedFPS = false;
bool FPSmode = 0;
uintptr_t addr_nvnSetPresentInterval;
uintptr_t addr_nvnAcquireTexture;
uintptr_t ptr_nvnWindowSetPresentInterval;
uintptr_t ptr_nvnWindowAcquireTexture;
typedef void (*nvnSetPresentInterval_0)(void* _this, int mode);
typedef void* (*nvnAcquireTexture_0)(void* _this, void* x1, int w2);

uint32_t vulkanSwap (void* vk_unk1_1, void* vk_unk2_1) {
	static uint8_t FPS_temp = 0;
	static uint64_t starttick = 0;
	static uint64_t endtick = 0;
	static uint64_t deltatick = 0;
	static uint64_t frameend = 0;
	static uint64_t framedelta = 0;
	static uint64_t frameavg = 0;
	
	if (starttick == 0) starttick = _ZN2nn2os13GetSystemTickEv();
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
	
	if (starttick == 0) starttick = _ZN2nn2os13GetSystemTickEv();
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

	return;
}

void nvnSetPresentInterval(void* _this, int mode) {
	if (!changeFPS) {
		((nvnSetPresentInterval_0)(ptr_nvnWindowSetPresentInterval))(_this, mode);
		changedFPS = false;
	}
	else if (nvnWindow && !_this) {
		((nvnSetPresentInterval_0)(ptr_nvnWindowSetPresentInterval))(nvnWindow, mode);
		changedFPS = true;
	}
	return;
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
		while ((_ZN2nn2os13GetSystemTickEv() - frameend) < (FPStiming-7800)) {
			svcSleepThread(100000);
		}
		((nvnQueuePresentTexture_0)(ptr_nvnQueuePresentTexture))(_this, unk2, unk3);
	}
	else ((nvnQueuePresentTexture_0)(ptr_nvnQueuePresentTexture))(_this, unk2, unk3);
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

	if (nvnWindow && FPSlock != *FPSlocked_shared) {
		changeFPS = true;
		changedFPS = false;
		if (*FPSlocked_shared == 30) {
			nvnSetPresentInterval(nullptr, 2);
			FPStiming = 0;
		}
		else {
			nvnSetPresentInterval(nullptr, 1);
			if (*FPSlocked_shared != 60) {
				FPStiming = 19200000/(*FPSlocked_shared);
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
	if (strcmp("nvnDeviceGetProcAddress", nvnFunction) == 0)
		return addr_nvnGetProcAddress;
	else if (strcmp("nvnQueuePresentTexture", nvnFunction) == 0) {
		ptr_nvnQueuePresentTexture = address;
		return addr_nvnPresentTexture;
	}
	else if (strcmp("nvnWindowSetPresentInterval", nvnFunction) == 0) {
		ptr_nvnWindowSetPresentInterval = address;
		return addr_nvnSetPresentInterval;
	}
	else if (strcmp("nvnWindowAcquireTexture", nvnFunction) == 0) {
		ptr_nvnWindowAcquireTexture = address;
		return addr_nvnAcquireTexture;
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
	Result ret = SaltySD_CheckIfSharedMemoryAvailable(&SharedMemoryOffset, 16);
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

			MAGIC = (uint32_t*)(base + 10);
			*MAGIC = 0x4B434F4C;
			FPSlocked_shared = (uint8_t*)(base + 14);
			FPSmode_shared = (uint8_t*)(base + 15);
			*FPSlocked_shared = 45;
			addr_nvnSetPresentInterval = (uint64_t)&nvnSetPresentInterval;
			addr_nvnAcquireTexture = (uint64_t)&nvnAcquireTexture;
		}
		else {
			SaltySDCore_printf("NX-FPS: shmemMap failed! Err: 0x%x\n", ret);
		}
	}
	SaltySDCore_printf("NX-FPS: injection finished\n");
}
