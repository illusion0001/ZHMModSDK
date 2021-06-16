#include "Hooks.h"
#include "HookImpl.h"

#include <Glacier/ZEntity.h>

std::unordered_set<HookBase*>* HookRegistry::g_Hooks = nullptr;

DetourTrampoline* Trampolines::g_Trampolines = nullptr;
size_t Trampolines::g_TrampolineCount = 0;

PATTERN_HOOK(
	"\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x41\x56\x48\x83\xEC\x00\x48\x8B\xF9\xE8\x00\x00\x00\x00\x45\x33\xF6",
	"xxxxxxxxxxxxxxxxxxxxxxxxx?xxxx????xxx",
	ZActor_ZActor, void(ZActor* th, ZComponentCreateInfo* createInfo)
);

PATTERN_HOOK(
	"\x48\x8B\xC4\x48\x89\x48\x08\x55\x56\x48\x8D\xA8\x68\xFF\xFF\xFF",
	"xxxxxxxxxxxxxxxx",
	ZEntitySceneContext_LoadScene, void(ZEntitySceneContext* th, ZSceneData& sceneData)
);

PATTERN_HOOK(
	"\x48\x89\x5C\x24\x10\x48\x89\x6C\x24\x18\x56\x57\x41\x56\x48\x83\xEC\x00\x41\x0F\xB6\xE9",
	"xxxxxxxxxxxxxxxxx?xxxx",
	ZUpdateEventContainer_AddDelegate, void(ZUpdateEventContainer* th, const ZDelegate<void(const SGameUpdateEvent&)>&, int, EUpdateMode)
);

PATTERN_HOOK(
	"\x48\x89\x5C\x24\x10\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x83\xEC\x00\x48\x8D\x79\x50",
	"xxxxxxxxxxxxxxxxxxx?xxxx",
	ZUpdateEventContainer_RemoveDelegate, void(ZUpdateEventContainer* th, const ZDelegate<void(const SGameUpdateEvent&)>&, int, EUpdateMode)
);

PATTERN_HOOK(
	"\x48\x89\x54\x24\x10\x55\x53\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xAC\x24\x68\xFB\xFF\xFF",
	"xxxxxxxxxxxxxxxxxxxxxxxxx",
	Engine_Init, bool(void*, void*)
);

PATTERN_HOOK(
	"\x41\x56\x48\x83\xEC\x00\x48\x89\x6C\x24\x48\x8B\xEA",
	"xxxxx?xxxxxxx",
	ZKnowledge_SetGameTension, void(ZKnowledge*, EGameTension)
);

PATTERN_HOOK(
	"\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x00\x0F\xB6\xF2\x48\x8B\xD9\xE8",
	"xxxxxxxxxxxxxx?xxxxxxx",
	GetApplicationOptionBool, bool(const ZString&, bool)
);

PATTERN_HOOK(
	"\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x18\x48\x89\x74\x24\x20\x57\x48\x83\xEC\x00\x0F\xB6\xF2",
	"xxxxxxxxxxxxxxxxxxx?xxx",
	ZApplicationEngineWin32_OnMainWindowActivated, void(ZApplicationEngineWin32*, bool)
);

PATTERN_HOOK(
	"\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x48\x89\x7C\x24\x20\x55\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\x6C\x24\xD1",
	"xxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
	ZApplicationEngineWin32_MainWindowProc, LRESULT(ZApplicationEngineWin32*, HWND, UINT, WPARAM, LPARAM)
);

PATTERN_HOOK(
	"\x40\x55\x56\x41\x54\x41\x56\x41\x57\x48\x83\xEC\x00\x4C\x8B\xF1",
	"xxxxxxxxxxxx?xxx",
	SetPropertyValue, bool(ZEntityRef, uint32_t, const ZObjectRef&, bool)
);

PATTERN_HOOK(
	"\x48\x89\x6C\x24\x18\x56\x57\x41\x56\x48\x83\xEC\x00\x4C\x8B\xF1\x48\xC7\x44\x24\x60",
	"xxxxxxxxxxxx?xxxxxxxx",
	SignalOutputPin, bool(ZEntityRef, uint32_t, const ZObjectRef&)
);

PATTERN_HOOK(
	"\x48\x89\x6C\x24\x20\x56\x41\x56\x41\x57\x48\x83\xEC\x00\x48\x8B\x29",
	"xxxxxxxxxxxxx?xxx",
	SignalInputPin, bool(ZEntityRef, uint32_t, const ZObjectRef&)
);

PATTERN_RELATIVE_CALL_HOOK(
	"\xE8\x00\x00\x00\x00\xEB\x00\x49\x8B\xC5\x48\x8B\x1D\x00\x00\x00\x00\x48\x89\x86\x58\x41\x01\x00",
	"x????x?xxxxxx????xxxxxxx",
	ZRenderDevice_ZRenderDevice, ZRenderDevice* (ZRenderDevice* th)
);

MODULE_HOOK(
	"d3d12.dll", "D3D12CreateDevice",
	D3D12CreateDevice, HRESULT(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice)
);

PATTERN_HOOK(
	"\x44\x88\x44\x24\x18\x57\x41\x54",
	"xxxxxxxx",
	ZRenderSwapChain_Resize, void(ZRenderSwapChain* th, void* a2, bool a3)
);

PATTERN_HOOK(
	"\x48\x89\x54\x24\x10\x48\x89\x4C\x24\x08\x55\x53\x41\x57",
	"xxxxxxxxxxxxxx",
	Check_SSL_Cert, bool(void*, void*)
);

PATTERN_HOOK(
	"\x40\x53\x48\x83\xEC\x00\x41\xF7\x00\x00\x00\x00\x00\x48\x8D\x59\x08",
	"xxxxx?xxx????xxxx",
	ZApplicationEngineWin32_OnDebugInfo, void(ZApplicationEngineWin32* th, const ZString& info, const ZString& details)
);

PATTERN_HOOK(
	"\x40\x53\x41\x55\x48\x83\xEC\x00\x48\x83\xB9\xF8\x00\x00\x00",
	"xxxxxxx?xxxxxxx",
	ZKeyboardWindows_Update, void(ZKeyboardWindows* th, bool a2)
);

PATTERN_CALL_HOOK(
	"\xE8\x00\x00\x00\x00\x49\x8B\xCF\xE8\x00\x00\x00\x00\x49\x8B\x8D\xC0\x07\x00\x00",
	"x????xxxx????xxxxxxx",
	ZRenderContext_Unknown01, void(ZRenderContext* th)
);

PATTERN_HOOK(
	"\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x00\x89\x11",
	"xxxxxxxxxxxxxx?xx",
	ZPackageManagerPackage_ZPackageManagerPackage, void*(ZPackageManagerPackage* th, void* a2, const ZString& a3, int a4, int patchLevel)
);

PATTERN_HOOK(
	"\x40\x53\x55\x48\x83\xEC\x00\x48\x8D\x99\x20\x01\x00\x00",
	"xxxxxx?xxxxxxx",
	ZGameLoopManager_ReleasePause, void(ZGameLoopManager* th, const ZString& a2)
);

PATTERN_HOOK(
	"\x48\x89\x7C\x24\x20\x41\x55\x41\x56\x41\x57\x48\x83\xEC\x00\x48\x8D\x79\x18",
	"xxxxxxxxxxxxxx?xxxx",
	ZGameUIManagerEntity_TryOpenMenu, bool(ZGameUIManagerEntity* th, EGameUIMenu menu, bool force)
);

PATTERN_HOOK(
	"\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x83\xEC\x00\x48\x8B\xF9\x48\x81\xC1",
	"xxxxxxxxxxxxxxxxxxxxxxxxxxx?xxxxxx",
	ZGameStatsManager_SendAISignals01, void(ZGameStatsManager* th)
);

PATTERN_HOOK(
	"\x48\x89\x5C\x24\x10\x48\x89\x6C\x24\x18\x48\x89\x74\x24\x20\x57\x41\x54\x41\x57\x48\x83\xEC\x00\x48\x8B\xF9",
	"xxxxxxxxxxxxxxxxxxxxxxx?xxx",
	ZGameStatsManager_SendAISignals02, void(ZGameStatsManager* th)
);

PATTERN_HOOK(
	"\x48\x89\x5C\x24\x08\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xAC\x24\x20\xFC\xFF\xFF",
	"xxxxxxxxxxxxxxxxxxxxxxxx",
	ZAchievementManagerSimple_OnEventReceived, void(ZAchievementManagerSimple* th, const SOnlineEvent& event)
);

PATTERN_HOOK(
	"\x48\x8B\xC4\x55\x48\x8D\xA8\xE8\xFC\xFF\xFF",
	"xxxxxxxxxxx",
	ZAchievementManagerSimple_OnEventSent, void(ZAchievementManagerSimple* th, uint32_t eventIndex, const ZDynamicObject& event)
);
