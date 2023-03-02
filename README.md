# NX-FPS

SaltyNX plugin that collects FPS data in Nintendo Switch games. You need my fork of SaltyNX installed.
https://github.com/masagrator/SaltyNX/releases

Put NX-FPS.elf to `/SaltySD/plugins`

Currently supported graphics APIs:
- NVN
- EGL
- Vulkan

To read FPS from plugin you can use [Status Monitor Overlay 0.8.1+](https://github.com/masagrator/Status-Monitor-Overlay) or write your own code.

I am providing instructions based on Status Monitor Overlay code how to read it from your own homebrew:
- To homebrew you need to include old ipc.h ([here](https://github.com/masagrator/ReverseNX-RT/blob/master/Overlay/include/ipc.h)) and SaltyNX.h ([here](https://github.com/masagrator/ReverseNX-RT/blob/master/Overlay/include/SaltyNX.h)).
- We need to connect to SaltyNX SharedMemory by connecting to SaltyNX, retrieving SharedMemory handle, terminating connection with SaltyNX and assigning SharedMemory to homebrew.

Helper:
```cpp
Handle remoteSharedMemory = 1;
SharedMemory _sharedmemory = {};
bool SharedMemoryUsed = false;

//Function pinging SaltyNX InjectServ port to check if it's alive.
bool CheckPort () {
	Handle saltysd;
	for (int i = 0; i < 67; i++) {
		if (R_SUCCEEDED(svcConnectToNamedPort(&saltysd, "InjectServ"))) {
			svcCloseHandle(saltysd);
			break;
		}
		else {
			if (i == 66) return false;
			svcSleepThread(1'000'000);
		}
	}
	for (int i = 0; i < 67; i++) {
		if (R_SUCCEEDED(svcConnectToNamedPort(&saltysd, "InjectServ"))) {
			svcCloseHandle(saltysd);
			return true;
		}
		else svcSleepThread(1'000'000);
	}
	return false;
}

bool LoadSharedMemory() {
    //Connect to main SaltyNX port. On failed attempt return false
    if (SaltySD_Connect())
      return false;
    //Retrieve handle necessary to get access to SaltyNX SharedMemory
    SaltySD_GetSharedMemoryHandle(&remoteSharedMemory);
    //Terminate SaltyNX main port connection
    SaltySD_Term();
    
    //Prepare struct that will be passed to shmemMap to get access to SaltyNX SharedMemory
    shmemLoadRemote(&_sharedmemory, remoteSharedMemory, 0x1000, Perm_Rw);
    //Try to get access to SaltyNX SharedMemory
    if (!shmemMap(&_sharedmemory)) {
      //On success change bool of SharedMemoryUsed to true and return true
      SharedMemoryUsed = true;
      return true;
    }
    //On failed attemp return false
    return false;
}

//By connecting to InjectServ port we can check if SaltyNX is alive and is not stuck anywhere.
bool SaltyNX = CheckPort();
//Check if SaltyNX signaled anything, if yes then use LoadSharedMemory() to get access to SaltyNX SharedMemory
if (SaltyNX) LoadSharedMemory();
```

- Next we need to find where our plugin is inside SharedMemory. For this our plugin stores magic 0x465053 as uint32_t. We need to find magic and based on that we know where our plugin is.

```cpp

uint32_t* MAGIC_shared = 0;
uint8_t* FPS_shared = 0;
float* FPSavg_shared = 0;
bool* pluginActive = 0;

//Function searching for NX-FPS magic through SharedMemory.
//SaltyNX is page aligning any SharedMemory reservation request to 4, that's why we check every 4th byte for MAGIC.
ptrdiff_t searchSharedMemoryBlock(uintptr_t base) {
	ptrdiff_t search_offset = 0;
	while(search_offset < 0x1000) {
		MAGIC_shared = (uint32_t*)(base + search_offset);
		if (*MAGIC_shared == 0x465053) {
			return search_offset;
		}
		else search_offset += 4;
	}
	return -1;
}

//Get virtual address of SaltyNX SharedMemory
uintptr_t base = (uintptr_t)shmemGetAddr(&_sharedmemory);
//Pass retrieved virtual address of SaltyNX SharedMemory to function that will search for NX-FPS magic
ptrdiff_t rel_offset = searchSharedMemoryBlock(base);
//If magic will be found, you will get offset that starts before magic. It cannot be lower than 0.
if (rel_offset > -1) {
  //Pass correct addresses inside SharedMemory to our pointers
  //It shows how many frames passed in one second
  FPS_shared = (uint8_t*)(base + rel_offset + 4);
  //It calculates average FPS based on last 10 readings
  FPSavg_shared = (float*)(base + rel_offset + 5);
  //Pointer where plugin writes true for every frame.
  pluginActive = (bool*)(base + rel_offset + 9);
  ///By passing false to *pluginActive and checking later if it has changed to true we can be sure plugin is working.
  *pluginActive = false;
}
```

>WARNING

Plugin brings some instability to boot process for some games. It is recommended to not close game before ~10 seconds have passed from showing Nintendo logo, otherwise you risk Kernel panic, which results in crashing OS.

---

Not working games with this plugin (You can find games not compatible with SaltyNX [here](https://github.com/masagrator/SaltyNX/blob/master/README.md))
| Title | Version(s) | Why? |
| ------------- | ------------- | ------------- |
| Final Fantasy VIII Remastered | all | Framework stuff is included in NROs which SaltyNX doesn't support |
| Final Fantasy X/X-2 | all | Framework stuff is included in NROs which SaltyNX doesn't support |

# Troubleshooting
Q: Why I got constantly 255?

A: 255 is default value before plugin starts counting frames. This may be a sign that:
* Game is using different API or function than what is currently supported
* Plugin missed symbol when initializing (for whatever reason)

Try first to run game again few times. If it's still 255, make an issue and state name of game. Next updates will include support for other graphics APIs.

# Thanks to:

- RetroNX channel for help with coding stuff,
- CTCaer for providing many useful informations and convincing me to the end that I should target high precision,
- Herbaciarz for providing video footage.
