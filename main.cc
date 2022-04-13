#include <cstdio>
#include <string>
#include <Windows.h>
#include <TlHelp32.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

// Copied from: https://stackoverflow.com/questions/865152/how-can-i-get-a-process-handle-by-its-name-in-c
HANDLE open_process_by_name(PCSTR name)
{
	DWORD pid = 0;

	// Create toolhelp snapshot.
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 process;
	ZeroMemory(&process, sizeof(process));
	process.dwSize = sizeof(process);

	// Walkthrough all processes.
	if (Process32First(snapshot, &process)) {
		do {
			// Compare process.szExeFile based on format of name, i.e., trim file path
			// trim .exe if necessary, etc.
			if (std::string(process.szExeFile) == std::string(name)) {
				pid = process.th32ProcessID;
				break;
			}
		} while (Process32Next(snapshot, &process));
	}

	CloseHandle(snapshot);

	if (pid != 0) {
		return OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	}

	// Not found
	return INVALID_HANDLE_VALUE;
}

int main(int argc, char* argv[])
{
	// Initialize COM stuff.
	CoInitialize(nullptr);

	// Get a pointer to the interface that has the function we want to patch.
	IMMDeviceEnumerator* device_enumerator = nullptr;
	CoCreateInstance(
		__uuidof(MMDeviceEnumerator),
		nullptr,
		CLSCTX_INPROC_SERVER,
		__uuidof(IMMDeviceEnumerator),
		(LPVOID*) &device_enumerator
	);

	IMMDevice* default_device = nullptr;
	device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &default_device);

	device_enumerator->Release();
	device_enumerator = nullptr;

	IAudioEndpointVolume* endpoint_volume = nullptr;
	default_device->Activate(
		__uuidof(IAudioEndpointVolume),
		CLSCTX_INPROC_SERVER,
		nullptr,
		(LPVOID*) &endpoint_volume
	);

	default_device->Release();
	default_device = nullptr;

	// Use the pointer to gain access to the vtable and get a pointer to the code we want to patch.
	uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(endpoint_volume);

	const char* dll_name = "AudioSes.dll";

	uintptr_t local_base = (uintptr_t) GetModuleHandleA(dll_name);
	uintptr_t SetMasterVolumeLevel = vtable[0x18 / 4] - local_base;
	uintptr_t SetMasterVolumeLevelScalar = vtable[0x1C / 4] - local_base;

	endpoint_volume->Release();

	CoUninitialize();

	// Open TeamViewer.
	HANDLE remote_process = open_process_by_name("TeamViewer.exe");
	if (remote_process == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "TeamViewer.exe not running!\n");
		return 1;
	}

	// Ensure AudioSes.dll is loaded first as TeamViewer only loads it when touching audio.
	LPVOID remote_dll_name = VirtualAllocEx(remote_process, nullptr, strlen(dll_name) + 1, MEM_COMMIT, PAGE_READWRITE);
	WriteProcessMemory(remote_process, remote_dll_name, (LPVOID) dll_name, strlen(dll_name) + 1, 0);
	HANDLE remote_thread = CreateRemoteThread(remote_process, nullptr, 0, (LPTHREAD_START_ROUTINE) GetProcAddress(GetModuleHandleA("Kernel32.dll"), "LoadLibraryA"), remote_dll_name, 0, 0);
	WaitForSingleObject(remote_thread, INFINITE);
	VirtualFreeEx(remote_process, remote_dll_name, strlen(dll_name) + 1, MEM_RELEASE);

	// Get the result of LoadLibrary, which is the base of AudioSes.dll.
	DWORD remote_base;
	GetExitCodeThread(remote_thread, &remote_base);

	CloseHandle(remote_thread);

	// Writing the following at the start of the target functions:
	// xor eax, eax
	// retn 0xC
	unsigned char patch[] = {0x31, 0xC0, 0xC2, 0x0C, 0x00};

	DWORD unused;
	VirtualProtectEx(remote_process, (LPVOID) (remote_base + SetMasterVolumeLevel), 5, PAGE_EXECUTE_READWRITE, &unused);
	WriteProcessMemory(remote_process, (LPVOID) (remote_base + SetMasterVolumeLevel), patch, 5, &unused);
	VirtualProtectEx(remote_process, (LPVOID) (remote_base + SetMasterVolumeLevelScalar), 5, PAGE_EXECUTE_READWRITE, &unused);
	WriteProcessMemory(remote_process, (LPVOID) (remote_base + SetMasterVolumeLevelScalar), patch, 5, &unused);

	CloseHandle(remote_process);
}
