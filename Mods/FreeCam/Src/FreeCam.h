#pragma once

#include <unordered_map>

#include "IPluginInterface.h"

#include <Glacier/ZEntity.h>
#include <Glacier/ZInput.h>
#include <Glacier/ZCollision.h>

class FreeCam : public IPluginInterface
{
public:
    FreeCam();
    ~FreeCam() override;

    void Init() override;
    void OnEngineInitialized() override;
    void OnDrawMenu() override;
    void OnDrawUI(bool p_HasFocus) override;

private:
    struct CameraPoint
    {
        float4 rotation;
        float4 position;
        double duration;
    };
    void ResetCameraPoints();
    void loadCameraPoints(const std::wstring& filename);
    void UpdatePoints();
    void OnFrameUpdate(const SGameUpdateEvent& p_UpdateEvent);
    void ToggleFreecam();
    void EnableFreecam();
    void DisableFreecam();
    void InstantlyKillNpc();
    void TeleportMainCharacter();
    bool GetFreeCameraRayCastClosestHitQueryOutput(ZRayQueryOutput& p_RayOutput);

private:
    DECLARE_PLUGIN_DETOUR(FreeCam, bool, ZInputAction_Digital, ZInputAction* th, int a2);
    DECLARE_PLUGIN_DETOUR(FreeCam, void, OnLoadScene, ZEntitySceneContext*, ZSceneData&);
    DECLARE_PLUGIN_DETOUR(FreeCam, void, OnClearScene, ZEntitySceneContext*, bool);

private:
    volatile bool m_FreeCamActive;
    volatile bool m_ShouldToggle;
    volatile bool m_FreeCamFrozen;
    ZEntityRef m_OriginalCam;
    ZInputAction m_ToggleFreeCamAction;
    ZInputAction m_FreezeFreeCamActionGc;
    ZInputAction m_FreezeFreeCamActionKb;
    ZInputAction m_InstantlyKillNpcAction;
    ZInputAction m_TeleportMainCharacterAction;
    bool m_ControlsVisible;
    bool m_HasToggledFreecamBefore;
    bool m_EditorStyleFreecam;
    std::unordered_map<std::string, std::string> m_PcControls;
    std::unordered_map<std::string, std::string> m_PcControlsEditorStyle;
    std::unordered_map<std::string, std::string> m_ControllerControls;

    std::vector<CameraPoint> m_cameraPoints;
    double m_lastFrameUpdate_time = 0;
    double m_cameraCurrentTime = 0;
    double m_totalDuration = 0;
    double m_segmentStartTime = 0;
    size_t m_numSegments = -1;
    size_t m_currentSegment = 0;
    bool m_benchmarkStarted = 0;
    HMODULE m_gameBase = 0;
    int* m_pIGC_Running = nullptr;
    int m_IGC_Started = 0;
};

DECLARE_ZHM_PLUGIN(FreeCam)
