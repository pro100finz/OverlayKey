#include <windows.h>
#include <stdio.h>

HANDLE Out = 0;
template<typename... Args>
void Print(const char* Format, Args... args)
{
	if (!Out)
	{
		AllocConsole();
		Out = GetStdHandle(STD_OUTPUT_HANDLE);
	}

	char Buff[1028];
	sprintf_s(Buff, Format, args...);
	int StrLengthOut = strlen(Buff);
	WriteConsoleA(Out, Buff, StrLengthOut, NULL, NULL);
}

DWORD GetModuleSize(LPVOID BaseAdress)
{
	IMAGE_DOS_HEADER* DosHeader = (IMAGE_DOS_HEADER*)(BaseAdress);
	IMAGE_NT_HEADERS* NtHeader = (IMAGE_NT_HEADERS*)(DosHeader->e_lfanew + (BYTE*)DosHeader);
	return NtHeader->OptionalHeader.SizeOfImage;
}

byte bCompare(const BYTE* pData, const BYTE* bMask, const char* szMask)
{
	for (; *szMask; ++szMask, ++pData, ++bMask)
		if (*szMask == 'x' && *pData != *bMask)
			return false;
	return (*szMask) == 0;
}

DWORD_PTR FindPattern(DWORD_PTR dwAddress, DWORD dwLen, BYTE* bMask, const char* szMask)
{
	for (DWORD_PTR i = 0; i < dwLen; i++)
		if (bCompare((BYTE*)(dwAddress + i), bMask, szMask))
			return (DWORD_PTR)(dwAddress + i);

	return 0;
}

#define RVA(Instr, InstrSize) ((DWORD64)Instr + InstrSize + *(LONG*)((DWORD64)Instr + (InstrSize - sizeof(LONG))))


class SteamOverlayInputHandler
{
	private:

	bool bInit = 1;
	DWORD_PTR adrKeyArray = 0;
	DWORD_PTR fnIsKeyDown = 0;

	typedef DWORD_PTR(__fastcall* oIsKeyPressed)(DWORD_PTR KeyArray, int* KeyId);
	oIsKeyPressed IsKeyPressed = 0;

	bool IsKeyDownImp(DWORD_PTR KeyArray, USHORT Key)
	{
		int iKey = Key;
		return *(byte*)IsKeyPressed(KeyArray, &iKey);
	}

	enum MouseKeys
	{
		LBM,
		MBM,
		LRM,
		X1M,
		X2M,
	};

	public:

	byte Init()
	{
		DWORD_PTR gor = (DWORD_PTR)GetModuleHandleW(L"gameoverlayrenderer64.dll");
		if (!gor)return 1;

		DWORD gor_size = GetModuleSize((LPVOID)gor);

		//48 8D 0D ? ? ? ? E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? B0 01
		DWORD_PTR Val = FindPattern(gor, gor_size, (PBYTE)"\x48\x8D\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x84\xC0\x0F\x84\x00\x00\x00\x00\xB0\x01", "xxx????x????xxxx????xx");
		if (!Val) return 2;

		adrKeyArray = RVA(Val, 7);
		
		//E8 ? ? ? ? 48 85 FF 74 05
		Val = FindPattern(gor, gor_size, (PBYTE)"\xE8\x00\x00\x00\x00\x48\x85\xFF\x74\x05","x????xxxxx");
		if (!Val) return 3;

		fnIsKeyDown = RVA(Val, 5);

		IsKeyPressed = (oIsKeyPressed)(fnIsKeyDown);

		bInit = 0;
		return 0;
	}

	bool IsKeyDown(USHORT key)
	{
		if (!bInit)
		{
			if (key < VK_BACK)
			{
				USHORT sKey = 0;
				switch (key)
				{
				case VK_LBUTTON:
					sKey = LBM;
					break;
				case VK_MBUTTON:
					sKey = MBM;
					break;
				case VK_RBUTTON:
					sKey = LRM;
					break;
				case VK_XBUTTON1:
					sKey = X1M;
					break;
				case VK_XBUTTON2:
					sKey = X2M;
					break;
				default:
					break;
				}

				//Mouse
				return IsKeyDownImp(adrKeyArray + 0x20, sKey); //+ 0x20 = MouseKeysArray
			}
			else
			{
				//Keybard
				return IsKeyDownImp(adrKeyArray, key);
			}
		}

		return 0;
	}
}SteamInput;

void ThreadCode()
{
	byte res = SteamInput.Init();
	Print("SteamInput res = %d\n", res);

	while (1)
	{
		if (SteamInput.IsKeyDown(VK_XBUTTON1))
		{
			Print("KeyDown VK_XBUTTON1\n");
		}

		if (SteamInput.IsKeyDown(VK_SHIFT))
		{
			Print("KeyDown VK_SHIFT\n");
		}
	}
}

int WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		CreateThread(0, 0,(LPTHREAD_START_ROUTINE)ThreadCode, 0, 0, 0);
		return 1;
	}
	return 0;
}