#include <Windows.h>
#include <iostream>

#include <d3d9.h>
#include <dxgi.h>
#include <Psapi.h>
#include <thread>
#include <chrono>

typedef HRESULT(__stdcall* Present_t)(LPDIRECT3DDEVICE9, CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*);
Present_t OrigPresent;
HWND currentHwnd = nullptr;

WNDPROC originalWndProc = nullptr;
bool KeyListener[256] = {};
bool isCustomProcSet = false;
bool isFirstCustomProcSet = false;
bool wasInGame = false;


DWORD sfModule = (DWORD)GetModuleHandle(0);
DWORD GameBase_Address = sfModule + 0xC24F4C;

class CGameBase
{
public:

	char ___spacer00[0x829C];
	int32_t  Index; // 0x829C
	int32_t GameType; // 0x82A0
	int32_t Unk; //0x82A4
	int32_t GameCount; // 0x82A8
	int32_t AutoWinHost; // 0x82AC

	char ___spacer01[0x7D18];
	int32_t ForceHost;//0xFFC8

	char ___spacer02[0x5D];
	uint8_t Guerilla;//0x10029 


};

CGameBase* GetGameBase()
{
	return *reinterpret_cast<CGameBase**>(GameBase_Address);
}

bool InGame()
{

	CGameBase* gameBase = GetGameBase();
	if (!(gameBase))
		return false;

	DWORD result = *(DWORD*)((DWORD)gameBase + 0x10A44);
	if (!result)
		return false;

	return true;
}
DWORD FindPattern(std::string moduleName, std::string pattern)
{

#define INRANGE(x,a,b)    (x >= a && x <= b) 
#define getBits( x )    (INRANGE((x&(~0x20)),'A','F') ? ((x&(~0x20)) - 'A' + 0xa) : (INRANGE(x,'0','9') ? x - '0' : 0))
#define getByte( x )    (getBits(x[0]) << 4 | getBits(x[1]))

	const char* pat = pattern.c_str();
	DWORD firstMatch = 0;
	DWORD rangeStart = (DWORD)GetModuleHandleA(moduleName.c_str());
	MODULEINFO miModInfo; GetModuleInformation(GetCurrentProcess(), (HMODULE)rangeStart, &miModInfo, sizeof(MODULEINFO));
	DWORD rangeEnd = rangeStart + miModInfo.SizeOfImage;
	for (DWORD pCur = rangeStart; pCur < rangeEnd; pCur++)
	{
		if (!*pat)
			return firstMatch;

		if (*(PBYTE)pat == '\?' || *(BYTE*)pCur == getByte(pat))
		{
			if (!firstMatch)
				firstMatch = pCur;

			if (!pat[2])
				return firstMatch;

			if (*(PWORD)pat == '\?\?' || *(PBYTE)pat != '\?')
				pat += 3;

			else
				pat += 2;    //one ?
		}
		else
		{
			pat = pattern.c_str();
			firstMatch = 0;
		}
	}
	return NULL;
}
bool WaitForModuleLoad()
{
	HMODULE hModule = GetModuleHandleA("graphics-hook32.dll");
	while (hModule == NULL)
	{
		Sleep(500);
		hModule = GetModuleHandleA("graphics-hook32.dll");
	}
	return (hModule != NULL);
}
HWND GetDeviceWindow(IDirect3DDevice9* pDevice)
{
    HWND hwnd = nullptr;
    IDirect3DSwapChain9* pSwapChain = nullptr;
    D3DPRESENT_PARAMETERS d3dpp;
    if (SUCCEEDED(pDevice->GetSwapChain(0, &pSwapChain)))
    {
        if (SUCCEEDED(pSwapChain->GetPresentParameters(&d3dpp)))
        {
            hwnd = d3dpp.hDeviceWindow;
        }
        pSwapChain->Release();
    }

    return hwnd;
}
void ResetCustomWndProc()
{
	if (isCustomProcSet)
	{
		SetWindowLongPtr(currentHwnd, GWLP_WNDPROC, (LONG_PTR)originalWndProc);
		originalWndProc = nullptr;
		isCustomProcSet = false;
		printf("Reset custom WndProc.\n");
	}
}

BOOL SetCustomWndProc(LPDIRECT3DDEVICE9 pDevice, WNDPROC newWndProc)
{
    HWND hwnd = GetDeviceWindow(pDevice);

    if (!hwnd)
    {
        printf("Failed to get HWND.\n");
        return FALSE;
    }
    if (originalWndProc)
    {
        SetWindowLongPtr(currentHwnd, GWLP_WNDPROC, (LONG_PTR)originalWndProc);
        originalWndProc = nullptr;
        printf("Restored original WndProc.\n");
    }
    originalWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)newWndProc);
    if (!originalWndProc)
    {
        printf("Failed to set new WndProc.\n");
        return FALSE;
    }
    currentHwnd = hwnd;
	isCustomProcSet = true;
    printf("Set custom WndProc successfully.\n");
    return TRUE;
}
LRESULT __stdcall WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	printf("WndProc is called \n");
	switch (uMsg) 
	{
	case WM_LBUTTONDOWN:
		KeyListener[VK_LBUTTON] = true;
		break;
	case WM_LBUTTONUP:
		KeyListener[VK_LBUTTON] = false;
		break;
	case WM_RBUTTONDOWN:
		KeyListener[VK_RBUTTON] = true;
		break;
	case WM_RBUTTONUP:
		KeyListener[VK_RBUTTON] = false;
		break;
	case WM_MBUTTONDOWN:
		KeyListener[VK_MBUTTON] = true;
		break;
	case WM_MBUTTONUP:
		KeyListener[VK_MBUTTON] = false;
		break;
	case WM_XBUTTONDOWN:
	{
		if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
		{
			KeyListener[VK_XBUTTON1] = true;
		}
		else if (GET_XBUTTON_WPARAM(wParam) == XBUTTON2)
		{
			KeyListener[VK_XBUTTON2] = true;
		}
		break;
	}
	case WM_XBUTTONUP:
	{
		if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
		{
			KeyListener[VK_XBUTTON1] = false;
		}
		else if (GET_XBUTTON_WPARAM(wParam) == XBUTTON2)
		{
			KeyListener[VK_XBUTTON2] = false;
		}
		break;
	}
	case WM_KEYDOWN:
		KeyListener[wParam] = true;
		break;
	case WM_KEYUP:
		KeyListener[wParam] = false;
		break;
	default:
		break;
	}

	return CallWindowProcW((WNDPROC)originalWndProc, hWnd, uMsg, wParam, lParam); 
}
HRESULT __stdcall Hooked_OBSPresent(LPDIRECT3DDEVICE9 pDevice, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
	bool isInGame = InGame();
	if (!isFirstCustomProcSet && !isInGame)
	{
		ResetCustomWndProc();
		printf("First Setting custom WndProc for in-lobby.\n");
		if (!SetCustomWndProc(pDevice, WndProc))
		{
			printf("Failed to set InLobby WndProc.\n");
		}
		isFirstCustomProcSet = true; 
	}
	if (!wasInGame && isInGame) 
	{
		ResetCustomWndProc();
		printf("Setting custom WndProc for in-game.\n");
		if (!SetCustomWndProc(pDevice, WndProc))
		{
			printf("Failed to set InGame WndProc.\n");
		}
	}
	else if (wasInGame && !isInGame) 
	{
		ResetCustomWndProc();
		printf("Setting custom WndProc for in-lobby.\n");
		if (!SetCustomWndProc(pDevice, WndProc))
		{
			printf("Failed to set InLobby WndProc.\n");
		}
	}

	wasInGame = isInGame; // Update wasInGame for next iteration
	return OrigPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}
bool InitializeConsole()
{
	BOOL result = AllocConsole();
	if (!result)
		return result;

	FILE* fDummy;
	freopen_s(&fDummy, "CONIN$", "r", stdin);
	freopen_s(&fDummy, "CONOUT$", "w", stderr);
	freopen_s(&fDummy, "CONOUT$", "w", stdout);
	return true;
}
template <typename T>
T SwapPointer(uintptr_t Address, uintptr_t Func)
{
	if (!Address)
		return (T)false;

	auto tmp = *reinterpret_cast<uintptr_t*>(Address);
	*reinterpret_cast<uintptr_t*>(Address) = Func;

	return (T)(tmp);
}
void Start()
{
	if (!InitializeConsole())
		return;

	while (!WaitForModuleLoad())
	{
		Sleep(500);
		return;
	}
	DWORD renderOBSPresent = *(DWORD*)(FindPattern("graphics-hook32.dll", "A1 ? ? ? ? ? ? ? ? 75 ? FF 75 ? FF 75 ? FF 75 ? 57") + 1);
	while (!*reinterpret_cast<uintptr_t*>(renderOBSPresent))
	{
		Sleep(500);
	}
	OrigPresent = SwapPointer<Present_t>((uintptr_t)renderOBSPresent, (uintptr_t)&Hooked_OBSPresent);
}
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule); 
		CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(Start), NULL, NULL, NULL);
		break;

	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}