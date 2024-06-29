#include "FreeCam.h"

#include <random>

#include "Events.h"
#include "Functions.h"
#include "Logging.h"

#include <Glacier/ZActor.h>
#include <Glacier/SGameUpdateEvent.h>
#include <Glacier/ZObject.h>
#include <Glacier/ZCameraEntity.h>
#include <Glacier/ZApplicationEngineWin32.h>
#include <Glacier/ZEngineAppCommon.h>
#include <Glacier/ZFreeCamera.h>
#include <Glacier/ZRender.h>
#include <Glacier/ZGameLoopManager.h>
#include <Glacier/ZHitman5.h>
#include <Glacier/ZHM5InputManager.h>
#include <Glacier/ZInputActionManager.h>

#include "IconsMaterialDesign.h"
#include <imgui_internal.h>

#include <filesystem>
#include "../../Shared/json.hpp"
#include <numbers>

FreeCam::FreeCam() :
    m_FreeCamActive(false),
    m_ShouldToggle(false),
    m_FreeCamFrozen(false),
    m_ToggleFreeCamAction("ToggleFreeCamera"),
    m_FreezeFreeCamActionGc("ActivateGameControl0"),
    m_FreezeFreeCamActionKb("KBMInspectNode"),
    m_InstantlyKillNpcAction("InstantKill"),
    m_TeleportMainCharacterAction("Teleport"),
    m_ControlsVisible(false),
    m_HasToggledFreecamBefore(false),
    m_EditorStyleFreecam(false)
{
    m_PcControls = {
        { "K", "Toggle freecam" },
        { "F3", "Lock camera and enable 47 input" },
        { "Ctrl + W/S", "Change FOV" },
        { "Ctrl + A/D", "Roll camera" },
        { "Ctrl + X", "Reset roll" },
        { "Alt + W/S", "Change camera speed" },
        { "Space + Q/E", "Change camera height" },
        { "Space + W/S", "Move camera on axis" },
        { "Shift", "Increase camera speed" },
        { "F9", "Kill NPC" },
        { "Ctrl + F9", "Teleport Hitman" },
    };

    m_PcControlsEditorStyle = {
        {"P", "Toggle freecam"},
        {"F3", "Lock camera and enable 47 input"},
        {"MMB", "Drag camera"},
        {"Scroll Wheel", "Zoom camera"},
        {"RMB", "Activate rotate"},
        {"Alt + MMB or RMB", "Orbit camera"},
        {"Z + Alt + MMB or RMB", "Orbit camera around selected entity"},
        {"Z", "Zoom to selected entity (press twice to focus the gizmo)"},
        {"Alt + Scroll Wheel", "Zoom camera with precision"},
        {"Shift", "Speed modifier"},
        {"RMB + Scroll wheel", "Adjust speed"},
    };

    m_ControllerControls = {
        { "Y + L", "Change FOV" },
        { "A + L", "Roll camera" },
        { "A + L press", "Reset rotation" },
        { "B + R", "Change camera speed" },
        { "RT", "Increase camera speed" },
        { "LB", "Lock camera and enable 47 input" },
        { "LT + R", "Change camera height" },
    };
}

FreeCam::~FreeCam()
{
    const ZMemberDelegate<FreeCam, void(const SGameUpdateEvent&)> s_Delegate(this, &FreeCam::OnFrameUpdate);
    Globals::GameLoopManager->UnregisterFrameUpdate(s_Delegate, 1, EUpdateMode::eUpdatePlayMode);

    // Reset the camera to default when unloading with freecam active.
    if (m_FreeCamActive)
    {
        TEntityRef<IRenderDestinationEntity> s_RenderDest;
        Functions::ZCameraManager_GetActiveRenderDestinationEntity->Call(Globals::CameraManager, &s_RenderDest);

        s_RenderDest.m_pInterfaceRef->SetSource(&m_OriginalCam);

        // Enable Hitman input.
        TEntityRef<ZHitman5> s_LocalHitman;
        Functions::ZPlayerRegistry_GetLocalPlayer->Call(Globals::PlayerRegistry, &s_LocalHitman);

        if (s_LocalHitman)
        {
            auto* s_InputControl = Functions::ZHM5InputManager_GetInputControlForLocalPlayer->Call(Globals::InputManager);

            if (s_InputControl)
            {
                Logger::Debug("Got local hitman entity and input control! Enabling input. {} {}", fmt::ptr(s_InputControl), fmt::ptr(s_LocalHitman.m_pInterfaceRef));
                s_InputControl->m_bActive = true;
            }
        }
    }
}


// CSGOSimple's pattern scan
// https://github.com/OneshotGH/CSGOSimple-master/blob/master/CSGOSimple/helpers/utils.cpp
std::uint8_t* PatternScan(void* module, const char* signature)
{
    static auto pattern_to_byte = [](const char* pattern) {
        auto bytes = std::vector<int>{};
        auto start = const_cast<char*>(pattern);
        auto end = const_cast<char*>(pattern) + strlen(pattern);

        for (auto current = start; current < end; ++current) {
            if (*current == '?') {
                ++current;
                if (*current == '?')
                    ++current;
                bytes.push_back(-1);
            }
            else {
                bytes.push_back(strtoul(current, &current, 16));
            }
        }
        return bytes;
        };

    auto dosHeader = (PIMAGE_DOS_HEADER)module;
    auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)module + dosHeader->e_lfanew);

    auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
    auto patternBytes = pattern_to_byte(signature);
    auto scanBytes = reinterpret_cast<std::uint8_t*>(module);

    auto s = patternBytes.size();
    auto d = patternBytes.data();

    for (auto i = 0ul; i < sizeOfImage - s; ++i) {
        bool found = true;
        for (auto j = 0ul; j < s; ++j) {
            if (scanBytes[i + j] != d[j] && d[j] != -1) {
                found = false;
                break;
            }
        }
        if (found) {
            return &scanBytes[i];
        }
    }
    return nullptr;
}

uintptr_t ReadLEA32(uintptr_t Address, size_t offset, size_t lea_size, size_t lea_opcode_size)
{
    uintptr_t Address_Result = Address;
    uintptr_t Patch_Address = 0;
    int32_t lea_offset = 0;
    uintptr_t New_Offset = 0;
    if (Address_Result)
    {
        if (offset)
        {
            Patch_Address = offset + Address_Result;
            lea_offset = *(int32_t*)(lea_size + Address_Result);
            New_Offset = Patch_Address + lea_offset + lea_opcode_size;
        }
        else
        {
            Patch_Address = Address_Result;
            lea_offset = *(int32_t*)(lea_size + Address_Result);
            New_Offset = Patch_Address + lea_offset + lea_opcode_size;
        }
        return New_Offset;
    }
    return 0;
}

void FreeCam::Init()
{
    Hooks::ZInputAction_Digital->AddDetour(this, &FreeCam::ZInputAction_Digital);
    Hooks::ZEntitySceneContext_LoadScene->AddDetour(this, &FreeCam::OnLoadScene);
    Hooks::ZEntitySceneContext_ClearScene->AddDetour(this, &FreeCam::OnClearScene);
    m_gameBase = GetModuleHandle(NULL);
    uintptr_t IsIGC_RunningAddr = (uintptr_t)PatternScan(m_gameBase, "ff 0d ? ? ? ? 48 8d 4b e8 48 83 c4 30 5b e9 ? ? ? ?");
    if (IsIGC_RunningAddr)
    {
        int* IsIGC_Running = nullptr;
        IsIGC_Running = (int*)ReadLEA32(IsIGC_RunningAddr, 0, 2, 6);
        if (IsIGC_Running)
        {
            m_pIGC_Running = IsIGC_Running;
        }
    }
}

void FreeCam::OnEngineInitialized()
{
    const ZMemberDelegate<FreeCam, void(const SGameUpdateEvent&)> s_Delegate(this, &FreeCam::OnFrameUpdate);
    Globals::GameLoopManager->RegisterFrameUpdate(s_Delegate, 1, EUpdateMode::eUpdatePlayMode);

    ZInputTokenStream::ZTokenData result;
    const char* binds = "FreeCameraInput={"
        "ToggleFreeCamera=tap(kb,k);"
        "Teleport=& | hold(kb,lctrl) hold(kb,rctrl) tap(kb,f9);"
        "InstantKill=tap(kb,f9);};";

    if (ZInputActionManager::AddBindings(binds))
    {
        Logger::Debug("Successfully added bindings.");
    }
    else
    {
        Logger::Debug("Failed to add bindings.");
    }
}

float lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

float4 lerp(const float4& a, const float4& b, float t)
{
    return { lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t), 1.0f };
}

SMatrix createSMatrixFromEulerAngles(float rotX, float rotY, float rotZ, const float4& trans)
{
    float cosX = std::cos(rotX);
    float sinX = std::sin(rotX);
    float cosY = std::cos(rotY);
    float sinY = std::sin(rotY);
    float cosZ = std::cos(rotZ);
    float sinZ = std::sin(rotZ);

    float4 XAxis = {
        cosY * cosZ,
        cosY * sinZ,
        -sinY,
        0.0f
    };

    float4 YAxis = {
        sinX * sinY * cosZ - cosX * sinZ,
        sinX * sinY * sinZ + cosX * cosZ,
        sinX * cosY,
        0.0f
    };

    float4 ZAxis = {
        cosX * sinY * cosZ + sinX * sinZ,
        cosX * sinY * sinZ - sinX * cosZ,
        cosX * cosY,
        0.0f
    };

    return { XAxis, YAxis, ZAxis, trans };
}

SMatrix interpolateQNToRT(const float4& startRot, const float4& endRot, const float4& startTrans, const float4& endTrans, float t)
{
    float lerpRotX = lerp(startRot.x, endRot.x, t);
    float lerpRotY = lerp(startRot.y, endRot.y, t);
    float lerpRotZ = lerp(startRot.z, endRot.z, t);
    float4 lerpTrans = lerp(startTrans, endTrans, t);

    return createSMatrixFromEulerAngles(lerpRotX, lerpRotY, lerpRotZ, lerpTrans);
}

constexpr float invert_c_RAD2DEG = (std::numbers::pi / -180.0f);

void FreeCam::ResetCameraPoints()
{
    m_cameraCurrentTime = 0;
    m_totalDuration = 0;
    m_segmentStartTime = 0;
    m_numSegments = -1;
    m_currentSegment = 0;
    m_cameraPoints.clear();
}

void FreeCam::loadCameraPoints(const std::wstring& filename)
{
    std::ifstream file(filename);
    if (!file)
    {
        return;
    }
    nlohmann::json jsonData;
    file >> jsonData;

    for (const auto& point : jsonData)
    {
        float4 rotation = {
            point["rotation"]["x"].get<float>() * invert_c_RAD2DEG,
            point["rotation"]["y"].get<float>() * invert_c_RAD2DEG,
            point["rotation"]["z"].get<float>() * invert_c_RAD2DEG,
            0.0f
        };

        float4 position = {
            point["position"]["x"].get<float>(),
            point["position"]["y"].get<float>(),
            point["position"]["z"].get<float>(),
            1.0f
        };

        double duration = point["duration"].get<double>();

        m_cameraPoints.push_back({ rotation, position, duration });
        m_totalDuration += duration;
        m_numSegments++;
    }
}

void FreeCam::UpdatePoints()
{
    if (m_cameraPoints.empty() || m_currentSegment >= m_cameraPoints.size() - 1)
    {
        // when complete, clear the list so user can reload it
        ResetCameraPoints();
        return;
    }

    if (m_cameraCurrentTime < m_totalDuration)
    {
        const auto& start = m_cameraPoints[m_currentSegment];
        const auto& end = m_cameraPoints[m_currentSegment + 1];
        double segmentDuration = start.duration;

        double t = (m_cameraCurrentTime - m_segmentStartTime) / segmentDuration;
        auto s_Camera = (*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCamera01;
        s_Camera.m_pInterfaceRef->SetWorldMatrix(interpolateQNToRT(start.rotation, end.rotation, start.position, end.position, t));

        m_cameraCurrentTime += m_lastFrameUpdate_time;

        if (m_cameraCurrentTime >= m_segmentStartTime + segmentDuration)
        {
            m_segmentStartTime += segmentDuration;
            m_currentSegment++;
        }
        return;
    }
    // when complete, clear the list so user can reload it
    ResetCameraPoints();
}

void FreeCam::OnFrameUpdate(const SGameUpdateEvent& p_UpdateEvent)
{
    if (!*Globals::ApplicationEngineWin32)
        return;

    if (!(*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCamera01.m_pInterfaceRef)
    {
        Logger::Debug("Creating free camera.");
        Functions::ZEngineAppCommon_CreateFreeCamera->Call(&(*Globals::ApplicationEngineWin32)->m_pEngineAppCommon);

        // If freecam was active we need to toggle.
        // This can happen after level restarts / changes.
        if (m_FreeCamActive)
            m_ShouldToggle = true;
    }

    if (m_EditorStyleFreecam) {
        (*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCameraControlEditorStyle01.m_pInterfaceRef->SetActive(m_FreeCamActive);
    } else {
        (*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCameraControl01.m_pInterfaceRef->SetActive(m_FreeCamActive);
    }

    if (Functions::ZInputAction_Digital->Call(&m_ToggleFreeCamAction, -1))
    {
        ToggleFreecam();
    }

    if (m_ShouldToggle)
    {
        m_ShouldToggle = false;

        if (m_FreeCamActive)
        {
            EnableFreecam();
            ResetCameraPoints();
            std::wstring local_path = std::filesystem::current_path();
            loadCameraPoints(local_path + L"\\data\\camera_points.json");
        }
        else
        {
            DisableFreecam();
            ResetCameraPoints();
        }
    }

    if (!m_benchmarkStarted)
    {
        const auto s_CurrentCamera = Functions::GetCurrentCamera->Call();
        SMatrix CameraLoc = s_CurrentCamera->GetWorldMatrix();
        static bool setOnce;
        if (!setOnce)
        {
            if (*m_pIGC_Running == 1)
            {
                m_IGC_Started = *m_pIGC_Running;
                setOnce = true;
            }
        }
        // `m_IGC_Started` must be true to indicate that IGC has started at least once
        if (m_IGC_Started && *m_pIGC_Running == 0)
        {
            ToggleFreecam();
            m_benchmarkStarted = true;

            {
                TEntityRef<ZHitman5> s_LocalHitman;
                Functions::ZPlayerRegistry_GetLocalPlayer->Call(Globals::PlayerRegistry, &s_LocalHitman);
                // Teleport player to start stage event prior to camera getting there
                if (s_LocalHitman)
                {
                    ZSpatialEntity* s_SpatialEntity = s_LocalHitman.m_ref.QueryInterface<ZSpatialEntity>();
                    SMatrix s_WorldMatrix = s_SpatialEntity->GetWorldMatrix();
                    s_WorldMatrix.Trans.x = -192.15279f;
                    s_WorldMatrix.Trans.y = 22.392776f;
                    s_WorldMatrix.Trans.z = -1.1392722f;
                    s_SpatialEntity->SetWorldMatrix(s_WorldMatrix);
                }
            }
        }
    }
    // While freecam is active, only enable hitman input when the "freeze camera" button is pressed.
    if (m_FreeCamActive)
    {
        if (Functions::ZInputAction_Digital->Call(&m_FreezeFreeCamActionKb, -1))
            m_FreeCamFrozen = !m_FreeCamFrozen;

        if (Functions::ZInputAction_Digital->Call(&m_InstantlyKillNpcAction, -1))
        {
            InstantlyKillNpc();
        }

        if (Functions::ZInputAction_Digital->Call(&m_TeleportMainCharacterAction, -1))
        {
            TeleportMainCharacter();
        }

        const bool s_FreezeFreeCam = Functions::ZInputAction_Digital->Call(&m_FreezeFreeCamActionGc, -1) || m_FreeCamFrozen;

        if (m_EditorStyleFreecam) {
            (*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCameraControlEditorStyle01.m_pInterfaceRef->m_bActive = !s_FreezeFreeCam;
        } else {
            (*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCameraControl01.m_pInterfaceRef->m_bFreezeCamera = s_FreezeFreeCam;
        }

        TEntityRef<ZHitman5> s_LocalHitman;
        Functions::ZPlayerRegistry_GetLocalPlayer->Call(Globals::PlayerRegistry, &s_LocalHitman);

        if (s_LocalHitman)
        {
            auto* s_InputControl = Functions::ZHM5InputManager_GetInputControlForLocalPlayer->Call(Globals::InputManager);

            if (s_InputControl)
                s_InputControl->m_bActive = s_FreezeFreeCam;
        }
        UpdatePoints();
    }
    m_lastFrameUpdate_time = p_UpdateEvent.m_GameTimeDelta.ToSeconds();
}

void FreeCam::OnDrawMenu()
{
    bool s_FreeCamActive = m_FreeCamActive;
    if (ImGui::Checkbox(ICON_MD_PHOTO_CAMERA " FREECAM", &s_FreeCamActive))
    {
        ToggleFreecam();
    }

    if (s_FreeCamActive)
    {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        ImGui::Checkbox("USE EDITOR STYLE FREECAM", &m_EditorStyleFreecam);
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }
    else
    {
        ImGui::Checkbox("USE EDITOR STYLE FREECAM", &m_EditorStyleFreecam);

    }

    if (ImGui::Button(ICON_MD_SPORTS_ESPORTS " FREECAM CONTROLS"))
        m_ControlsVisible = !m_ControlsVisible;
}

void FreeCam::ToggleFreecam()
{
    m_FreeCamActive = !m_FreeCamActive;
    m_ShouldToggle = true;
    m_HasToggledFreecamBefore = true;
}

void FreeCam::EnableFreecam()
{
    auto s_Camera = (*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCamera01;

    TEntityRef<IRenderDestinationEntity> s_RenderDest;
    Functions::ZCameraManager_GetActiveRenderDestinationEntity->Call(Globals::CameraManager, &s_RenderDest);

    m_OriginalCam = *s_RenderDest.m_pInterfaceRef->GetSource();

    const auto s_CurrentCamera = Functions::GetCurrentCamera->Call();
    s_Camera.m_pInterfaceRef->SetWorldMatrix(s_CurrentCamera->GetWorldMatrix());

    Logger::Debug("Camera trans: {}", fmt::ptr(&s_Camera.m_pInterfaceRef->m_mTransform.Trans));

    s_RenderDest.m_pInterfaceRef->SetSource(&s_Camera.m_ref);
}

void FreeCam::DisableFreecam()
{
    TEntityRef<IRenderDestinationEntity> s_RenderDest;
    Functions::ZCameraManager_GetActiveRenderDestinationEntity->Call(Globals::CameraManager, &s_RenderDest);

    s_RenderDest.m_pInterfaceRef->SetSource(&m_OriginalCam);

    // Enable Hitman input.
    TEntityRef<ZHitman5> s_LocalHitman;
    Functions::ZPlayerRegistry_GetLocalPlayer->Call(Globals::PlayerRegistry, &s_LocalHitman);

    if (s_LocalHitman)
    {
        auto* s_InputControl = Functions::ZHM5InputManager_GetInputControlForLocalPlayer->Call(Globals::InputManager);

        if (s_InputControl)
        {
            Logger::Debug("Got local hitman entity and input control! Enabling input. {} {}", fmt::ptr(s_InputControl), fmt::ptr(s_LocalHitman.m_pInterfaceRef));
            s_InputControl->m_bActive = true;
        }
    }
}

void FreeCam::InstantlyKillNpc()
{
    ZRayQueryOutput s_RayOutput{};

    if (GetFreeCameraRayCastClosestHitQueryOutput(s_RayOutput) && s_RayOutput.m_BlockingEntity)
    {
        ZEntityRef s_LogicalParent = s_RayOutput.m_BlockingEntity.GetLogicalParent();
        ZActor* s_Actor = s_LogicalParent.QueryInterface<ZActor>();

        if (s_Actor)
        {
            TEntityRef<IItem> s_Item;
            TEntityRef<ZSetpieceEntity> s_SetPieceEntity;

            Functions::ZActor_KillActor->Call(s_Actor, s_Item, s_SetPieceEntity, EDamageEvent::eDE_UNDEFINED, EDeathBehavior::eDB_IMPACT_ANIM);
        }
    }
}

void FreeCam::TeleportMainCharacter()
{
    ZRayQueryOutput s_RayOutput{};

    if (GetFreeCameraRayCastClosestHitQueryOutput(s_RayOutput) && s_RayOutput.m_BlockingEntity)
    {
        TEntityRef<ZHitman5> s_LocalHitman;

        Functions::ZPlayerRegistry_GetLocalPlayer->Call(Globals::PlayerRegistry, &s_LocalHitman);

        if (s_LocalHitman)
        {
            ZSpatialEntity* s_SpatialEntity = s_LocalHitman.m_ref.QueryInterface<ZSpatialEntity>();
            SMatrix s_WorldMatrix = s_SpatialEntity->GetWorldMatrix();

            s_WorldMatrix.Trans = s_RayOutput.m_vPosition;

            s_SpatialEntity->SetWorldMatrix(s_WorldMatrix);
        }
    }
}

bool FreeCam::GetFreeCameraRayCastClosestHitQueryOutput(ZRayQueryOutput& p_RayOutput)
{
    auto s_Camera = (*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCamera01;
    SMatrix s_WorldMatrix = s_Camera.m_pInterfaceRef->GetWorldMatrix();
    float4 s_InvertedDirection = float4(-s_WorldMatrix.ZAxis.x, -s_WorldMatrix.ZAxis.y, -s_WorldMatrix.ZAxis.z, -s_WorldMatrix.ZAxis.w);
    float4 s_From = s_WorldMatrix.Trans;
    float4 s_To = s_WorldMatrix.Trans + s_InvertedDirection * 500.f;

    if (!*Globals::CollisionManager)
    {
        Logger::Error("Collision manager not found.");

        return false;
    }

    ZRayQueryInput s_RayInput{
        .m_vFrom = s_From,
        .m_vTo = s_To,
    };

    if (!(*Globals::CollisionManager)->RayCastClosestHit(s_RayInput, &p_RayOutput))
    {
        Logger::Error("Raycast failed.");

        return false;
    }

    return true;
}

void FreeCam::OnDrawUI(bool p_HasFocus)
{
    if (m_ControlsVisible)
    {
        ImGui::PushFont(SDK()->GetImGuiBlackFont());
        const auto s_ControlsExpanded = ImGui::Begin(ICON_MD_PHOTO_CAMERA " FreeCam Controls", &m_ControlsVisible);
        ImGui::PushFont(SDK()->GetImGuiRegularFont());

        if (s_ControlsExpanded)
        {
            ImGui::Text("Last frame update time: %lf", m_lastFrameUpdate_time);
            ImGui::Text("m_cameraCurrentTime: %lf", m_cameraCurrentTime);
            ImGui::Text("m_totalDuration: %lf", m_totalDuration);
            ImGui::Text("m_segmentStartTime: %lf", m_segmentStartTime);
            ImGui::Text("m_numSegments: %li", m_numSegments);
            ImGui::Text("m_currentSegment: %li", m_currentSegment);
            const auto s_CurrentCamera = Functions::GetCurrentCamera->Call();
            SMatrix CameraLoc = s_CurrentCamera->GetWorldMatrix();
            ImGui::Text("CameraLoc.XAxis: %f, %f, %f", CameraLoc.XAxis.x, CameraLoc.XAxis.y, CameraLoc.XAxis.z);
            ImGui::Text("CameraLoc.YAxis: %f, %f, %f", CameraLoc.YAxis.x, CameraLoc.YAxis.y, CameraLoc.YAxis.z);
            ImGui::Text("CameraLoc.ZAxis: %f, %f, %f", CameraLoc.ZAxis.x, CameraLoc.ZAxis.y, CameraLoc.ZAxis.z);
            ImGui::Text("CameraLoc.Transform: %f, %f, %f", CameraLoc.Trans.x, CameraLoc.Trans.y, CameraLoc.Trans.z);
            ImGui::Text("m_benchmarkStarted: %s", m_benchmarkStarted ? "true" : "false");
            /*
            ImGui::TextUnformatted("PC Controls");

            ImGui::BeginTable("FreeCamControlsPc", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit);

            if (m_EditorStyleFreecam) {
                for (auto& [s_Key, s_Description]: m_PcControlsEditorStyle) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(s_Key.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(s_Description.c_str());
                }
            } else {
                for (auto& [s_Key, s_Description]: m_PcControls)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(s_Key.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(s_Description.c_str());
                }
            }

            ImGui::EndTable();

            if (!m_EditorStyleFreecam) {
                ImGui::TextUnformatted("Controller Controls");

                ImGui::BeginTable("FreeCamControlsController", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit);

                for (auto& [s_Key, s_Description]: m_ControllerControls) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(s_Key.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(s_Description.c_str());
                }

                ImGui::EndTable();
            }
            */
        }

        ImGui::PopFont();
        ImGui::End();
        ImGui::PopFont();
    }
}

DEFINE_PLUGIN_DETOUR(FreeCam, bool, ZInputAction_Digital, ZInputAction* th, int a2)
{
    if (!m_FreeCamActive)
        return HookResult<bool>(HookAction::Continue());

    if (strcmp(th->m_szName, "ActivateGameControl0") == 0 && m_FreeCamFrozen)
        return HookResult(HookAction::Return(), true);

    return HookResult<bool>(HookAction::Continue());
}

DEFINE_PLUGIN_DETOUR(FreeCam, void, OnLoadScene, ZEntitySceneContext* th, ZSceneData&)
{
    if (m_FreeCamActive)
        DisableFreecam();

    m_FreeCamActive = false;
    m_ShouldToggle = false;

    return HookResult<void>(HookAction::Continue());
}

DEFINE_PLUGIN_DETOUR(FreeCam, void, OnClearScene, ZEntitySceneContext* th, bool)
{
    if (m_FreeCamActive)
        DisableFreecam();

    m_FreeCamActive = false;
    m_ShouldToggle = false;

    return HookResult<void>(HookAction::Continue());
}

DEFINE_ZHM_PLUGIN(FreeCam);
