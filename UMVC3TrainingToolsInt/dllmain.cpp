// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <d3d9.h>
#include "gui\imgui\imgui.h"
#include "gui\imgui\imgui_impl_win32.h"
#include "gui\imgui\imgui_impl_dx9.h"
#include "mutex"
#include "utils/MemoryMgr.h"
#include "utils/Trampoline.h"
#include "utils\Patterns.h"
#include "code\umvc3utils.h"
#include <filesystem>
#include <fstream>
#include "code\Localizer.h"
#include "code\umvc3menu.h"
#include "code/TrainingTeamEditor.h"
#include "code/TrainingShortcut.h"
#include "code/TrainingPad.h"
#include "code/ReleaseUpdate.h"
#include "commdlg.h"
#include <algorithm>
//#include "code\umvc3_sMvc3Main.h"
//#define _CRTDBG_MAP_ALLOC
//#include <stdlib.h>
//#include <crtdbg.h>

using namespace Memory::VP;
using Localizer::UiLabel;
using Localizer::UiText;
bool ShowToolWindow = false;
static bool toolToggleWasDown = false;
bool ShowAboutWindow = false;
//For Making the progress bar for blockstun timers blue.
ImVec4 BStunColor = ImVec4(0.2f, 0.25f, 0.75f, 1.0f);

char path[MAX_PATH];
HMODULE hm = NULL;

std::string TestStr = "Click Me Please.";
//sMvc3Manager* ThreeManager;

//extern "C" bool ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

HWND hWnd = nullptr;
MSG msg;
LPDIRECT3D9 D3d = NULL;
LPDIRECT3DDEVICE9 D3ddev = NULL;
RECT clientRect;
D3DPRESENT_PARAMETERS d3dpp;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

	switch (msg)
	{
		// Hide the tool window so the shortcut can reopen it.
	case WM_CLOSE:
		ShowToolWindow = false;
		//ShowWindow(hWnd, SW_HIDE);
		return 0;

	case WM_DESTROY:
		hwnd = NULL;
		return 0;

	default:
		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}


}

static bool CreateD3DDevice(HWND hWnd)
{
	//Checks for valid Direct3D9 version.
	D3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (D3d == nullptr) { return false; }

	//Gets the Current Window's size.
	GetClientRect(hWnd, &clientRect);
	int width = clientRect.right - clientRect.left;
	int height = clientRect.bottom - clientRect.top;

	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.Windowed = TRUE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	d3dpp.BackBufferWidth = width;
	d3dpp.BackBufferHeight = height;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	d3dpp.hDeviceWindow = hWnd;

	auto deviceParameters = d3dpp;
	if (D3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING, &deviceParameters, &D3ddev) < 0)
		return false;

	return true;
}

static void CleanupD3DDevice() {
	if (D3ddev)
	{
		D3ddev->Release();
		D3ddev = NULL;
	}
	if (D3d)
	{
		D3d->Release();
		D3d = NULL;
	}
}

static void ResizeD3D9Device(int width, int height)
{
	if (!D3ddev)
		return;

	//Gotta update the back buffer size with the new dimensions.
	d3dpp.BackBufferWidth = width;
	d3dpp.BackBufferHeight = height;

	//Other Released Resources Here.
	ImGui_ImplDX9_InvalidateDeviceObjects();

	// Direct3D may clear the back buffer dimensions in the structure passed to Reset.
	auto deviceParameters = d3dpp;
	if (FAILED(D3ddev->Reset(&deviceParameters)))
	{
		OutputDebugStringA("D3D9Device reset has failed!\n");
		return;
	}

	//Other Resources Rebuild here.
	ImGui_ImplDX9_CreateDeviceObjects();
}

//Checks for valid version of the game.
static bool CheckGame()
{
	char* gameName = (char*)(_addr(0x140B12D10));

	if (strcmp(gameName, "umvc3") == 0)
	{
		return true;
	}
	else
	{
		const bool spanish = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_SPANISH;
		MessageBoxW(nullptr, spanish
			? L"Esta versi\u00f3n del juego no es compatible. Usa la versi\u00f3n actual de Steam o la de abril de 2017."
			: L"This game version is incompatible. Use the current Steam version or the April 2017 version.",
			L"UMVC3 Training Tools", MB_ICONINFORMATION);
		return false;
	}
}

static void GetASIPath()
{
	hm = NULL;

	if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)&CheckGame, &hm) == 0)
	{
		int ret = GetLastError();
		fprintf(stderr, "GetModuleHandle failed, error = %d\n", ret);
	}
	if (GetModuleFileName(hm, path, sizeof(path)) == 0)
	{
		int ret = GetLastError();
		fprintf(stderr, "GetModuleFileName failed, error = %d\n", ret);
		// Return or however you want to handle an error.
	}

}

static void TheAboutWindow(bool* p_open)
{
	ImGui::SetNextWindowPos(ImVec2(0, 25));
	RECT clientRect;
	GetClientRect(hWnd, &clientRect);
	int width = clientRect.right - clientRect.left;
	int height = clientRect.bottom - clientRect.top;
	ImGui::SetNextWindowSize(ImVec2(width, (height - 100)));
	if (!ImGui::Begin("UMVC3 Training Tools", p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::End();
		return;
	}
	ImGui::Text(UiText("UI_VERSION"), ReleaseUpdate::VersionTag);
	ImGui::TextUnformatted(UiText("UI_AUTHOR"));
	ImGui::Separator();
	ImGui::TextWrapped("%s", UiText("UI_THANKS"));
	ImGui::Separator();
	ImGui::TextWrapped("%s", UiText("UI_TRAINING_ONLY"));

	if (ImGui::Button(UiText("UI_OK")))
		ShowAboutWindow = false;

	ImGui::End();

}

static void TheMenuBar()
{
	static bool languageSaveFailed = false;
	if (ShowAboutWindow) TheAboutWindow(&ShowAboutWindow);

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu(UiText("MENU_FILE")))
		{
			if (ImGui::MenuItem(UiText("FILE_RESETSETTINGS")))
			{
				ResetSettings();

			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(UiText("MENU_MISC")))
		{
			ImGui::MenuItem(UiText("MISC_ABOUT"), NULL, &ShowAboutWindow);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(UiLabel("UI_LANGUAGE").c_str()))
		{
			const bool spanish = Localizer::IsSpanish();
			if (ImGui::MenuItem(UiText("UI_SPANISH"), nullptr, spanish))
				languageSaveFailed = !Localizer::SetSpanish(true);
			if (ImGui::MenuItem(UiText("UI_ENGLISH"), nullptr, !spanish))
				languageSaveFailed = !Localizer::SetSpanish(false);
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	if (languageSaveFailed)
		ImGui::TextWrapped("%s", UiText("UI_LANGUAGE_SAVE_FAILED"));
}

static void TheExtraOptionsTab()
{
	//if (ImGui::BeginTabItem("Extra"))
	if (ImGui::BeginTabItem(UiLabel("P1_EXTRA").c_str()))
	{
		ImGui::Separator();

		if (ImGui::Button(UiText("P1_RESTARTINTRO")))
		{
			if (CheckTheMode() == true)
			{
				//*(unsigned char*)(block2 + 0x118) = 3;
				int RestartValue = 1;
				*(char*)(sAction + 0x118) = RestartValue;
			}
		}

		ImGui::TextUnformatted(UiText("P1_RESTARTINTROD"));

		if (ImGui::Button(UiText("P1_RESTARTREADY")))
		{
			if (CheckTheMode() == true)
			{
				//*(unsigned char*)(block2 + 0x118) = 3;
				int RestartValue = 2;
				*(char*)(sAction + 0x118) = RestartValue;
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P1_RESTARTREADYD"));

		if (ImGui::Button(UiText("P1_RESTARTTRAINING")))
			TrainingTeamEditor::RequestTrainingRestart();

		ImGui::SeparatorText(UiText("P1_POSITIONING"));

		ImGui::TextUnformatted(UiText("P1_PLAYER1POSITION"));
		if (ImGui::DragFloat(UiText("P1_PLAYER1XPOSITION"), &p1Pos, 1.0f, -800.0f, 800.0f, "%.3f", ImGuiInputTextFlags_CharsDecimal))
		{
			if (p1Pos > 800)
			{
				p1Pos = 800;
			}
			else if (p1Pos < -800)
			{
				p1Pos = -800;
			}
		}

		ImGui::TextUnformatted(UiText("P1_PLAYER2POSITION"));
		if (ImGui::DragFloat(UiText("P1_PLAYER2XPOSITION"), &p2Pos, 1.0f, -800.0f, 800.0f, "%.3f", ImGuiInputTextFlags_CharsDecimal))
		{
			if (p2Pos > 800)
			{
				p2Pos = 800;
			}
			else if (p2Pos < -800)
			{
				p2Pos = -800;
			}
		}

		ImGui::TextUnformatted(UiText("P1_POSITIONPRESETS"));
		if (ImGui::Button(UiText("P1_POSDEFAULT")))
		{

			p1Pos = -125.0f;
			p2Pos = 125.0f;


		}
		ImGui::SameLine();
		if (ImGui::Button(UiText("P1_POSDEFAULT2P")))
		{

			p1Pos = 125.0f;
			p2Pos = -125.0f;


		}
		if (ImGui::Button(UiText("P1_POSLEFT")))
		{
			p1Pos = TrainingTeamEditor::LeftCorner.p1;
			p2Pos = TrainingTeamEditor::LeftCorner.p2;
		}
		ImGui::SameLine();
		if (ImGui::Button(UiText("P1_POSLEFT2P")))
		{

			p1Pos = -800.0f;
			p2Pos = -750.0f;


		}

		if (ImGui::Button(UiText("P1_POSRIGHT")))
		{
			p1Pos = TrainingTeamEditor::RightCorner.p1;
			p2Pos = TrainingTeamEditor::RightCorner.p2;
		}
		ImGui::SameLine();
		if (ImGui::Button(UiText("P1_POSRIGHT2P")))
		{

			p1Pos = 800.0f;
			p2Pos = 750.0f;


		}

		ImGui::SeparatorText(UiText("P1_ETC"));

		if (ImGui::Checkbox(UiText("P1_ENDLESSX1P"), &EndlessXFactorP1))
		{

		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P1_ENDLESSX1PD"));

		ImGui::SameLine();

		if (ImGui::Checkbox(UiText("P1_ENDLESSX2P"), &EndlessXFactorP2))
		{

		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P1_ENDLESSX2PD"));
		ImGui::Separator();

		//ImGui::SeparatorText("Etc.");

		if (ImGui::Checkbox(UiText("P1_MOVEINPUTSTOLEFT"), &MoveInputDisplay))
		{
			LeftSideInputDisplay();
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P1_MOVEINPUTSTOLEFTD"));

		ImGui::Separator();

		//ImGui::Text(UiText("UI_GAME_SPEED"));
		if (ImGui::DragFloat(UiText("P1_CHARACTERSPEEDP1"), &P1Char1Speed, 0.01f, 0.01666667f, 2.0f, "%.3f", ImGuiInputTextFlags_CharsDecimal))
		{

			if (P1Char1Speed > 2.0)
			{
				P1Char1Speed = 2.0;
			}
			else if (P1Char1Speed < 0.01666667)
			{
				P1Char1Speed = 0.01666667;
			}

			SetGlobalPlayerSpeed();
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P1_CHARACTERSPEEDP1D"));

		if (ImGui::DragFloat(UiText("P1_CHARACTERSPEEDP2"), &P2Char1Speed, 0.01f, 0.01666667f, 2.0f, "%.3f", ImGuiInputTextFlags_CharsDecimal))
		{

			if (P2Char1Speed > 2.0)
			{
				P2Char1Speed = 2.0;
			}
			else if (P2Char1Speed < 0.01666667)
			{
				P2Char1Speed = 0.01666667;
			}

			SetGlobalPlayerSpeed();
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P1_CHARACTERSPEEDP2D"));

		if (ImGui::Button(UiText("P1_NORMALSPEEDP1")))
		{
			P1Char1Speed = 1;
			SetGlobalPlayerSpeed();
		}

		ImGui::SameLine();

		if (ImGui::Button(UiText("P1_NORMALSPEEDP2")))
		{
			P2Char1Speed = 1;
			SetGlobalPlayerSpeed();
		}

		if (ImGui::Button(UiText("P1_VJOESLOWSPEEDP1")))
		{
			P1Char1Speed = 0.75;
			SetGlobalPlayerSpeed();
		}

		ImGui::SameLine();

		if (ImGui::Button(UiText("P1_VJOESLOWSPEEDP2")))
		{
			P2Char1Speed = 0.75;
			SetGlobalPlayerSpeed();
		}

		if (ImGui::Button(UiText("P1_VALEOFMISTSPEEDP1")))
		{
			P1Char1Speed = 0.66666666667;
			SetGlobalPlayerSpeed();
		}

		ImGui::SameLine();

		if (ImGui::Button(UiText("P1_VALEOFMISTSPEEDP2")))
		{
			P2Char1Speed = 0.66666666667;
			SetGlobalPlayerSpeed();
		}


#if DEBUG
		if (ImGui::Checkbox(UiText("UI_GAME_SPEED_ENABLE"), &ModifyGameSpeed))
		{

		}


		if (ModifyGameSpeed)
		{
			if (ImGui::SliderFloat(UiText("UI_GAME_SPEED"), &GameSpeed, 0.01666667f, 5.0f))
			{
				SetGameSpeed(GameSpeed);

			}
		}
		else
		{
			ResetGameSpeed();
		}
#endif // DEBUG

		ImGui::Separator();

		ImGui::TextUnformatted(UiText("P1_MORESOON"));

		ImGui::EndTabItem();
	}
}

static void TheStatusOptionsTab()
{
	//Player and Status Related  Settings such as Health.
	if (ImGui::BeginTabItem(UiLabel("P2_STATUS").c_str()))
	{
		ImGui::SeparatorText(UiText("P2_CHARACTERHP"));

		//Toggle for using individual character health.
		if (ImGui::Checkbox(UiText("P2_SETINDIVIDUALHP"), &SetIndividualHP))
		{

		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P2_SETINDIVIDUALHPD"));

		//Toggle for Applying Health Settings to Red Health.
		if (ImGui::Checkbox(UiText("P2_APPLYREDHPP1"), &AlsoSetRedHealthP1))
		{

		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P2_APPLYREDHPP1D"));

		if (ImGui::Checkbox(UiText("P2_APPLYREDHPP2"), &AlsoSetRedHealthP2))
		{

		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P2_APPLYREDHPP2D"));

		if (P1Char1Health)
		{
			ImGui::TextUnformatted(UiText("P2_HEALTHP1C1"));
			if (ImGui::DragFloat(UiText("UI_HP_P1C1"), &P1Char1Health, 0.003f, 0.001f, 1.0f, "%.3f", ImGuiInputTextFlags_CharsDecimal))
			{


				if (P1Char1Health > 1.0)
				{
					P1Char1Health = 1.0;
				}
				else if (P1Char1Health < 0.01)
				{
					P1Char1Health = 0.01;
				}

				if (CheckTheMode() == true)
				{

				}
			}
		}

		if (P1Char2Health)
		{
			ImGui::TextUnformatted(UiText("P2_HEALTHP1C2"));
			if (ImGui::DragFloat(UiText("UI_HP_P1C2"), &P1Char2Health, 0.003f, 0.001f, 1.0f, "%.3f", ImGuiInputTextFlags_CharsDecimal))
			{

				if (P1Char2Health > 1.0)
				{
					P1Char2Health = 1.0;
				}
				else if (P1Char2Health < 0.01)
				{
					P1Char2Health = 0.01;
				}

				if (CheckTheMode() == true)
				{

				}
			}
		}

		if (P1Char3Health)
		{
			ImGui::TextUnformatted(UiText("P2_HEALTHP1C3"));
			if (ImGui::DragFloat(UiText("UI_HP_P1C3"), &P1Char3Health, 0.003f, 0.001f, 1.0f, "%.3f", ImGuiInputTextFlags_CharsDecimal))
			{

				if (P1Char3Health > 1.0)
				{
					P1Char3Health = 1.0;
				}
				else if (P1Char3Health < 0.01)
				{
					P1Char3Health = 0.01;
				}

				if (CheckTheMode() == true)
				{

				}
			}
		}

		if (P2Char1Health)
		{
			ImGui::TextUnformatted(UiText("P2_HEALTHP2C1"));
			if (ImGui::DragFloat(UiText("UI_HP_P2C1"), &P2Char1Health, 0.003f, 0.001f, 1.0f, "%.3f", ImGuiInputTextFlags_CharsDecimal))
			{

				if (P2Char1Health > 1.0)
				{
					P2Char1Health = 1.0;
				}
				else if (P2Char1Health < 0.01)
				{
					P2Char1Health = 0.01;
				}

				if (CheckTheMode() == true)
				{

				}
			}
		}

		if (P2Char2Health)
		{
			ImGui::TextUnformatted(UiText("P2_HEALTHP2C2"));
			if (ImGui::DragFloat(UiText("UI_HP_P2C2"), &P2Char2Health, 0.003f, 0.001f, 1.0f, "%.3f", ImGuiInputTextFlags_CharsDecimal))
			{

				if (P2Char2Health > 1.0)
				{
					P2Char2Health = 1.0;
				}
				else if (P2Char2Health < 0.01)
				{
					P2Char2Health = 0.01;
				}

				if (CheckTheMode() == true)
				{

				}
			}
		}

		if (P2Char3Health)
		{
			ImGui::TextUnformatted(UiText("P2_HEALTHP2C3"));
			if (ImGui::DragFloat(UiText("UI_HP_P2C3"), &P2Char3Health, 0.003f, 0.001f, 1.0f, "%.3f", ImGuiInputTextFlags_CharsDecimal))
			{

				if (P2Char3Health > 1.0)
				{
					P2Char3Health = 1.0;
				}
				else if (P2Char3Health < 0.01)
				{
					P2Char3Health = 0.01;
				}

				if (CheckTheMode() == true)
				{

				}
			}
		}

		ImGui::SeparatorText(UiText("P2_HYPERMETER"));

		//if (P1Meter)
		//{
		ImGui::TextUnformatted(UiText("P2_PLAYER1METER"));
		if (ImGui::DragInt(UiText("P2_P1METER"), &P1Meter, 100.0f, 0, 50000, "%d"))
		{
			if (CheckTheMode() == true)
			{

				if (P1Meter > 50000)
				{
					P1Meter = 50000;
				}
				else if (P1Meter < 0)
				{
					P1Meter = 0;
				}

				SetMeters();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P2_P1METERD"));
		//}

		//if (P2Meter)
		//{
		ImGui::TextUnformatted(UiText("P2_PLAYER2METER"));
		if (ImGui::DragInt(UiText("P2_P2METER"), &P2Meter, 100.0f, 0, 50000, "%d"))
		{
			if (CheckTheMode() == true)
			{


				if (P2Meter > 50000)
				{
					P2Meter = 50000;
				}
				else if (P2Meter < 0)
				{
					P2Meter = 0;
				}

				SetMeters();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P2_P2METERD"));
		//}

		if (ImGui::Checkbox(UiText("P2_METERLOCKP1"), &LockP1Meter))
		{

		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P2_METERLOCKP1D"));

		ImGui::SameLine();

		if (ImGui::Checkbox(UiText("P2_METERLOCKP2"), &LockP2Meter))
		{

		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P2_METERLOCKP2D"));

		ImGui::SeparatorText(UiText("P2_JAMMINGBOMB"));

		if (ImGui::Checkbox(UiText("P2_JAMP1C1"), &P1C1Jammed))
		{
			if (CheckTheMode() == true)
			{
				JammingToggle();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P2_JAMD"));

		if (ImGui::Checkbox(UiText("P2_JAMP1C2"), &P1C2Jammed))
		{
			if (CheckTheMode() == true)
			{
				JammingToggle();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P2_JAMD"));

		if (ImGui::Checkbox(UiText("P2_JAMP1C3"), &P1C3Jammed))
		{
			if (CheckTheMode() == true)
			{
				JammingToggle();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P2_JAMD"));

		if (ImGui::Checkbox(UiText("P2_JAMP2C1"), &P2C1Jammed))
		{
			if (CheckTheMode() == true)
			{
				JammingToggle();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P2_JAMD"));

		if (ImGui::Checkbox(UiText("P2_JAMP2C2"), &P2C2Jammed))
		{
			if (CheckTheMode() == true)
			{
				JammingToggle();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P2_JAMD"));

		if (ImGui::Checkbox(UiText("P2_JAMP2C3"), &P2C3Jammed))
		{
			if (CheckTheMode() == true)
			{
				JammingToggle();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P2_JAMD"));

		ImGui::Separator();

		ImGui::TextUnformatted(UiText("P1_MORESOON"));

		ImGui::EndTabItem();

	}
}

static const char* EvidenceLabel(int index)
{
	constexpr const char* keys[]{
		"P3_EVIDENCE1", "P3_EVIDENCE2", "P3_EVIDENCE3", "P3_EVIDENCE4", "P3_EVIDENCE5",
		"P3_EVIDENCE6", "P3_EVIDENCE7", "P3_EVIDENCE8", "P3_EVIDENCE9", "P3_EVIDENCEA",
		"P3_EVIDENCEB", "P3_EVIDENCEC", "P3_EVIDENCED", "P3_EVIDENCEE"
	};
	return UiText(keys[std::clamp(index, 0, static_cast<int>(std::size(keys)) - 1)]);
}

static void TheCharacterOptionsTab()
{
	if (ImGui::BeginTabItem(UiLabel("P3_CHARACTER").c_str()))
	{

		ImGui::TextUnformatted(UiText("P3_REMEMBER"));

		ImGui::SeparatorText(UiText("P3_FRANK"));

		if (FrankLevel)
		{
			ImGui::TextUnformatted(UiText("P3_FRANKLEVEL"));
			if (ImGui::SliderInt(UiText("UI_LEVEL"), &FrankLevel, 1, 5))
			{
				ChangeFrankLevel(FrankLevel);
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
				ImGui::SetTooltip("%s", UiText("P3_FRANKLEVELD"));
		}


#ifdef _DEBUG

		//ImGui::SeparatorText("Vergil");
		//if (ImGui::Checkbox("Endless Spiral Swords", &VergilSpiralSwordsForever))
		//{

		//}
#endif


		ImGui::SeparatorText(UiText("P3_MRWRIGHT"));

		ImGui::TextUnformatted(UiText("P3_MRWRIGHTEVIDENCE"));
		if (WrightEvidenceA || WrightEvidenceB || WrightEvidenceC)
		{
			ImGui::TextUnformatted(UiText("P3_MRWRIGHTEVIDENCE1"));

			if (ImGui::BeginCombo("##WrightEvidencecombo", EvidenceLabel(EvidenceNameToID(selected_item) + 1)))
			{
				if (CheckTheMode() == true)
				{
					for (int n = 0; n < IM_ARRAYSIZE(EvidenceTypes); n++)
					{
						bool is_selected = (selected_item == EvidenceTypes[n]);
						if (ImGui::Selectable(EvidenceLabel(n), is_selected))
						{
							selected_item = EvidenceTypes[n];
							WrightEvidenceA = EvidenceNameToID(selected_item);
							ChangeWrightEvidence();

						}
						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
				ImGui::SetTooltip("%s", UiText("P3_MRWRIGHTEVIDENCED"));

			ImGui::TextUnformatted(UiText("P3_MRWRIGHTEVIDENCE2"));

			if (ImGui::BeginCombo("##WrightEvidencecombo2", EvidenceLabel(EvidenceNameToID(selected_itemTwo) + 1)))
			{
				if (CheckTheMode() == true)
				{
					for (int n = 0; n < IM_ARRAYSIZE(EvidenceTypesTwo); n++)
					{
						bool is_selected = (selected_itemTwo == EvidenceTypesTwo[n]);
						if (ImGui::Selectable(EvidenceLabel(n), is_selected))
						{
							selected_itemTwo = EvidenceTypesTwo[n];
							WrightEvidenceB = EvidenceNameToID(selected_itemTwo);
							ChangeWrightEvidence();

						}
						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
				ImGui::SetTooltip("%s", UiText("P3_MRWRIGHTEVIDENCED"));

			ImGui::TextUnformatted(UiText("P3_MRWRIGHTEVIDENCE3"));

			if (ImGui::BeginCombo("##WrightEvidencecombo3", EvidenceLabel(EvidenceNameToID(selected_itemThree) + 1)))
			{
				if (CheckTheMode() == true)
				{
					for (int n = 0; n < IM_ARRAYSIZE(EvidenceTypesThree); n++)
					{
						bool is_selected = (selected_itemThree == EvidenceTypesThree[n]);
						if (ImGui::Selectable(EvidenceLabel(n), is_selected))
						{
							selected_itemThree = EvidenceTypesThree[n];
							WrightEvidenceC = EvidenceNameToID(selected_itemThree);
							ChangeWrightEvidence();

						}
						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
				ImGui::SetTooltip("%s", UiText("P3_MRWRIGHTEVIDENCED"));
		}

		if (ImGui::Checkbox(UiText("P3_MRWRIGHTLOCKEVIDENCE"), &LockEvidence))
		{

		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P3_MRWRIGHTLOCKEVIDENCED"));


		//ImGui::Text("Turnabout Toggle");
		if (ImGui::Checkbox(UiText("P3_MRWRIGHTTURNABOUTT"), &Turnabout))
		{
			if (CheckTheMode() == true)
			{
				Objection();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P3_MRWRIGHTTURNABOUTTD"));

		ImGui::SeparatorText(UiText("P3_JEAN"));
		//ImGui::Text("Dark Phoenix Toggle");
		if (ImGui::Checkbox(UiText("P3_JEANDARKPHOENIX"), &DarkPhoenix))
		{
			if (CheckTheMode() == true)
			{
				PopTheBird();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P3_JEANDARKPHOENIXD"));

		ImGui::SeparatorText(UiText("P3_MODOK"));
		if (MODOKLOU)
		{
			ImGui::TextUnformatted(UiText("P3_MODOKLOU"));
			if (ImGui::SliderInt(UiText("UI_UNDERSTANDING"), &MODOKLOU, 1, 10))
			{
				if (CheckTheMode() == true)
				{
					ChangeMODOKUnderstanding(MODOKLOU);
				}
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P3_MODOKLOUD"));

		if (ImGui::Checkbox(UiText("P3_MODOKLOCKU"), &LockMODOKLOU))
		{
			if (CheckTheMode() == true)
			{
				ChangeMODOKUnderstanding(MODOKLOU);
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P3_MODOKLOCKUD"));

		ImGui::SeparatorText(UiText("P3_DORMAMMU"));
		ImGui::TextUnformatted(UiText("P3_DORMAMMUSPELL"));

		ImGui::TextUnformatted(UiText("P3_DORMAMMUSPELLRED"));
		if (ImGui::SliderInt(UiText("P3_DORMRED"), &DormRed, 0, 3))
		{
			if (CheckTheMode() == true)
			{
				DormSpellSet = true;
				SetDormSpellLevels();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P3_DORMAMMUSPELLREDD"));

		ImGui::TextUnformatted(UiText("P3_DORMAMMUSPELLBLUE"));
		if (ImGui::SliderInt(UiText("P3_DORMBLUE"), &DormBlue, 0, 3))
		{
			if (CheckTheMode() == true)
			{
				DormSpellSet = true;
				SetDormSpellLevels();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P3_DORMAMMUSPELLBLUED"));

		if (ImGui::Checkbox(UiText("P3_DORMAMMUSPELLLOCK"), &LockDormSpells))
		{

		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P3_DORMAMMUSPELLLOCKD"));

		ImGui::SeparatorText(UiText("P3_DEADPOOL"));
		//ImGui::Text("Deadpool Teleport");
		if (ImGui::SliderInt(UiText("P3_DEADPOOLTELEPORTCOUNT"), &DeadpoolTeleportCount, 0, 2))
		{
			if (CheckTheMode() == true)
			{
				SetDeadpoolTeleport();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P3_DEADPOOLTELEPORTCOUNTD"));

		if (ImGui::Checkbox(UiText("P3_DEADPOOLTELEPORTLOCK"), &FreezeDeadpoolTPCounter))
		{
			if (CheckTheMode() == true)
			{
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P3_DEADPOOLTELEPORTLOCKD"));


		ImGui::SeparatorText(UiText("P3_WESKER"));

		if (ImGui::Button(UiText("P3_WESKERBREAKGLASSES")))
		{
			if (CheckTheMode() == true)
			{
				BreakWeskerGlasses();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P3_WESKERBREAKGLASSESD"));

		if (ImGui::Checkbox(UiText("P3_WESKERBREAKATSTART"), &BreakGlassesAtRestart))
		{

		}


		ImGui::SeparatorText(UiText("P3_ETC"));
		//ImGui::Text("Endless Install Toggle");
		if (ImGui::Checkbox(UiText("P3_ENDLESSINSTALLS"), &EndlessInstalls))
		{
			if (CheckTheMode() == true)
			{
				EndlessInstallBoolUpdate();
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P3_ENDLESSINSTALLSD"));

		ImGui::Separator();
		ImGui::EndTabItem();

	}

}

static void TheDebugStuffTab()
{
	//For Debug Stuff and keeping records.
	if (ImGui::BeginTabItem("Debug Stuff"))
	{
		GetMainPointers();
		GetPlayerData();
		GetHurtboxData();
		GetHitboxDataPart1();
		GetHitstunTimers();
		GetShots();

		ImGui::Separator();

		//if (ImGui::TreeNode("Collision Stuff"))
		//{

#pragma region Hurtbox Data and Coordinates

		ImGui::SeparatorText("Collision Stuff");

		//The Hurtboxes aren't really stored in the same way as the character data stuff I'm used to working with normally.
		//Have to use this funky looking method to get the characters' hurtboxes during runtime.
		uint64_t t;
		uint64_t tt;
		uint64_t ttt;
		uint64_t tttt;
		uint64_t v;
		//std::list<uint64_t>HurtboxPointers;
		int HurtboxCount = 0;
		//140D47F98

		ImGui::Separator();

		ImGui::Text("Shot List Stuff");

		ImGui::Text("sShotList: %x", sShotList);
		ImGui::Text("P1ShotList: %x", P1Shots);
		ImGui::Text("P2ShotList: %x", P2Shots);
		ImGui::Text("P1ShotList: %x", P1ProjectileCount);
		ImGui::Text("P1ShotList: %x", P2ProjectileCount);

		int ShotTotal = P1ProjectileCount + P2ProjectileCount;
		static ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;
		ImVec2 outer_size = ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 8);
		if (ImGui::BeginTable("Shot List", 12, flags, outer_size))
		{
			ImGui::TableSetupScrollFreeze(1, 1);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide); // Make the first column not hideable to match our use of TableSetupScrollFreeze()
			ImGui::TableSetupColumn("VTable");
			ImGui::TableSetupColumn("Pos X");
			ImGui::TableSetupColumn("Pos Y");
			ImGui::TableSetupColumn("Pos Z");
			ImGui::TableSetupColumn("Quat X");
			ImGui::TableSetupColumn("Quat Y");
			ImGui::TableSetupColumn("Quat Z");
			ImGui::TableSetupColumn("Quat W");
			ImGui::TableSetupColumn("Size X");
			ImGui::TableSetupColumn("Size Y");
			ImGui::TableSetupColumn("Size Z");

			ImGui::TableHeadersRow();
			ImGuiListClipper clipper;
			clipper.Begin(ShotTotal);
			while (clipper.Step())
			{
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
				{
					ImGui::TableNextRow();
					for (int column = 0; column < 12; column++)
					{

						if (!ImGui::TableSetColumnIndex(column) && column > 0)
							continue;
						if (column == 0)
							ImGui::Text("%s", ActiveShots[row].ProjName.c_str());
						else if (column == 1)
							ImGui::Text("%x", ActiveShots[row].ShotVTAddress);
						else if (column == 2)
							ImGui::Text("%f", ActiveShots[row].PossibleAbsPos.X);
						else if (column == 3)
							ImGui::Text("%f", ActiveShots[row].PossibleAbsPos.Y);
						else if (column == 4)
							ImGui::Text("%f", ActiveShots[row].PossibleAbsPos.Z);
						else if (column == 5)
							ImGui::Text("%f", ActiveShots[row].PossibleQuat.X);
						else if (column == 6)
							ImGui::Text("%f", ActiveShots[row].PossibleQuat.Y);
						else if (column == 7)
							ImGui::Text("%f", ActiveShots[row].PossibleQuat.Z);
						else if (column == 8)
							ImGui::Text("%f", ActiveShots[row].PossibleQuat.W);
						else if (column == 9)
							ImGui::Text("%f", ActiveShots[row].PossibleScale.X);
						else if (column == 10)
							ImGui::Text("%f", ActiveShots[row].PossibleScale.Y);
						else if (column == 11)
							ImGui::Text("%f", ActiveShots[row].PossibleScale.Z);
						else continue;
					}
				}
			}
			ImGui::EndTable();


		}
		//ImGui::Text("Shot Count: %i", ShotCount);
		/*
				if (ShotTotal > 0) {
				for (int n = 0; n < ShotTotal; n++)
				{
					ImGui::Text("Shot File: %x", ActiveShots[n].ShotVTAddress);
					ImGui::Text("Shot Name %s", ActiveShots[n].ProjName.c_str());
					ImGui::Text("Possible Position");
					ImGui::SameLine();
					ImGui::Text("X: %f", ActiveShots[n].PossibleAbsPos.X);
					ImGui::SameLine();
					ImGui::Text("Y: %f", ActiveShots[n].PossibleAbsPos.Y);
					ImGui::SameLine();
					ImGui::Text("Z: %f", ActiveShots[n].PossibleAbsPos.Z);
					ImGui::Text("Rotation (Quaternion)");
					ImGui::SameLine();
					ImGui::Text("X: %f", ActiveShots[n].PossibleQuat.X);
					ImGui::SameLine();
					ImGui::Text("Y: %f", ActiveShots[n].PossibleQuat.Y);
					ImGui::SameLine();
					ImGui::Text("Z: %f", ActiveShots[n].PossibleQuat.Z);
					ImGui::SameLine();
					ImGui::Text("W: %f", ActiveShots[n].PossibleQuat.W);
					ImGui::Text("Possible Scale");
					ImGui::SameLine();
					ImGui::Text("X: %f", ActiveShots[n].PossibleScale.X);
					ImGui::SameLine();
					ImGui::Text("Y: %f", ActiveShots[n].PossibleScale.Y);
					ImGui::SameLine();
					ImGui::Text("Z: %f", ActiveShots[n].PossibleScale.Z);
				}
			}

			*/

		ImGui::Separator();



#pragma region P1C1

		ImGui::SeparatorText("Player 1 Character 1");
		//ReadProcessMemory(hProcess, (LPVOID*)(P1Character1Data + 0x4E10), &t, sizeof(uint64_t), 0);
		t = *(uint64_t*)_addr(P1Character1Data + 0x4E10);
		tt = *(uint64_t*)_addr(t + 0x30);
		//ImGui::Text("%x", t);
		//ReadProcessMemory(hProcess, (LPVOID*)(t + 0x30), &tt, sizeof(uint64_t), 0);		
		//ImGui::Text("%x", tt);
		ImGui::Text("Hurtboxes");
		tttt = tt;

		for (int i = 0; i < P1C1HurtboxCount; i++)
		{

			ImGui::Text("X: %f", P1C1Hurtboxes[i].CollData.Coordinates.X);
			ImGui::SameLine();
			ImGui::Text("Y: %f", P1C1Hurtboxes[i].CollData.Coordinates.Y);
			ImGui::SameLine();
			ImGui::Text("Z: %f", P1C1Hurtboxes[i].CollData.Coordinates.Z);
			ImGui::SameLine();
			ImGui::Text("Size: %f", P1C1Hurtboxes[i].CollData.Radius);

		}

		ImGui::Text("Hitboxes");

		for (int i = 0; i < P1C1HitboxCount; i++)
		{

			ImGui::Text("X: %f", P1C1Hitboxes[i].GlobalX);
			ImGui::SameLine();
			ImGui::Text("Y: %f", P1C1Hitboxes[i].GlobalY);
			ImGui::SameLine();
			//ImGui::Text("Z: %f", P1C1Hitboxes[i].GlobalZ);
			//ImGui::SameLine();
			ImGui::Text("Size: %f", P1C1Hitboxes[i].Radius);

		}

#pragma endregion

#pragma region P1C2

		ImGui::SeparatorText("Player 1 Character 2");
		t = *(uint64_t*)_addr(P1Character2Data + 0x4E10);
		tt = *(uint64_t*)_addr(t + 0x30);
		//ImGui::Text("%x", tt);
		ImGui::Text("Hurtboxes");
		tttt = tt;

		for (int i = 0; i < P1C2HurtboxCount; i++)
		{

			ImGui::Text("X: %f", P1C2Hurtboxes[i].CollData.Coordinates.X);
			ImGui::SameLine();
			ImGui::Text("Y: %f", P1C2Hurtboxes[i].CollData.Coordinates.Y);
			ImGui::SameLine();
			ImGui::Text("Z: %f", P1C2Hurtboxes[i].CollData.Coordinates.Z);
			ImGui::SameLine();
			ImGui::Text("Size: %f", P1C2Hurtboxes[i].CollData.Radius);

		}

		ImGui::Text("Hitboxes");

		for (int i = 0; i < P1C2HitboxCount; i++)
		{

			ImGui::Text("X: %f", P1C2Hitboxes[i].GlobalX);
			ImGui::SameLine();
			ImGui::Text("Y: %f", P1C2Hitboxes[i].GlobalY);
			ImGui::SameLine();
			//ImGui::Text("Z: %f", P1C2Hitboxes[i].GlobalZ);
			//ImGui::SameLine();
			ImGui::Text("Size: %f", P1C2Hitboxes[i].Radius);

		}

#pragma endregion

#pragma region P1C3

		ImGui::SeparatorText("Player 1 Character 3");
		t = *(uint64_t*)_addr(P1Character3Data + 0x4E10);
		tt = *(uint64_t*)_addr(t + 0x30);
		//ImGui::Text("%x", tt);
		ImGui::Text("Hurtboxes");
		tttt = tt;

		for (int i = 0; i < P1C3HurtboxCount; i++)
		{

			ImGui::Text("X: %f", P1C3Hurtboxes[i].CollData.Coordinates.X);
			ImGui::SameLine();
			ImGui::Text("Y: %f", P1C3Hurtboxes[i].CollData.Coordinates.Y);
			ImGui::SameLine();
			ImGui::Text("Z: %f", P1C3Hurtboxes[i].CollData.Coordinates.Z);
			ImGui::SameLine();
			ImGui::Text("Size: %f", P1C3Hurtboxes[i].CollData.Radius);

		}

		ImGui::Text("Hitboxes");

		for (int i = 0; i < P1C3HitboxCount; i++)
		{

			ImGui::Text("X: %f", P1C3Hitboxes[i].GlobalX);
			ImGui::SameLine();
			ImGui::Text("Y: %f", P1C3Hitboxes[i].GlobalY);
			ImGui::SameLine();
			//ImGui::Text("Z: %f", P1C3Hitboxes[i].GlobalZ);
			//ImGui::SameLine();
			ImGui::Text("Size: %f", P1C3Hitboxes[i].Radius);

		}

#pragma endregion

#pragma region P2C1

		ImGui::SeparatorText("Player 2 Character 1");
		t = *(uint64_t*)_addr(P2Character1Data + 0x4E10);
		tt = *(uint64_t*)_addr(t + 0x30);
		//ImGui::Text("%x", tt);
		ImGui::Text("Hurtboxes");
		tttt = tt;

		for (int i = 0; i < P2C1HurtboxCount; i++)
		{

			ImGui::Text("X: %f", P2C1Hurtboxes[i].CollData.Coordinates.X);
			ImGui::SameLine();
			ImGui::Text("Y: %f", P2C1Hurtboxes[i].CollData.Coordinates.Y);
			ImGui::SameLine();
			ImGui::Text("Z: %f", P2C1Hurtboxes[i].CollData.Coordinates.Z);
			ImGui::SameLine();
			ImGui::Text("Size: %f", P2C1Hurtboxes[i].CollData.Radius);

		}

		ImGui::Text("Hitboxes");

		for (int i = 0; i < P2C1HitboxCount; i++)
		{

			ImGui::Text("X: %f", P2C1Hitboxes[i].GlobalX);
			ImGui::SameLine();
			ImGui::Text("Y: %f", P2C1Hitboxes[i].GlobalY);
			ImGui::SameLine();
			//ImGui::Text("Z: %f", P2C1Hitboxes[i].GlobalZ);
			//ImGui::SameLine();
			ImGui::Text("Size: %f", P2C1Hitboxes[i].Radius);

		}

#pragma endregion

#pragma region P2C2

		ImGui::SeparatorText("Player 2 Character 2");
		t = *(uint64_t*)_addr(P2Character2Data + 0x4E10);
		tt = *(uint64_t*)_addr(t + 0x30);
		//ImGui::Text("%x", tt);
		ImGui::Text("Hurtboxes");
		tttt = tt;

		for (int i = 0; i < P2C2HurtboxCount; i++)
		{

			ImGui::Text("X: %f", P2C2Hurtboxes[i].CollData.Coordinates.X);
			ImGui::SameLine();
			ImGui::Text("Y: %f", P2C2Hurtboxes[i].CollData.Coordinates.Y);
			ImGui::SameLine();
			ImGui::Text("Z: %f", P2C2Hurtboxes[i].CollData.Coordinates.Z);
			ImGui::SameLine();
			ImGui::Text("Size: %f", P2C2Hurtboxes[i].CollData.Radius);

		}

		ImGui::Text("Hitboxes");

		for (int i = 0; i < P2C2HitboxCount; i++)
		{

			ImGui::Text("X: %f", P2C2Hitboxes[i].GlobalX);
			ImGui::SameLine();
			ImGui::Text("Y: %f", P2C2Hitboxes[i].GlobalY);
			ImGui::SameLine();
			//ImGui::Text("Z: %f", P2C2Hitboxes[i].GlobalZ);
			//ImGui::SameLine();
			ImGui::Text("Size: %f", P2C2Hitboxes[i].Radius);

		}

#pragma endregion

#pragma region P2C3

		ImGui::SeparatorText("Player 2 Character 3");
		t = *(uint64_t*)_addr(P2Character3Data + 0x4E10);
		tt = *(uint64_t*)_addr(t + 0x30);
		//ImGui::Text("%x", tt);
		ImGui::Text("Hurtboxes");
		tttt = tt;

		for (int i = 0; i < P2C3HurtboxCount; i++)
		{

			ImGui::Text("X: %f", P2C3Hurtboxes[i].CollData.Coordinates.X);
			ImGui::SameLine();
			ImGui::Text("Y: %f", P2C3Hurtboxes[i].CollData.Coordinates.Y);
			ImGui::SameLine();
			ImGui::Text("Z: %f", P2C3Hurtboxes[i].CollData.Coordinates.Z);
			ImGui::SameLine();
			ImGui::Text("Size: %f", P2C3Hurtboxes[i].CollData.Radius);

		}

		ImGui::Text("Hitboxes");

		for (int i = 0; i < P2C3HitboxCount; i++)
		{

			ImGui::Text("X: %f", P2C3Hitboxes[i].GlobalX);
			ImGui::SameLine();
			ImGui::Text("Y: %f", P2C3Hitboxes[i].GlobalY);
			ImGui::SameLine();
			//ImGui::Text("Z: %f", P2C3Hitboxes[i].GlobalZ);
			//ImGui::SameLine();
			ImGui::Text("Size: %f", P2C3Hitboxes[i].Radius);

		}

#pragma endregion

#pragma endregion

		//	ImGui::TreePop();
		//}

		if (ImGui::TreeNode("Active installs"))
		{
#pragma region GetInstallID

			GetActiveInstallData();

			//Gets install IDs and Stuff.
			ImGui::SeparatorText("Character Install State IDs");

			ImGui::Text("Player 1 Character 1:");
			ImGui::SameLine();
			ImGui::Text("%i", P1Character1ID);

			ImGui::Text("Slot 1 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C1InstallID1);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C1InstallType1);

			ImGui::Text("Slot 2 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C1InstallID2);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C1InstallType2);

			ImGui::Text("Slot 3 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C1InstallID3);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C1InstallType3);

			ImGui::Text("Slot 4 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C1InstallID4);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C1InstallType4);

			ImGui::Text("Slot 5 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C1InstallID5);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C1InstallType5);

			ImGui::Text("Player 1 Character 2:");
			ImGui::SameLine();
			ImGui::Text("%i", P1Character2ID);

			ImGui::Text("Slot 1 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C2InstallID1);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C2InstallType1);

			ImGui::Text("Slot 2 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C2InstallID2);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C2InstallType2);

			ImGui::Text("Slot 3 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C2InstallID3);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C2InstallType3);

			ImGui::Text("Slot 4 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C2InstallID4);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C2InstallType4);

			ImGui::Text("Slot 5 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C2InstallID5);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C2InstallType5);

			ImGui::Text("Player 1 Character 3:");
			ImGui::SameLine();
			ImGui::Text("%i", P1Character3ID);

			ImGui::Text("Slot 1 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C3InstallID1);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C3InstallType1);

			ImGui::Text("Slot 2 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C3InstallID2);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C3InstallType2);

			ImGui::Text("Slot 3 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C3InstallID3);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C3InstallType3);

			ImGui::Text("Slot 4 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C3InstallID4);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C3InstallType4);

			ImGui::Text("Slot 5 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C3InstallID5);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P1C3InstallType5);

			ImGui::Text("Player 2 Character 1:");
			ImGui::SameLine();
			ImGui::Text("%i", P2Character1ID);

			ImGui::Text("Slot 1 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C1InstallID1);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C1InstallType1);

			ImGui::Text("Slot 2 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C1InstallID2);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C1InstallType2);

			ImGui::Text("Slot 3 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C1InstallID3);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C1InstallType3);

			ImGui::Text("Slot 4 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C1InstallID4);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C1InstallType4);

			ImGui::Text("Slot 5 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C1InstallID5);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C1InstallType5);

			ImGui::Text("Player 2 Character 2:");
			ImGui::SameLine();
			ImGui::Text("%i", P2Character2ID);

			ImGui::Text("Slot 1 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C2InstallID1);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C2InstallType1);

			ImGui::Text("Slot 2 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C2InstallID2);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C2InstallType2);

			ImGui::Text("Slot 3 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C2InstallID3);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C2InstallType3);

			ImGui::Text("Slot 4 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C2InstallID4);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C2InstallType4);

			ImGui::Text("Slot 5 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C2InstallID5);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C2InstallType5);

			ImGui::Text("Player 1 Character 3:");
			ImGui::SameLine();
			ImGui::Text("%i", P2Character3ID);

			ImGui::Text("Slot 1 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C3InstallID1);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C3InstallType1);

			ImGui::Text("Slot 2 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C3InstallID2);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C3InstallType2);

			ImGui::Text("Slot 3 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C3InstallID3);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C3InstallType3);

			ImGui::Text("Slot 4 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C3InstallID4);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C3InstallType4);

			ImGui::Text("Slot 5 - ");
			ImGui::SameLine();
			ImGui::Text("Install ID: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C3InstallID5);
			ImGui::SameLine();
			ImGui::Text(" Type: ");
			ImGui::SameLine();
			ImGui::Text("%x", P2C3InstallType5);


#pragma endregion
			ImGui::TreePop();
		}
		/*
		for (size_t i = 0; i < 6; i++)
		{
			if (scriptableFighters[i].name == nullptr) {
				//ImGui::Text("p%d:%llx", i, scriptableFighters[i].fighter);
				//ImGui::Text("p%d:%llx",i, scriptableFighters[i].fighter);
			}
			else {
				//ImGui::Text("p%d:%llx", i, scriptableFighters[i].fighter);
				//ImGui::Text("p%d:%llx %llx %s", i, scriptableFighters[i].fighter, scriptableFighters[i].tickPtr, scriptableFighters[i].name);
			}


			if (scriptableFighters[i].fighter->tagState == ETagState::Active || scriptableFighters[i].fighter->tagState == ETagState::Active2)
			{
				//ImGui::Text("  X:%d", (int)scriptableFighters[i].fighter->vector.x);
				//ImGui::SameLine();
				//ImGui::Text("- Y:%d", (int)scriptableFighters[i].fighter->vector.y);
				//ImGui::SameLine();
				//ImGui::Text("- Vel:%d", (int)scriptableFighters[i].fighter->vector.xVelocity);

				//ImGui::Text("  Anim:%x", scriptableFighters[i].fighter->anim);
				//ImGui::Text("  Special:%x", scriptableFighters[i].fighter->specialState);

				//ImGui::Text("  Grounded:%s", scriptableFighters[i].GetGroundedState());
				//ImGui::Text("  TagState:%s", scriptableFighters[i].GetTagState());
			}


		}
		*/
		ImGui::EndTabItem();
	}

}

static void TheIncomingStuffTab()
{
	if (ImGui::BeginTabItem(UiLabel("P5_INCOMING").c_str()))
	{
		ImGui::TextUnformatted(UiText("P5_INCOMINGW"));
		ImGui::TextUnformatted(UiText("P5_INCOMINGD"));
		ImGui::Separator();

		ImGui::TextUnformatted(UiText("P5_INCOMINGTD"));
		//if (ImGui::DragInt(UiText("P2_P1METER"), &P1Meter, 100.0f, 0, 50000, "%d"))
		if (ImGui::DragInt(UiText("P5_INCOMINGFRAMES"), &FrameDelayofDeath, 1, 0, 300, "%d"))
		{
			if (FrameDelayofDeath < 0) { FrameDelayofDeath = 0; }
			if (FrameDelayofDeath > 300) { FrameDelayofDeath = 300; }
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P5_INCOMINGFRAMESD"));

		if (ImGui::DragFloat(UiText("P5_INCOMINGKOPOSX"), &DeathSiteX, 1.0f, -800.0f, 800.0f, "%.3f", ImGuiInputTextFlags_CharsDecimal))
		{
			if (DeathSiteX < -800) { DeathSiteX = -800; }
			if (DeathSiteX > 800) { DeathSiteX = 800; }
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P5_INCOMINGKOPOSXD"));

		if (ImGui::DragFloat(UiText("P5_INCOMINGKOPOSY"), &DeathSiteY, 1.0f, 0.0f, 3000.0f, "%.3f", ImGuiInputTextFlags_CharsDecimal))
		{
			if (DeathSiteY < 0) { DeathSiteY = 0; }
			if (DeathSiteY > 3000) { DeathSiteY = 3000; }
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P5_INCOMINGKOPOSYD"));
		ImGui::Separator();

		if (ImGui::Button(UiText("P5_INCOMINGSYNCP1SPOS")))
		{
			DeathSiteX = p1Pos;
			DeathSiteY = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button(UiText("P5_INCOMINGSYNCP2SPOS")))
		{
			DeathSiteX = p2Pos;
			DeathSiteY = 0;
		}
		ImGui::Separator();
		ImGui::TextUnformatted(UiText("P5_INCOMINGDEATHPOSPRESETS"));
		if (ImGui::Button(UiText("P5_INCOMINGPOSCENTER")))
		{
			DeathSiteX = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button(UiText("P5_INCOMINGPOSLCORNER")))
		{
			DeathSiteX = -800;
		}
		ImGui::SameLine();
		if (ImGui::Button(UiText("P5_INCOMINGPOSRCORNER")))
		{
			DeathSiteX = 800;
		}
		ImGui::Separator();
		if (ImGui::Checkbox(UiText("P5_KOACTIVECHARACTERP1"), &KOActiveCharacterAtStart))
		{
			if (CheckTheMode() == true)
			{
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P5_KOACTIVECHARACTERP1D"));

		if (ImGui::Checkbox(UiText("P5_KOACTIVECHARACTERP2"), &KOActiveOpponentAtStart))
		{
			if (CheckTheMode() == true)
			{
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P5_KOACTIVECHARACTERP2D"));

		ImGui::SeparatorText(UiText("P5_KOTOGGLES"));

		if (ImGui::Checkbox(UiText("P5_KOTOGGLEP1C1"), &Player1Character1Dead))
		{
			KOToggles();
		}
		ImGui::SameLine();
		if (ImGui::Checkbox(UiText("P5_KOTOGGLEP1C2"), &Player1Character2Dead))
		{
			KOToggles();
		}
		ImGui::SameLine();
		if (ImGui::Checkbox(UiText("P5_KOTOGGLEP1C3"), &Player1Character3Dead))
		{
			KOToggles();
		}
		if (ImGui::Checkbox(UiText("P5_KOTOGGLEP2C1"), &Player2Character1Dead))
		{
			KOToggles();
		}
		ImGui::SameLine();
		if (ImGui::Checkbox(UiText("P5_KOTOGGLEP2C2"), &Player2Character2Dead))
		{
			KOToggles();
		}
		ImGui::SameLine();
		if (ImGui::Checkbox(UiText("P5_KOTOGGLEP2C3"), &Player2Character3Dead))
		{
			KOToggles();
		}
		ImGui::TextUnformatted(UiText("P1_MORESOON"));

		ImGui::EndTabItem();

	}

}

static void TheExtraDataTab()
{
	if (ImGui::BeginTabItem(UiLabel("P4_DATA").c_str()))
	{
		GetPlayerData();
		//GetHurtboxData();
		GetHitboxDataPart1();
		GetHitstunTimers();

		float progress = 0.0f;
		float Bprogress = 0.0f;
		char buf1[32];
		char buf2[32];
		char buf3[32];
		char buf4[32];
		char buf5[32];
		char buf6[32];
		char buf7[32];
		char buf8[32];
		char buf9[32];
		char bufA[32];
		char bufB[32];
		char bufC[32];
		char bufD[32];
		char bufE[32];
		char bufF[32];
		char bufG[32];
		char bufH[32];
		char bufI[32];
		ImGui::SeparatorText(UiText("P4_REMAININGHITSTUN"));

#pragma region Remaining Hitstun

		//Player 1 Character 1.
		if (P1C1AirGroundState == 1)
		{
			if (P1C1GroundRemaingHitstun > P1C1GroundRemaingHitstunPREV || P1C1GroundRemaingHitstun < (P1C1GroundRemaingHitstunPREV - 2))
			{
				P1C1GroundRemaingHitstunMAX = P1C1GroundRemaingHitstun;
			}

			if (P1C1GroundHitstunTimer == 0 && P1C1AirHitstunTimer == 0 || P1C1GroundRemaingHitstunMAX == 0) //Checks if the character is out of hitstun.
			{
				progress = 0.0;
				P1C1GroundRemaingHitstun = 0;
				P1C1GroundRemaingHitstunMAX = 0;
			}
			else {
				progress = P1C1GroundRemaingHitstun / P1C1GroundRemaingHitstunMAX;
			}
			if (P1C1GroundRemaingHitstun > P1C1GroundRemaingHitstunMAX || P1C1GroundRemaingHitstun == P1C1GroundRemaingHitstunMAX)
			{
				P1C1GroundRemaingHitstunMAX = P1C1GroundRemaingHitstun;
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 150, 210, 255));
				sprintf(buf1, "%d/%d", (int)(P1C1GroundRemaingHitstun), (int)(P1C1GroundRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf1);
				ImGui::PopStyleColor();				
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C1"));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 150, 210, 255));
				sprintf(buf1, "%d/%d", (int)(P1C1GroundRemaingHitstun), (int)(P1C1GroundRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf1);
				ImGui::PopStyleColor();
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C1"));
			}
			P1C1GroundRemaingHitstunPREV = P1C1GroundRemaingHitstun;
		}
		else //Airborne.
		{

			if (P1C1AirRemaingHitstun > P1C1AirRemaingHitstunPREV || P1C1AirRemaingHitstun < (P1C1AirRemaingHitstunPREV - 2))
			{
				P1C1AirRemaingHitstunMAX = P1C1AirRemaingHitstun;
			}

			if (P1C1GroundHitstunTimer == 0 && P1C1AirHitstunTimer == 0 || P1C1AirRemaingHitstunMAX == 0)//Checks if the character is out of hitstun.
			{
				progress = 0.0;
				P1C1AirRemaingHitstun = 0;
				P1C1AirRemaingHitstunMAX = 0;
			}
			else
			{
				progress = P1C1AirRemaingHitstun / P1C1AirRemaingHitstunMAX;
			}

			if (P1C1AirRemaingHitstun > P1C1AirRemaingHitstunMAX || P1C1AirRemaingHitstun == P1C1AirRemaingHitstunMAX)
			{
				P1C1AirRemaingHitstunMAX = P1C1AirRemaingHitstun;
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 150, 210, 255));
				sprintf(buf1, "%d/%d", (int)(P1C1AirRemaingHitstun), (int)(P1C1AirRemaingHitstunMAX));				
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf1);
				ImGui::PopStyleColor();
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C1"));
			}
			else
			{				
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 150, 210, 255));
				sprintf(buf1, "%d/%d", (int)(P1C1AirRemaingHitstun), (int)(P1C1AirRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf1);
				ImGui::PopStyleColor();
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C1"));
			}
			P1C1AirRemaingHitstunPREV = P1C1AirRemaingHitstun;
		}

		//Player 1 Character 2.
		if (P1C2AirGroundState == 1)
		{
			if (P1C2GroundRemaingHitstun > P1C2GroundRemaingHitstunPREV || P1C2GroundRemaingHitstun < (P1C2GroundRemaingHitstunPREV - 2))
			{
				P1C2GroundRemaingHitstunMAX = P1C2GroundRemaingHitstun;
			}
			if (P1C2GroundHitstunTimer == 0 && P1C2AirHitstunTimer == 0 || P1C2GroundRemaingHitstunMAX == 0) //Checks if the character is out of hitstun.
			{
				progress = 0.0;
				P1C2GroundRemaingHitstun = 0;
				P1C2GroundRemaingHitstunMAX = 0;
			}
			else {
				progress = P1C2GroundRemaingHitstun / P1C2GroundRemaingHitstunMAX;
			}
			if (P1C2GroundRemaingHitstun > P1C2GroundRemaingHitstunMAX || P1C2GroundRemaingHitstun == P1C2GroundRemaingHitstunMAX)
			{
				P1C2GroundRemaingHitstunMAX = P1C2GroundRemaingHitstun;
				sprintf(buf2, "%d/%d", (int)(P1C2GroundRemaingHitstun), (int)(P1C2GroundRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf2);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C2"));
			}
			else
			{
				sprintf(buf2, "%d/%d", (int)(P1C2GroundRemaingHitstun), (int)(P1C2GroundRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf2);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C2"));
			}
			P1C2GroundRemaingHitstunPREV = P1C2GroundRemaingHitstun;
		}
		else //Airborne.
		{
			if (P1C2AirRemaingHitstun > P1C2AirRemaingHitstunPREV || P1C2AirRemaingHitstun < (P1C2AirRemaingHitstunPREV - 2))
			{
				P1C2AirRemaingHitstunMAX = P1C2AirRemaingHitstun;
			}
			if (P1C2GroundHitstunTimer == 0 && P1C2AirHitstunTimer == 0 || P1C2AirRemaingHitstunMAX == 0)//Checks if the character is out of hitstun.
			{
				progress = 0.0;
				P1C2AirRemaingHitstun = 0;
				P1C2AirRemaingHitstunMAX = 0;
			}
			else
			{
				progress = P1C2AirRemaingHitstun / P1C2AirRemaingHitstunMAX;
			}

			if (P1C2AirRemaingHitstun > P1C2AirRemaingHitstunMAX || P1C2AirRemaingHitstun == P1C2AirRemaingHitstunMAX)
			{
				P1C2AirRemaingHitstunMAX = P1C2AirRemaingHitstun;
				sprintf(buf2, "%d/%d", (int)(P1C2AirRemaingHitstun), (int)(P1C2AirRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf2);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C2"));
			}
			else
			{
				sprintf(buf2, "%d/%d", (int)(P1C2AirRemaingHitstun), (int)(P1C2AirRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf2);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C2"));
			}
			P1C2AirRemaingHitstunPREV = P1C2AirRemaingHitstun;
		}

		//Player 1 Character 3.
		if (P1C3AirGroundState == 1)
		{
			if (P1C3GroundRemaingHitstun > P1C3GroundRemaingHitstunPREV || P1C3GroundRemaingHitstun < (P1C3GroundRemaingHitstunPREV - 2))
			{
				P1C3GroundRemaingHitstunMAX = P1C3GroundRemaingHitstun;
			}
			if (P1C3GroundHitstunTimer == 0 && P1C3AirHitstunTimer == 0 || P1C3GroundRemaingHitstunMAX == 0) //Checks if the character is out of hitstun.
			{
				progress = 0.0;
				P1C3GroundRemaingHitstun = 0;
				P1C3GroundRemaingHitstunMAX = 0;
			}
			else {
				progress = P1C3GroundRemaingHitstun / P1C3GroundRemaingHitstunMAX;
			}
			if (P1C3GroundRemaingHitstun > P1C3GroundRemaingHitstunMAX || P1C3GroundRemaingHitstun == P1C3GroundRemaingHitstunMAX)
			{
				P1C3GroundRemaingHitstunMAX = P1C3GroundRemaingHitstun;
				sprintf(buf3, "%d/%d", (int)(P1C3GroundRemaingHitstun), (int)(P1C3GroundRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf3);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C3"));
			}
			else
			{
				sprintf(buf3, "%d/%d", (int)(P1C3GroundRemaingHitstun), (int)(P1C3GroundRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf3);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C3"));
			}
			P1C3GroundRemaingHitstunPREV = P1C3GroundRemaingHitstun;
		}
		else //Airborne.
		{
			if (P1C3AirRemaingHitstun > P1C3AirRemaingHitstunPREV || P1C3AirRemaingHitstun < (P1C3AirRemaingHitstunPREV - 2))
			{
				P1C3AirRemaingHitstunMAX = P1C3AirRemaingHitstun;
			}
			if (P1C3GroundHitstunTimer == 0 && P1C3AirHitstunTimer == 0 || P1C3AirRemaingHitstunMAX == 0)//Checks if the character is out of hitstun.
			{
				progress = 0.0;
				P1C3AirRemaingHitstun = 0;
				P1C3AirRemaingHitstunMAX = 0;
			}
			else
			{
				progress = P1C3AirRemaingHitstun / P1C3AirRemaingHitstunMAX;
			}

			if (P1C3AirRemaingHitstun > P1C3AirRemaingHitstunMAX || P1C3AirRemaingHitstun == P1C3AirRemaingHitstunMAX)
			{
				P1C3AirRemaingHitstunMAX = P1C3AirRemaingHitstun;
				sprintf(buf3, "%d/%d", (int)(P1C3AirRemaingHitstun), (int)(P1C3AirRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf3);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C3"));
			}
			else
			{
				sprintf(buf3, "%d/%d", (int)(P1C3AirRemaingHitstun), (int)(P1C3AirRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf3);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C3"));
			}
			P1C3AirRemaingHitstunPREV = P1C3AirRemaingHitstun;
		}

		//Player 2 Character 1.
		if (P2C1AirGroundState == 1)
		{
			if (P2C1GroundRemaingHitstun > P2C1GroundRemaingHitstunPREV || P2C1GroundRemaingHitstun < (P2C1GroundRemaingHitstunPREV - 2))
			{
				P2C1GroundRemaingHitstunMAX = P2C1GroundRemaingHitstun;
			}
			if (P2C1GroundHitstunTimer == 0 && P2C1AirHitstunTimer == 0 || P2C1GroundRemaingHitstunMAX == 0) //Checks if the character is out of hitstun.
			{
				progress = 0.0;
				P2C1GroundRemaingHitstun = 0;
				P2C1GroundRemaingHitstunMAX = 0;
			}
			else {
				progress = P2C1GroundRemaingHitstun / P2C1GroundRemaingHitstunMAX;
			}
			if (P2C1GroundRemaingHitstun > P2C1GroundRemaingHitstunMAX || P2C1GroundRemaingHitstun == P2C1GroundRemaingHitstunMAX)
			{
				P2C1GroundRemaingHitstunMAX = P2C1GroundRemaingHitstun;
				sprintf(buf4, "%d/%d", (int)(P2C1GroundRemaingHitstun), (int)(P2C1GroundRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf4);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C1"));
			}
			else
			{
				sprintf(buf4, "%d/%d", (int)(P2C1GroundRemaingHitstun), (int)(P2C1GroundRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf4);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C1"));
			}
			P2C1GroundRemaingHitstunPREV = P2C1GroundRemaingHitstun;

		}
		else //Airborne.
		{
			if (P2C1AirRemaingHitstun > P2C1AirRemaingHitstunPREV || P2C1AirRemaingHitstun < (P2C1AirRemaingHitstunPREV - 2))
			{
				P2C1AirRemaingHitstunMAX = P2C1AirRemaingHitstun;
			}
			if (P2C1GroundHitstunTimer == 0 && P2C1AirHitstunTimer == 0 || P2C1AirRemaingHitstunMAX == 0)//Checks if the character is out of hitstun.
			{
				progress = 0.0;
				P2C1AirRemaingHitstun = 0;
				P2C1AirRemaingHitstunMAX = 0;
			}
			else
			{
				progress = P2C1AirRemaingHitstun / P2C1AirRemaingHitstunMAX;
			}

			if (P2C1AirRemaingHitstun > P2C1AirRemaingHitstunMAX || P2C1AirRemaingHitstun == P2C1AirRemaingHitstunMAX)
			{
				P2C1AirRemaingHitstunMAX = P2C1AirRemaingHitstun;				
				sprintf(buf4, "%d/%d", (int)(P2C1AirRemaingHitstun), (int)(P2C1AirRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf4);

				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C1"));
			}
			else
			{
				
				sprintf(buf4, "%d/%d", (int)(P2C1AirRemaingHitstun), (int)(P2C1AirRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf4);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C1"));
			}
			P2C1AirRemaingHitstunPREV = P2C1AirRemaingHitstun;
		}

		//Player 2 Character 2.
		if (P2C2AirGroundState == 1)
		{
			if (P2C2GroundRemaingHitstun > P2C2GroundRemaingHitstunPREV || P2C2GroundRemaingHitstun < (P2C2GroundRemaingHitstunPREV - 2))
			{
				P2C2GroundRemaingHitstunMAX = P2C2GroundRemaingHitstun;
			}
			if (P2C2GroundHitstunTimer == 0 && P2C2AirHitstunTimer == 0 || P2C2GroundRemaingHitstunMAX == 0) //Checks if the character is out of hitstun.
			{
				progress = 0.0;
				P2C2GroundRemaingHitstun = 0;
				P2C2GroundRemaingHitstunMAX = 0;
			}
			else {
				progress = P2C2GroundRemaingHitstun / P2C2GroundRemaingHitstunMAX;
			}
			if (P2C2GroundRemaingHitstun > P2C2GroundRemaingHitstunMAX || P2C2GroundRemaingHitstun == P2C2GroundRemaingHitstunMAX)
			{
				P2C2GroundRemaingHitstunMAX = P2C2GroundRemaingHitstun;
				sprintf(buf5, "%d/%d", (int)(P2C2GroundRemaingHitstun), (int)(P2C2GroundRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf5);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C2"));
			}
			else
			{
				sprintf(buf5, "%d/%d", (int)(P2C2GroundRemaingHitstun), (int)(P2C2GroundRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf5);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C2"));
			}
			P2C2GroundRemaingHitstunPREV = P2C2GroundRemaingHitstun;

		}
		else //Airborne.
		{
			if (P2C2AirRemaingHitstun > P2C2AirRemaingHitstunPREV || P2C2AirRemaingHitstun < (P2C2AirRemaingHitstunPREV - 2))
			{
				P2C2AirRemaingHitstunMAX = P2C2AirRemaingHitstun;
			}
			if (P2C2GroundHitstunTimer == 0 && P2C2AirHitstunTimer == 0 || P2C2AirRemaingHitstunMAX == 0)//Checks if the character is out of hitstun.
			{
				progress = 0.0;
				P2C2AirRemaingHitstun = 0;
				P2C2AirRemaingHitstunMAX = 0;
			}
			else
			{
				progress = P2C2AirRemaingHitstun / P2C2AirRemaingHitstunMAX;
			}

			if (P2C2AirRemaingHitstun > P2C2AirRemaingHitstunMAX || P2C2AirRemaingHitstun == P2C2AirRemaingHitstunMAX)
			{
				P2C2AirRemaingHitstunMAX = P2C2AirRemaingHitstun;
				sprintf(buf5, "%d/%d", (int)(P2C2AirRemaingHitstun), (int)(P2C2AirRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf5);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C2"));
			}
			else
			{
				sprintf(buf5, "%d/%d", (int)(P2C2AirRemaingHitstun), (int)(P2C2AirRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf5);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C2"));
			}
			P2C2AirRemaingHitstunPREV = P2C2AirRemaingHitstun;
		}

		//Player 2 Character 3.
		if (P2C3AirGroundState == 1)
		{
			if (P2C3GroundRemaingHitstun > P2C3GroundRemaingHitstunPREV || P2C3GroundRemaingHitstun < (P2C3GroundRemaingHitstunPREV - 2))
			{
				P2C3GroundRemaingHitstunMAX = P2C3GroundRemaingHitstun;
			}
			if (P2C3GroundHitstunTimer == 0 && P2C3AirHitstunTimer == 0 || P2C3GroundRemaingHitstunMAX == 0) //Checks if the character is out of hitstun.
			{
				progress = 0.0;
				P2C3GroundRemaingHitstun = 0;
				P2C3GroundRemaingHitstunMAX = 0;
			}
			else {
				progress = P2C3GroundRemaingHitstun / P2C3GroundRemaingHitstunMAX;
			}
			if (P2C3GroundRemaingHitstun > P2C3GroundRemaingHitstunMAX || P2C3GroundRemaingHitstun == P2C3GroundRemaingHitstunMAX)
			{
				P2C3GroundRemaingHitstunMAX = P2C3GroundRemaingHitstun;
				sprintf(buf6, "%d/%d", (int)(P2C3GroundRemaingHitstun), (int)(P2C3GroundRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf6);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C3"));
			}
			else
			{
				sprintf(buf6, "%d/%d", (int)(P2C3GroundRemaingHitstun), (int)(P2C3GroundRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf6);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C3"));
			}
			P2C3GroundRemaingHitstunPREV = P2C3GroundRemaingHitstun;
		}
		else //Airborne.
		{
			if (P2C3AirRemaingHitstun > P2C3AirRemaingHitstunPREV || P2C3AirRemaingHitstun < (P2C3AirRemaingHitstunPREV - 2))
			{
				P2C3AirRemaingHitstunMAX = P2C3AirRemaingHitstun;
			}
			if (P2C3GroundHitstunTimer == 0 && P2C3AirHitstunTimer == 0 || P2C3AirRemaingHitstunMAX == 0)//Checks if the character is out of hitstun.
			{
				progress = 0.0;
				P2C3AirRemaingHitstun = 0;
				P2C3AirRemaingHitstunMAX = 0;
			}
			else
			{
				progress = P2C3AirRemaingHitstun / P2C3AirRemaingHitstunMAX;
			}

			if (P2C3AirRemaingHitstun > P2C3AirRemaingHitstunMAX || P2C3AirRemaingHitstun == P2C3AirRemaingHitstunMAX)
			{
				P2C3AirRemaingHitstunMAX = P2C3AirRemaingHitstun;
				sprintf(buf6, "%d/%d", (int)(P2C3AirRemaingHitstun), (int)(P2C3AirRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf6);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C3"));
			}
			else
			{
				sprintf(buf6, "%d/%d", (int)(P2C3AirRemaingHitstun), (int)(P2C3AirRemaingHitstunMAX));
				ImGui::ProgressBar((progress), ImVec2(0.f, 0.f), buf6);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C3"));
			}
			P2C3AirRemaingHitstunPREV = P2C3AirRemaingHitstun;
		}

		

#pragma endregion

#pragma region Remaining Blockstun
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, BStunColor);
		ImGui::SeparatorText(UiText("P4_REMAININGBLOCKSTUN"));
		//Makes it Blue.

		//Checks for Pushblock Timer not being 0, and gets that instead if not 0.
		if (!P1C1PushblockTimer) 
		{
			if (P1C1RemaingBlockstun > P1C1RemaingBlockstunPREV || P1C1RemaingBlockstun < (P1C1RemaingBlockstunPREV - 2))
			{
				P1C1RemaingBlockstunMAX = P1C1RemaingBlockstun;
			}

			if (P1C1RemaingBlockstunMAX == 0 || P1C1RemaingBlockstun == 0) //Checks if the character is out of Blockstun.
			{
				Bprogress = 0.0;
				P1C1RemaingBlockstun = 0;
				P1C1RemaingBlockstunMAX = 0;
			}
			else
			{
				Bprogress = P1C1RemaingBlockstun / P1C1RemaingBlockstunMAX;
			}

			if (P1C1RemaingBlockstun > P1C1RemaingBlockstunMAX || P1C1RemaingBlockstun == P1C1RemaingBlockstunMAX)
			{
				P1C1RemaingBlockstunMAX = P1C1RemaingBlockstun;
				sprintf(buf7, "%d/%d", (int)(P1C1RemaingBlockstun), (int)(P1C1RemaingBlockstunMAX));				
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), buf7);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C1"));
			}
			else
			{
				sprintf(buf7, "%d/%d", (int)(P1C1RemaingBlockstun), (int)(P1C1RemaingBlockstunMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), buf7);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C1"));
			}
			P1C1RemaingBlockstunPREV = P1C1RemaingBlockstun;
		}
		else
		{
			if(P1C1PushblockTimer > P1C1PushblockTimerPREV || P1C1PushblockTimer < (P1C1PushblockTimerPREV - 2))
			{			
				P1C1PushblockTimerMAX = P1C1PushblockTimer;			
			}

			if (P1C1PushblockTimerMAX == 0 || P1C1PushblockTimer == 0) //Checks if the character is out of Blockstun.
			{
				Bprogress = 0.0;
				P1C1PushblockTimer = 0;
				P1C1PushblockTimerMAX = 0;
			}
			else
			{
				Bprogress = P1C1PushblockTimer / P1C1PushblockTimerMAX;
			}
		

			if (P1C1PushblockTimer > P1C1PushblockTimerMAX || P1C1PushblockTimer == P1C1PushblockTimerMAX)
			{
				P1C1PushblockTimerMAX = P1C1PushblockTimer;
				sprintf(bufD, "%d/%d", (int)(P1C1PushblockTimer), (int)(P1C1PushblockTimerMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufD);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C1"));
			}
			else
			{
				sprintf(bufD, "%d/%d", (int)(P1C1PushblockTimer), (int)(P1C1PushblockTimerMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufD);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C1"));
			}

			P1C1PushblockTimerPREV = P1C1PushblockTimer;
		}

		//Checks for Pushblock Timer not being 0, and gets that instead if not 0.
		if (!P1C2PushblockTimer)
		{
			if (P1C2RemaingBlockstun > P1C2RemaingBlockstunPREV || P1C2RemaingBlockstun < (P1C2RemaingBlockstunPREV - 2))
			{
				P1C2RemaingBlockstunMAX = P1C2RemaingBlockstun;
			}

			if (P1C2RemaingBlockstunMAX == 0) //Checks if the character is out of Blockstun.
			{
				Bprogress = 0.0;
				P1C2RemaingBlockstun = 0;
				P1C2RemaingBlockstunMAX = 0;
			}
			else
			{
				Bprogress = P1C2RemaingBlockstun / P1C2RemaingBlockstunMAX;
			}

			if (P1C2RemaingBlockstun > P1C2RemaingBlockstunMAX || P1C2RemaingBlockstun == P1C2RemaingBlockstunMAX)
			{
				P1C2RemaingBlockstunMAX = P1C2RemaingBlockstun;
				sprintf(buf8, "%d/%d", (int)(P1C2RemaingBlockstun), (int)(P1C2RemaingBlockstunMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), buf8);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C2"));
			}
			else
			{
				sprintf(buf8, "%d/%d", (int)(P1C2RemaingBlockstun), (int)(P1C2RemaingBlockstunMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), buf8);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C2"));
			}
			P1C2RemaingBlockstunPREV = P1C2RemaingBlockstun;
		}
		else
		{
			if (P1C2PushblockTimer > P1C2PushblockTimerPREV || P1C2PushblockTimer < (P1C2PushblockTimerPREV - 2))
			{
				P1C2PushblockTimerMAX = P1C2PushblockTimer;
			}

			if (P1C2PushblockTimerMAX == 0) //Checks if the character is out of Blockstun.
			{
				Bprogress = 0.0;
				P1C2PushblockTimer = 0;
				P1C2PushblockTimerMAX = 0;
			}
			else
			{
				Bprogress = P1C2PushblockTimer / P1C2PushblockTimerMAX;
			}


			if (P1C2PushblockTimer > P1C2PushblockTimerMAX || P1C2PushblockTimer == P1C2PushblockTimerMAX)
			{
				P1C2PushblockTimerMAX = P1C2PushblockTimer;
				sprintf(bufE, "%d/%d", (int)(P1C2PushblockTimer), (int)(P1C2PushblockTimerMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufE);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C2"));
			}
			else
			{
				sprintf(bufE, "%d/%d", (int)(P1C2PushblockTimer), (int)(P1C2PushblockTimerMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufE);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C2"));
			}

			P1C2PushblockTimerPREV = P1C2PushblockTimer;
		}

		//Checks for Pushblock Timer not being 0, and gets that instead if not 0.
		if (!P1C3PushblockTimer)
		{
			if (P1C3RemaingBlockstun > P1C3RemaingBlockstunPREV || P1C3RemaingBlockstun < (P1C3RemaingBlockstunPREV - 2))
			{
				P1C3RemaingBlockstunMAX = P1C3RemaingBlockstun;
			}

			if (P1C3RemaingBlockstunMAX == 0) //Checks if the character is out of Blockstun.
			{
				Bprogress = 0.0;
				P1C3RemaingBlockstun = 0;
				P1C3RemaingBlockstunMAX = 0;
			}
			else
			{
				Bprogress = P1C3RemaingBlockstun / P1C3RemaingBlockstunMAX;
			}

			if (P1C3RemaingBlockstun > P1C3RemaingBlockstunMAX || P1C3RemaingBlockstun == P1C3RemaingBlockstunMAX)
			{
				P1C3RemaingBlockstunMAX = P1C3RemaingBlockstun;
				sprintf(buf9, "%d/%d", (int)(P1C3RemaingBlockstun), (int)(P1C3RemaingBlockstunMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), buf9);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C3"));
			}
			else
			{
				sprintf(buf9, "%d/%d", (int)(P1C3RemaingBlockstun), (int)(P1C3RemaingBlockstunMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), buf9);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C3"));
			}
			P1C3RemaingBlockstunPREV = P1C3RemaingBlockstun;
		}
		else
		{
			if (P1C3PushblockTimer > P1C3PushblockTimerPREV || P1C3PushblockTimer < (P1C3PushblockTimerPREV - 2))
			{
				P1C3PushblockTimerMAX = P1C3PushblockTimer;
			}

			if (P1C3PushblockTimerMAX == 0) //Checks if the character is out of Blockstun.
			{
				Bprogress = 0.0;
				P1C3PushblockTimer = 0;
				P1C3PushblockTimerMAX = 0;
			}
			else
			{
				Bprogress = P1C3PushblockTimer / P1C3PushblockTimerMAX;
			}


			if (P1C3PushblockTimer > P1C3PushblockTimerMAX || P1C3PushblockTimer == P1C3PushblockTimerMAX)
			{
				P1C3PushblockTimerMAX = P1C3PushblockTimer;
				sprintf(bufF, "%d/%d", (int)(P1C3PushblockTimer), (int)(P1C3PushblockTimerMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufF);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C3"));
			}
			else
			{
				sprintf(bufF, "%d/%d", (int)(P1C3PushblockTimer), (int)(P1C3PushblockTimerMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufF);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP1C3"));
			}

			P1C3PushblockTimerPREV = P1C3PushblockTimer;
		}

		//Checks for Pushblock Timer not being 0, and gets that instead if not 0.
		if (!P2C1PushblockTimer)
		{
			if (P2C1RemaingBlockstun > P2C1RemaingBlockstunPREV || P2C1RemaingBlockstun < (P2C1RemaingBlockstunPREV - 2))
			{
				P2C1RemaingBlockstunMAX = P2C1RemaingBlockstun;
			}

			if (P2C1RemaingBlockstunMAX == 0) //Checks if the character is out of Blockstun.
			{
				Bprogress = 0.0;
				P2C1RemaingBlockstun = 0;
				P2C1RemaingBlockstunMAX = 0;
			}
			else
			{
				Bprogress = P2C1RemaingBlockstun / P2C1RemaingBlockstunMAX;
			}

			if (P2C1RemaingBlockstun > P2C1RemaingBlockstunMAX || P2C1RemaingBlockstun == P2C1RemaingBlockstunMAX)
			{
				P2C1RemaingBlockstunMAX = P2C1RemaingBlockstun;
				sprintf(bufA, "%d/%d", (int)(P2C1RemaingBlockstun), (int)(P2C1RemaingBlockstunMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufA);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C1"));
			}
			else
			{
				sprintf(bufA, "%d/%d", (int)(P2C1RemaingBlockstun), (int)(P2C1RemaingBlockstunMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufA);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C1"));
			}
			P2C1RemaingBlockstunPREV = P2C1RemaingBlockstun;
		}
		else
		{
			if (P2C1PushblockTimer > P2C1PushblockTimerPREV || P2C1PushblockTimer < (P2C1PushblockTimerPREV - 2))
			{
				P2C1PushblockTimerMAX = P2C1PushblockTimer;
			}

			if (P2C1PushblockTimerMAX == 0) //Checks if the character is out of Blockstun.
			{
				Bprogress = 0.0;
				P2C1PushblockTimer = 0;
				P2C1PushblockTimerMAX = 0;
			}
			else
			{
				Bprogress = P2C1PushblockTimer / P2C1PushblockTimerMAX;
			}


			if (P2C1PushblockTimer > P2C1PushblockTimerMAX || P2C1PushblockTimer == P2C1PushblockTimerMAX)
			{
				P2C1PushblockTimerMAX = P2C1PushblockTimer;
				sprintf(bufG, "%d/%d", (int)(P2C1PushblockTimer), (int)(P2C1PushblockTimerMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufG);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C1"));
			}
			else
			{
				sprintf(bufG, "%d/%d", (int)(P2C1PushblockTimer), (int)(P2C1PushblockTimerMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufG);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C1"));
			}

			P2C1PushblockTimerPREV = P2C1PushblockTimer;
		}

		//Checks for Pushblock Timer not being 0, and gets that instead if not 0.
		if (!P2C2PushblockTimer)
		{
			if (P2C2RemaingBlockstun > P2C2RemaingBlockstunPREV || P2C2RemaingBlockstun < (P2C2RemaingBlockstunPREV - 2))
			{
				P2C2RemaingBlockstunMAX = P2C2RemaingBlockstun;
			}

			if (P2C2RemaingBlockstunMAX == 0) //Checks if the character is out of Blockstun.
			{
				Bprogress = 0.0;
				P2C2RemaingBlockstun = 0;
				P2C2RemaingBlockstunMAX = 0;
			}
			else
			{
				Bprogress = P2C2RemaingBlockstun / P2C2RemaingBlockstunMAX;
			}

			if (P2C2RemaingBlockstun > P2C2RemaingBlockstunMAX || P2C2RemaingBlockstun == P2C2RemaingBlockstunMAX)
			{
				P2C2RemaingBlockstunMAX = P2C2RemaingBlockstun;
				sprintf(bufB, "%d/%d", (int)(P2C2RemaingBlockstun), (int)(P2C2RemaingBlockstunMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufB);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C2"));
			}
			else
			{
				sprintf(bufB, "%d/%d", (int)(P2C2RemaingBlockstun), (int)(P2C2RemaingBlockstunMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufB);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C2"));
			}
			P2C2RemaingBlockstunPREV = P2C2RemaingBlockstun;
		}
		else
		{
			if (P2C2PushblockTimer > P2C2PushblockTimerPREV || P2C2PushblockTimer < (P2C2PushblockTimerPREV - 2))
			{
				P2C2PushblockTimerMAX = P2C2PushblockTimer;
			}

			if (P2C2PushblockTimerMAX == 0) //Checks if the character is out of Blockstun.
			{
				Bprogress = 0.0;
				P2C2PushblockTimer = 0;
				P2C2PushblockTimerMAX = 0;
			}
			else
			{
				Bprogress = P2C2PushblockTimer / P2C2PushblockTimerMAX;
			}


			if (P2C2PushblockTimer > P2C2PushblockTimerMAX || P2C2PushblockTimer == P2C2PushblockTimerMAX)
			{
				P2C2PushblockTimerMAX = P2C2PushblockTimer;
				sprintf(bufH, "%d/%d", (int)(P2C2PushblockTimer), (int)(P2C2PushblockTimerMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufH);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C2"));
			}
			else
			{
				sprintf(bufH, "%d/%d", (int)(P2C2PushblockTimer), (int)(P2C2PushblockTimerMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufH);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C2"));
			}

			P2C2PushblockTimerPREV = P2C2PushblockTimer;
		}

		//Checks for Pushblock Timer not being 0, and gets that instead if not 0.
		if (!P2C3PushblockTimer)
		{
			if (P2C3RemaingBlockstun > P2C3RemaingBlockstunPREV || P2C3RemaingBlockstun < (P2C3RemaingBlockstunPREV - 2))
			{
				P2C3RemaingBlockstunMAX = P2C3RemaingBlockstun;
			}

			if (P2C3RemaingBlockstunMAX == 0) //Checks if the character is out of Blockstun.
			{
				Bprogress = 0.0;
				P2C3RemaingBlockstun = 0;
				P2C3RemaingBlockstunMAX = 0;
			}
			else
			{
				Bprogress = P2C3RemaingBlockstun / P2C3RemaingBlockstunMAX;
			}

			if (P2C3RemaingBlockstun > P2C3RemaingBlockstunMAX || P2C3RemaingBlockstun == P2C3RemaingBlockstunMAX)
			{
				P2C3RemaingBlockstunMAX = P2C3RemaingBlockstun;
				sprintf(bufC, "%d/%d", (int)(P2C3RemaingBlockstun), (int)(P2C3RemaingBlockstunMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufC);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C3"));
			}
			else
			{
				sprintf(bufC, "%d/%d", (int)(P2C3RemaingBlockstun), (int)(P2C3RemaingBlockstunMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufC);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C3"));
			}
			P2C3RemaingBlockstunPREV = P2C3RemaingBlockstun;
		}
		else
		{
			if (P2C3PushblockTimer > P2C3PushblockTimerPREV || P2C3PushblockTimer < (P2C3PushblockTimerPREV - 2))
			{
				P2C3PushblockTimerMAX = P2C3PushblockTimer;
			}

			if (P2C3PushblockTimerMAX == 0) //Checks if the character is out of Blockstun.
			{
				Bprogress = 0.0;
				P2C3PushblockTimer = 0;
				P2C3PushblockTimerMAX = 0;
			}
			else
			{
				Bprogress = P2C3PushblockTimer / P2C3PushblockTimerMAX;
			}


			if (P2C3PushblockTimer > P2C3PushblockTimerMAX || P2C3PushblockTimer == P2C3PushblockTimerMAX)
			{
				P2C3PushblockTimerMAX = P2C3PushblockTimer;
				sprintf(bufI, "%d/%d", (int)(P2C3PushblockTimer), (int)(P2C3PushblockTimerMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufI);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C3"));
			}
			else
			{
				sprintf(bufI, "%d/%d", (int)(P2C3PushblockTimer), (int)(P2C3PushblockTimerMAX));
				ImGui::ProgressBar((Bprogress), ImVec2(0.f, 0.f), bufI);
				ImGui::SameLine();
				ImGui::SeparatorText(UiText("P4_HITSTUNP2C3"));
			}

			P2C3PushblockTimerPREV = P2C3PushblockTimer;
		}


		ImGui::PopStyleColor();


#pragma endregion


		ImGui::SeparatorText(UiText("P4_HITSTUNTIMERS"));

		if (ImGui::TreeNode(UiText("P4_HITSTUNTIMERSSH")))
		{
#pragma region Hitstun Stuff

			//Makes the text larger.
			ImGui::PushFont(NULL, 20.0f);



			static ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

			ImGuiStyle& style = ImGui::GetStyle();
			ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, (float)(int)(style.FramePadding.y * 0.60f));
			ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, (float)(int)(style.ItemSpacing.y * 0.60f));

			ImGui::PopStyleVar(2);

			if (ImGui::BeginTable("table1", 5, flags))
			{
				ImGui::TableSetupColumn(UiText("P4_HITSTUNPLAYER"), ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn(UiText("P4_HITSTUNGROUND"), ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn(UiText("P4_HITSTUNAIR"), ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn(UiText("P4_HITSTUNMAXGROUND"), ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn(UiText("P4_HITSTUNMAXAIR"), ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableHeadersRow();

				if (ColorHitstunTimers) {

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(UiText("P4_HITSTUNP1C1"));
					ImGui::TableSetColumnIndex(1);
					if (P1C1GroundHitstunTimer > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P1C1GroundHitstunTimer > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P1C1GroundHitstunTimer);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(2);
					if (P1C1AirHitstunTimer > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P1C1AirHitstunTimer > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P1C1AirHitstunTimer);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(3);
					if (P1C1GroundHitstunTimerMAX > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P1C1GroundHitstunTimerMAX > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P1C1GroundHitstunTimerMAX);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(4);
					if (P1C1AirHitstunTimerMAX > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P1C1AirHitstunTimerMAX > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P1C1AirHitstunTimerMAX);
					ImGui::PopStyleColor();


					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(UiText("P4_HITSTUNP1C2"));
					ImGui::TableSetColumnIndex(1);
					if (P1C2GroundHitstunTimer > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P1C2GroundHitstunTimer > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P1C2GroundHitstunTimer);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(2);
					if (P1C2AirHitstunTimer > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P1C2AirHitstunTimer > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P1C2AirHitstunTimer);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(3);
					if (P1C2GroundHitstunTimerMAX > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P1C2GroundHitstunTimerMAX > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P1C2GroundHitstunTimerMAX);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(4);
					if (P1C2AirHitstunTimerMAX > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P1C2AirHitstunTimerMAX > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P1C2AirHitstunTimerMAX);
					ImGui::PopStyleColor();

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(UiText("P4_HITSTUNP1C3"));
					ImGui::TableSetColumnIndex(1);
					if (P1C3GroundHitstunTimer > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P1C3GroundHitstunTimer > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P1C3GroundHitstunTimer);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(2);
					if (P1C3AirHitstunTimer > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P1C3AirHitstunTimer > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P1C3AirHitstunTimer);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(3);
					if (P1C3GroundHitstunTimerMAX > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P1C3GroundHitstunTimerMAX > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P1C3GroundHitstunTimerMAX);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(4);
					if (P1C3AirHitstunTimerMAX > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P1C3AirHitstunTimerMAX > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P1C3AirHitstunTimerMAX);
					ImGui::PopStyleColor();

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(UiText("P4_HITSTUNP2C1"));
					ImGui::TableSetColumnIndex(1);
					if (P2C1GroundHitstunTimer > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P2C1GroundHitstunTimer > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P2C1GroundHitstunTimer);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(2);
					if (P2C1AirHitstunTimer > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P2C1AirHitstunTimer > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P2C1AirHitstunTimer);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(3);
					if (P2C1GroundHitstunTimerMAX > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P2C1GroundHitstunTimerMAX > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P2C1GroundHitstunTimerMAX);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(4);
					if (P2C1AirHitstunTimerMAX > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P2C1AirHitstunTimerMAX > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P2C1AirHitstunTimerMAX);
					ImGui::PopStyleColor();

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(UiText("P4_HITSTUNP2C2"));
					ImGui::TableSetColumnIndex(1);
					if (P2C2GroundHitstunTimer > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P2C2GroundHitstunTimer > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P2C2GroundHitstunTimer);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(2);
					if (P2C2AirHitstunTimer > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P2C2AirHitstunTimer > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P2C2AirHitstunTimer);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(3);
					if (P2C2GroundHitstunTimerMAX > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P2C2GroundHitstunTimerMAX > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P2C2GroundHitstunTimerMAX);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(4);
					if (P2C2AirHitstunTimerMAX > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P2C2AirHitstunTimerMAX > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P2C2AirHitstunTimerMAX);
					ImGui::PopStyleColor();

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(UiText("P4_HITSTUNP2C3"));
					ImGui::TableSetColumnIndex(1);
					if (P2C3GroundHitstunTimer > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P2C3GroundHitstunTimer > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P2C3GroundHitstunTimer);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(2);
					if (P2C3AirHitstunTimer > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P2C3AirHitstunTimer > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P2C3AirHitstunTimer);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(3);
					if (P2C3GroundHitstunTimerMAX > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P2C3GroundHitstunTimerMAX > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P2C3GroundHitstunTimerMAX);
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(4);
					if (P2C3AirHitstunTimerMAX > 300) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 150, 255)); }
					else if (P2C3AirHitstunTimerMAX > 175) { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255)); }
					else { ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); }
					ImGui::Text("%f", P2C3AirHitstunTimerMAX);
					ImGui::PopStyleColor();

				}
				else
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(UiText("P4_HITSTUNP1C1"));
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%f", P1C1GroundHitstunTimer);
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%f", P1C1AirHitstunTimer);
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%f", P1C1GroundHitstunTimerMAX);
					ImGui::TableSetColumnIndex(4);
					ImGui::Text("%f", P1C1AirHitstunTimerMAX);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(UiText("P4_HITSTUNP1C2"));
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%f", P1C2GroundHitstunTimer);
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%f", P1C2AirHitstunTimer);
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%f", P1C2GroundHitstunTimerMAX);
					ImGui::TableSetColumnIndex(4);
					ImGui::Text("%f", P1C2AirHitstunTimerMAX);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(UiText("P4_HITSTUNP1C3"));
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%f", P1C3GroundHitstunTimer);
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%f", P1C3AirHitstunTimer);
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%f", P1C3GroundHitstunTimerMAX);
					ImGui::TableSetColumnIndex(4);
					ImGui::Text("%f", P1C3AirHitstunTimerMAX);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(UiText("P4_HITSTUNP2C1"));
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%f", P2C1GroundHitstunTimer);
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%f", P2C1AirHitstunTimer);
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%f", P2C1GroundHitstunTimerMAX);
					ImGui::TableSetColumnIndex(4);
					ImGui::Text("%f", P2C1AirHitstunTimerMAX);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(UiText("P4_HITSTUNP2C2"));
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%f", P2C2GroundHitstunTimer);
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%f", P2C2AirHitstunTimer);
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%f", P2C2GroundHitstunTimerMAX);
					ImGui::TableSetColumnIndex(4);
					ImGui::Text("%f", P2C2AirHitstunTimerMAX);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(UiText("P4_HITSTUNP2C3"));
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%f", P2C3GroundHitstunTimer);
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%f", P2C3AirHitstunTimer);
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%f", P2C3GroundHitstunTimerMAX);
					ImGui::TableSetColumnIndex(4);
					ImGui::Text("%f", P2C3AirHitstunTimerMAX);

				}
				ImGui::EndTable();
			}

			ImGui::PopFont();

#pragma endregion
			ImGui::TreePop();
		}
#ifdef _DEBUG
		if (ImGui::Checkbox(UiText("P4_COLORHITSTUNTEXT"), &ColorHitstunTimers))
		{

		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
			ImGui::SetTooltip("%s", UiText("P4_COLORHITSTUNTEXTD"));
#endif


		ImGui::EndTabItem();
	}
}

//For Recording And Playback Stuff.
static void TheRecordPlaybackTab()
{
	if (ImGui::BeginTabItem(UiLabel("P6_RECORDPLAYBACK").c_str()))
	{
		ImGui::TextUnformatted(UiText("P5_INCOMINGW"));

		switch (RecordingSlot)
		{

		case 2:

			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_BOTH"))) {
					if (CheckTheMode() == true)
					{
						//recording = true;
						//replaying = false;
						recordingP1 = true;
						replayingP1 = false;
						recordingP2 = true;
						replayingP2 = false;
						recordReplayIndex2P1 = 0;
						recordReplayIndex2P2 = 0;

					}
				}
			}
			//ImGui::SameLine();
			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_P1"))) {
					if (CheckTheMode() == true)
					{
						recordingP1 = true;
						replayingP1 = false;
						//recording = false;
						//replaying = false;
						recordingP2 = false;
						replayingP2 = false;
						recordReplayIndex2P1 = 0;
					}
				}
				ImGui::SameLine();
			}
			//ImGui::SameLine();
			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_P2"))) {
					if (CheckTheMode() == true)
					{
						recordingP2 = true;
						replayingP2 = false;
						//recording = false;
						//replaying = false;
						recordingP1 = false;
						replayingP1 = false;
						recordReplayIndex2P2 = 0;
					}
				}
			}

			else if (recordReplayIndex2P1 > 90 || recordReplayIndex2P2 > 90) {
				if (ImGui::Button(UiText("REC_STOP"))) {
					if (CheckTheMode() == true)
					{
						//recording = false;
						recordingP1 = false;
						recordingP2 = false;
						recordedLength2P1 = recordReplayIndex2P1 - 1;
						recordedLength2P2 = recordReplayIndex2P2 - 1;
					}
				}
			}

			//Playback Code Below.
			ImGui::Separator();

			if (replayAvailable2P1 && replayAvailable2P2 && !recordingP1 && !recordingP2) {
				if (!replayingP1 && !replayingP2) {
					if (ImGui::Button(UiText("REC_PLAY_BOTH"))) {
						if (CheckTheMode() == true)
						{

							replayingP1 = true;
							replayingP2 = true;
							recordReplayIndex2 = 0;
							recordReplayIndex2P1 = 0;
							recordReplayIndex2P2 = 0;

						}

					}
				}
				else if (recordReplayIndex2P1 > 90 || recordReplayIndex2P2 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP")))
					{
						if (CheckTheMode() == true)
						{
							replayingP1 = false;
							replayingP2 = false;

						}
					}
				}
			}

			if (replayAvailable2P1 && !recordingP1) {
				if (!replayingP1) {
					if (ImGui::Button(UiText("REC_PLAY_P1"))) {
						if (CheckTheMode() == true)
						{
							replayingP1 = true;
							recordReplayIndex2P1 = 0;
						}

					}
				}
				else if (recordReplayIndex2P1 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP_P1")))
					{
						if (CheckTheMode() == true)
						{
							replayingP1 = false;
						}
					}
				}
				ImGui::SameLine();
			}

			if (replayAvailable2P2 && !recordingP2) {
				if (!replayingP2) {
					if (ImGui::Button(UiText("REC_PLAY_P2"))) {
						if (CheckTheMode() == true)
						{
							replayingP2 = true;
							recordReplayIndex2P2 = 0;
						}

					}
				}
				else if (recordReplayIndex2P2 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP_P2")))
					{
						if (CheckTheMode() == true)
						{
							replayingP2 = false;
						}
					}
				}
			}


			ImGui::Separator();

			//Saves Recordings. Needs to be adjusted.
			if (ImGui::Button(UiText("REC_SAVE_P1")))
			{
				if (CheckTheMode() == true)
				{
					if (!recording && replayAvailable2P1)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };

						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = "dat\0"; // extra '\0' for lpstrFilter

						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;
						if (GetSaveFileName(&ofn))
						{
							pRec = fopen(ofn.lpstrFile, "wb");
							fwrite(replayBuffer3, 550, recordedLength2P1, pRec);
							fclose(pRec);
						}
					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button(UiText("REC_SAVE_P2")))
			{
				if (CheckTheMode() == true)
				{
					if (!recording && replayAvailable2P2)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };

						char szFile[_MAX_PATH] = "RecP2";
						const char szExt[] = "dat\0"; // extra '\0' for lpstrFilter

						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetSaveFileName(&ofn))
						{
							pRec = fopen(ofn.lpstrFile, "wb");
							fwrite(replayBuffer4, 550, recordedLength2P2, pRec);
							fclose(pRec);

						}
					}
				}
			}

			if (ImGui::Button(UiText("REC_LOAD_P1")))
			{
				if (CheckTheMode() == true)
				{
					if (!recordingP1 && !replayingP1)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };
						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = ".dat\0"; // extra '\0' for lpstrFilter
						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetOpenFileName(&ofn))
						{
							if (ofn.lStructSize > (ReplayBufferSize * ReplayLength))
							{
								MessageBoxW(hWnd, Localizer::UiWideText("REC_TOO_LARGE").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);

							}
							else
							{
								//MessageBoxA(0,ofn.lpstrFile,0,MB_ICONINFORMATION);
								std::ifstream mRP(ofn.lpstrFile, std::ios::binary | std::ios::ate);
								mRP.unsetf(std::ios::skipws);
								std::ifstream::pos_type pos = mRP.tellg();
								int filesize = pos;

								if (filesize % 550 != 0)
								{
									MessageBoxW(hWnd, Localizer::UiWideText("REC_INVALID").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);
								}
								else
								{
									//Gets the file size, sets the needed variables to play the replay when loaded, 
									//and then copies the file into an array in memory.
									int length = pos / 550;
									replayAvailable2P1 = true;
									recordedLength2P1 = length;
									recordReplayIndex2P1 = 0;

									mRP.seekg(0, std::ios::beg);

									std::vector<BYTE> pChars;
									pChars.reserve(filesize);

									pChars.insert(pChars.begin(),
										std::istream_iterator<BYTE>(mRP),
										std::istream_iterator<BYTE>());

									//For loop for inserting the frames in the proper place. Based on code from recording.
									//Need to fix this.
									for (int i = 0; i < recordedLength2P1; i++)
									{
										memcpy(&replayBuffer3[i], &pChars[(i * 550)], ReplayBufferSize);
									}
								}


							}

						}


					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button(UiText("REC_LOAD_P2")))
			{
				if (CheckTheMode() == true)
				{
					if (!recordingP2 && !replayingP2)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };
						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = ".dat\0"; // extra '\0' for lpstrFilter
						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetOpenFileName(&ofn))
						{
							if (ofn.lStructSize > (ReplayBufferSize * ReplayLength))
							{
								MessageBoxW(hWnd, Localizer::UiWideText("REC_TOO_LARGE").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);

							}
							else
							{
								//MessageBoxA(0,ofn.lpstrFile,0,MB_ICONINFORMATION);
								std::ifstream mRP(ofn.lpstrFile, std::ios::binary | std::ios::ate);
								mRP.unsetf(std::ios::skipws);
								std::ifstream::pos_type pos = mRP.tellg();
								int filesize = pos;

								if (filesize % 550 != 0)
								{
									MessageBoxW(hWnd, Localizer::UiWideText("REC_INVALID").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);
								}
								else
								{
									//Gets the file size, sets the needed variables to play the replay when loaded, 
									//and then copies the file into an array in memory.
									int length = pos / 550;
									replayAvailable2P2 = true;
									recordedLength2P2 = length;
									recordReplayIndex2P2 = 0;

									mRP.seekg(0, std::ios::beg);

									std::vector<BYTE> pChars;
									pChars.reserve(filesize);

									pChars.insert(pChars.begin(),
										std::istream_iterator<BYTE>(mRP),
										std::istream_iterator<BYTE>());

									//For loop for inserting the frames in the proper place. Based on code from recording.
									for (int i = 0; i < recordedLength2P2; i++)
									{
										memcpy(&replayBuffer4[i], &pChars[(i * 550)], ReplayBufferSize);
									}
								}


							}

						}


					}
				}
			}


			break;

		case 3:

			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_BOTH"))) {
					if (CheckTheMode() == true)
					{
						//recording = true;
						//replaying = false;
						recordingP1 = true;
						replayingP1 = false;
						recordingP2 = true;
						replayingP2 = false;
						recordReplayIndex3P1 = 0;
						recordReplayIndex3P2 = 0;

					}
				}
			}
			//ImGui::SameLine();
			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_P1"))) {
					if (CheckTheMode() == true)
					{
						recordingP1 = true;
						replayingP1 = false;
						//recording = false;
						//replaying = false;
						recordingP2 = false;
						replayingP2 = false;
						recordReplayIndex3P1 = 0;
					}
				}
				ImGui::SameLine();
			}
			//ImGui::SameLine();
			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_P2"))) {
					if (CheckTheMode() == true)
					{
						recordingP2 = true;
						replayingP2 = false;
						//recording = false;
						//replaying = false;
						recordingP1 = false;
						replayingP1 = false;
						recordReplayIndex3P2 = 0;
					}
				}
			}

			else if (recordReplayIndex3P1 > 90 || recordReplayIndex3P2 > 90) {
				if (ImGui::Button(UiText("REC_STOP"))) {
					if (CheckTheMode() == true)
					{
						//recording = false;
						recordingP1 = false;
						recordingP2 = false;
						recordedLength3P1 = recordReplayIndex3P1 - 1;
						recordedLength3P2 = recordReplayIndex3P2 - 1;
					}
				}
			}

			//Playback Code Below.
			ImGui::Separator();

			if (replayAvailable3P1 && replayAvailable3P2 && !recordingP1 && !recordingP2) {
				if (!replayingP1 && !replayingP2) {
					if (ImGui::Button(UiText("REC_PLAY_BOTH"))) {
						if (CheckTheMode() == true)
						{

							replayingP1 = true;
							replayingP2 = true;
							recordReplayIndex3 = 0;
							recordReplayIndex3P1 = 0;
							recordReplayIndex3P2 = 0;

						}

					}
				}
				else if (recordReplayIndex3P1 > 90 || recordReplayIndex3P2 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP")))
					{
						if (CheckTheMode() == true)
						{
							replayingP1 = false;
							replayingP2 = false;

						}
					}
				}
			}

			if (replayAvailable3P1 && !recordingP1) {
				if (!replayingP1) {
					if (ImGui::Button(UiText("REC_PLAY_P1"))) {
						if (CheckTheMode() == true)
						{
							replayingP1 = true;
							recordReplayIndex3P1 = 0;
						}

					}
				}
				else if (recordReplayIndex3P1 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP_P1")))
					{
						if (CheckTheMode() == true)
						{
							replayingP1 = false;
						}
					}
				}
				ImGui::SameLine();
			}

			if (replayAvailable3P2 && !recordingP2) {
				if (!replayingP2) {
					if (ImGui::Button(UiText("REC_PLAY_P2"))) {
						if (CheckTheMode() == true)
						{
							replayingP2 = true;
							recordReplayIndex3P2 = 0;
						}

					}
				}
				else if (recordReplayIndex3P2 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP_P2")))
					{
						if (CheckTheMode() == true)
						{
							replayingP2 = false;
						}
					}
				}
			}


			ImGui::Separator();

			//Saves Recordings. Needs to be adjusted.
			if (ImGui::Button(UiText("REC_SAVE_P1")))
			{
				if (CheckTheMode() == true)
				{
					if (!recording && replayAvailable3P1)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };

						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = "dat\0"; // extra '\0' for lpstrFilter

						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;
						if (GetSaveFileName(&ofn))
						{
							pRec = fopen(ofn.lpstrFile, "wb");
							fwrite(replayBuffer5, 550, recordedLength3P1, pRec);
							fclose(pRec);
						}
					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button(UiText("REC_SAVE_P2")))
			{
				if (CheckTheMode() == true)
				{
					if (!recording && replayAvailable3P2)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };

						char szFile[_MAX_PATH] = "RecP2";
						const char szExt[] = "dat\0"; // extra '\0' for lpstrFilter

						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetSaveFileName(&ofn))
						{
							pRec = fopen(ofn.lpstrFile, "wb");
							fwrite(replayBuffer6, 550, recordedLength3P2, pRec);
							fclose(pRec);

						}
					}
				}
			}

			if (ImGui::Button(UiText("REC_LOAD_P1")))
			{
				if (CheckTheMode() == true)
				{
					if (!recordingP1 && !replayingP1)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };
						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = ".dat\0"; // extra '\0' for lpstrFilter
						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetOpenFileName(&ofn))
						{
							if (ofn.lStructSize > (ReplayBufferSize * ReplayLength))
							{
								MessageBoxW(hWnd, Localizer::UiWideText("REC_TOO_LARGE").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);

							}
							else
							{
								//MessageBoxA(0,ofn.lpstrFile,0,MB_ICONINFORMATION);
								std::ifstream mRP(ofn.lpstrFile, std::ios::binary | std::ios::ate);
								mRP.unsetf(std::ios::skipws);
								std::ifstream::pos_type pos = mRP.tellg();
								int filesize = pos;

								if (filesize % 550 != 0)
								{
									MessageBoxW(hWnd, Localizer::UiWideText("REC_INVALID").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);
								}
								else
								{
									//Gets the file size, sets the needed variables to play the replay when loaded, 
									//and then copies the file into an array in memory.
									int length = pos / 550;
									replayAvailable3P1 = true;
									recordedLength3P1 = length;
									recordReplayIndex3P1 = 0;

									mRP.seekg(0, std::ios::beg);

									std::vector<BYTE> pChars;
									pChars.reserve(filesize);

									pChars.insert(pChars.begin(),
										std::istream_iterator<BYTE>(mRP),
										std::istream_iterator<BYTE>());

									//For loop for inserting the frames in the proper place. Based on code from recording.
									//Need to fix this.
									for (int i = 0; i < recordedLength3P1; i++)
									{
										memcpy(&replayBuffer5[i], &pChars[(i * 550)], ReplayBufferSize);
									}
								}


							}

						}


					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button(UiText("REC_LOAD_P2")))
			{
				if (CheckTheMode() == true)
				{
					if (!recordingP2 && !replayingP2)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };
						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = ".dat\0"; // extra '\0' for lpstrFilter
						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetOpenFileName(&ofn))
						{
							if (ofn.lStructSize > (ReplayBufferSize * ReplayLength))
							{
								MessageBoxW(hWnd, Localizer::UiWideText("REC_TOO_LARGE").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);

							}
							else
							{
								//MessageBoxA(0,ofn.lpstrFile,0,MB_ICONINFORMATION);
								std::ifstream mRP(ofn.lpstrFile, std::ios::binary | std::ios::ate);
								mRP.unsetf(std::ios::skipws);
								std::ifstream::pos_type pos = mRP.tellg();
								int filesize = pos;

								if (filesize % 550 != 0)
								{
									MessageBoxW(hWnd, Localizer::UiWideText("REC_INVALID").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);
								}
								else
								{
									//Gets the file size, sets the needed variables to play the replay when loaded, 
									//and then copies the file into an array in memory.
									int length = pos / 550;
									replayAvailable3P2 = true;
									recordedLength3P2 = length;
									recordReplayIndex3P2 = 0;

									mRP.seekg(0, std::ios::beg);

									std::vector<BYTE> pChars;
									pChars.reserve(filesize);

									pChars.insert(pChars.begin(),
										std::istream_iterator<BYTE>(mRP),
										std::istream_iterator<BYTE>());

									//For loop for inserting the frames in the proper place. Based on code from recording.
									for (int i = 0; i < recordedLength3P2; i++)
									{
										memcpy(&replayBuffer6[i], &pChars[(i * 550)], ReplayBufferSize);
									}
								}


							}

						}


					}
				}
			}


			break;

		case 4:

			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_BOTH"))) {
					if (CheckTheMode() == true)
					{
						//recording = true;
						//replaying = false;
						recordingP1 = true;
						replayingP1 = false;
						recordingP2 = true;
						replayingP2 = false;
						recordReplayIndex4P1 = 0;
						recordReplayIndex4P2 = 0;

					}
				}
			}
			//ImGui::SameLine();
			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_P1"))) {
					if (CheckTheMode() == true)
					{
						recordingP1 = true;
						replayingP1 = false;
						//recording = false;
						//replaying = false;
						recordingP2 = false;
						replayingP2 = false;
						recordReplayIndex4P1 = 0;
					}
				}
				ImGui::SameLine();
			}
			//ImGui::SameLine();
			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_P2"))) {
					if (CheckTheMode() == true)
					{
						recordingP2 = true;
						replayingP2 = false;
						//recording = false;
						//replaying = false;
						recordingP1 = false;
						replayingP1 = false;
						recordReplayIndex4P2 = 0;
					}
				}
			}

			else if (recordReplayIndex4P1 > 90 || recordReplayIndex4P2 > 90) {
				if (ImGui::Button(UiText("REC_STOP"))) {
					if (CheckTheMode() == true)
					{
						//recording = false;
						recordingP1 = false;
						recordingP2 = false;
						recordedLength4P1 = recordReplayIndex4P1 - 1;
						recordedLength4P2 = recordReplayIndex4P2 - 1;
					}
				}
			}

			//Playback Code Below.
			ImGui::Separator();

			if (replayAvailable4P1 && replayAvailable4P2 && !recordingP1 && !recordingP2) {
				if (!replayingP1 && !replayingP2) {
					if (ImGui::Button(UiText("REC_PLAY_BOTH"))) {
						if (CheckTheMode() == true)
						{

							replayingP1 = true;
							replayingP2 = true;
							recordReplayIndex4 = 0;
							recordReplayIndex4P1 = 0;
							recordReplayIndex4P2 = 0;

						}

					}
				}
				else if (recordReplayIndex4P1 > 90 || recordReplayIndex4P2 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP")))
					{
						if (CheckTheMode() == true)
						{
							replayingP1 = false;
							replayingP2 = false;

						}
					}
				}
			}

			if (replayAvailable4P1 && !recordingP1) {
				if (!replayingP1) {
					if (ImGui::Button(UiText("REC_PLAY_P1"))) {
						if (CheckTheMode() == true)
						{
							replayingP1 = true;
							recordReplayIndex4P1 = 0;
						}

					}
				}
				else if (recordReplayIndex4P1 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP_P1")))
					{
						if (CheckTheMode() == true)
						{
							replayingP1 = false;
						}
					}
				}
				ImGui::SameLine();
			}

			if (replayAvailable4P2 && !recordingP2) {
				if (!replayingP2) {
					if (ImGui::Button(UiText("REC_PLAY_P2"))) {
						if (CheckTheMode() == true)
						{
							replayingP2 = true;
							recordReplayIndex4P2 = 0;
						}

					}
				}
				else if (recordReplayIndex4P2 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP_P2")))
					{
						if (CheckTheMode() == true)
						{
							replayingP2 = false;
						}
					}
				}
			}


			ImGui::Separator();

			//Saves Recordings. Needs to be adjusted.
			if (ImGui::Button(UiText("REC_SAVE_P1")))
			{
				if (CheckTheMode() == true)
				{
					if (!recording && replayAvailable4P1)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };

						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = "dat\0"; // extra '\0' for lpstrFilter

						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;
						if (GetSaveFileName(&ofn))
						{
							pRec = fopen(ofn.lpstrFile, "wb");
							fwrite(replayBuffer7, 550, recordedLength4P1, pRec);
							fclose(pRec);
						}
					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button(UiText("REC_SAVE_P2")))
			{
				if (CheckTheMode() == true)
				{
					if (!recording && replayAvailable4P2)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };

						char szFile[_MAX_PATH] = "RecP2";
						const char szExt[] = "dat\0"; // extra '\0' for lpstrFilter

						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetSaveFileName(&ofn))
						{
							pRec = fopen(ofn.lpstrFile, "wb");
							fwrite(replayBuffer8, 550, recordedLength4P2, pRec);
							fclose(pRec);

						}
					}
				}
			}

			if (ImGui::Button(UiText("REC_LOAD_P1")))
			{
				if (CheckTheMode() == true)
				{
					if (!recordingP1 && !replayingP1)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };
						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = ".dat\0"; // extra '\0' for lpstrFilter
						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetOpenFileName(&ofn))
						{
							if (ofn.lStructSize > (ReplayBufferSize * ReplayLength))
							{
								MessageBoxW(hWnd, Localizer::UiWideText("REC_TOO_LARGE").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);

							}
							else
							{
								//MessageBoxA(0,ofn.lpstrFile,0,MB_ICONINFORMATION);
								std::ifstream mRP(ofn.lpstrFile, std::ios::binary | std::ios::ate);
								mRP.unsetf(std::ios::skipws);
								std::ifstream::pos_type pos = mRP.tellg();
								int filesize = pos;

								if (filesize % 550 != 0)
								{
									MessageBoxW(hWnd, Localizer::UiWideText("REC_INVALID").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);
								}
								else
								{
									//Gets the file size, sets the needed variables to play the replay when loaded, 
									//and then copies the file into an array in memory.
									int length = pos / 550;
									replayAvailable4P1 = true;
									recordedLength4P1 = length;
									recordReplayIndex4P1 = 0;

									mRP.seekg(0, std::ios::beg);

									std::vector<BYTE> pChars;
									pChars.reserve(filesize);

									pChars.insert(pChars.begin(),
										std::istream_iterator<BYTE>(mRP),
										std::istream_iterator<BYTE>());

									//For loop for inserting the frames in the proper place. Based on code from recording.
									//Need to fix this.
									for (int i = 0; i < recordedLength4P1; i++)
									{
										memcpy(&replayBuffer7[i], &pChars[(i * 550)], ReplayBufferSize);
									}
								}


							}

						}


					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button(UiText("REC_LOAD_P2")))
			{
				if (CheckTheMode() == true)
				{
					if (!recordingP2 && !replayingP2)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };
						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = ".dat\0"; // extra '\0' for lpstrFilter
						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetOpenFileName(&ofn))
						{
							if (ofn.lStructSize > (ReplayBufferSize * ReplayLength))
							{
								MessageBoxW(hWnd, Localizer::UiWideText("REC_TOO_LARGE").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);

							}
							else
							{
								//MessageBoxA(0,ofn.lpstrFile,0,MB_ICONINFORMATION);
								std::ifstream mRP(ofn.lpstrFile, std::ios::binary | std::ios::ate);
								mRP.unsetf(std::ios::skipws);
								std::ifstream::pos_type pos = mRP.tellg();
								int filesize = pos;

								if (filesize % 550 != 0)
								{
									MessageBoxW(hWnd, Localizer::UiWideText("REC_INVALID").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);
								}
								else
								{
									//Gets the file size, sets the needed variables to play the replay when loaded, 
									//and then copies the file into an array in memory.
									int length = pos / 550;
									replayAvailable4P2 = true;
									recordedLength4P2 = length;
									recordReplayIndex4P2 = 0;

									mRP.seekg(0, std::ios::beg);

									std::vector<BYTE> pChars;
									pChars.reserve(filesize);

									pChars.insert(pChars.begin(),
										std::istream_iterator<BYTE>(mRP),
										std::istream_iterator<BYTE>());

									//For loop for inserting the frames in the proper place. Based on code from recording.
									for (int i = 0; i < recordedLength4P2; i++)
									{
										memcpy(&replayBuffer8[i], &pChars[(i * 550)], ReplayBufferSize);
									}
								}


							}

						}


					}
				}
			}


			break;

		case 5:

			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_BOTH"))) {
					if (CheckTheMode() == true)
					{
						//recording = true;
						//replaying = false;
						recordingP1 = true;
						replayingP1 = false;
						recordingP2 = true;
						replayingP2 = false;
						recordReplayIndex5P1 = 0;
						recordReplayIndex5P2 = 0;

					}
				}
			}
			//ImGui::SameLine();
			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_P1"))) {
					if (CheckTheMode() == true)
					{
						recordingP1 = true;
						replayingP1 = false;
						//recording = false;
						//replaying = false;
						recordingP2 = false;
						replayingP2 = false;
						recordReplayIndex5P1 = 0;
					}
				}
				ImGui::SameLine();
			}
			//ImGui::SameLine();
			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_P2"))) {
					if (CheckTheMode() == true)
					{
						recordingP2 = true;
						replayingP2 = false;
						//recording = false;
						//replaying = false;
						recordingP1 = false;
						replayingP1 = false;
						recordReplayIndex5P2 = 0;
					}
				}
			}

			else if (recordReplayIndex5P1 > 90 || recordReplayIndex5P2 > 90) {
				if (ImGui::Button(UiText("REC_STOP"))) {
					if (CheckTheMode() == true)
					{
						//recording = false;
						recordingP1 = false;
						recordingP2 = false;
						recordedLength5P1 = recordReplayIndex5P1 - 1;
						recordedLength5P2 = recordReplayIndex5P2 - 1;
					}
				}
			}

			//Playback Code Below.
			ImGui::Separator();

			if (replayAvailable5P1 && replayAvailable5P2 && !recordingP1 && !recordingP2) {
				if (!replayingP1 && !replayingP2) {
					if (ImGui::Button(UiText("REC_PLAY_BOTH"))) {
						if (CheckTheMode() == true)
						{

							replayingP1 = true;
							replayingP2 = true;
							recordReplayIndex5 = 0;
							recordReplayIndex5P1 = 0;
							recordReplayIndex5P2 = 0;

						}

					}
				}
				else if (recordReplayIndex5P1 > 90 || recordReplayIndex5P2 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP")))
					{
						if (CheckTheMode() == true)
						{
							replayingP1 = false;
							replayingP2 = false;

						}
					}
				}
			}

			if (replayAvailable5P1 && !recordingP1) {
				if (!replayingP1) {
					if (ImGui::Button(UiText("REC_PLAY_P1"))) {
						if (CheckTheMode() == true)
						{
							replayingP1 = true;
							recordReplayIndex5P1 = 0;
						}

					}
				}
				else if (recordReplayIndex5P1 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP_P1")))
					{
						if (CheckTheMode() == true)
						{
							replayingP1 = false;
						}
					}
				}
				ImGui::SameLine();
			}

			if (replayAvailable5P2 && !recordingP2) {
				if (!replayingP2) {
					if (ImGui::Button(UiText("REC_PLAY_P2"))) {
						if (CheckTheMode() == true)
						{
							replayingP2 = true;
							recordReplayIndex5P2 = 0;
						}

					}
				}
				else if (recordReplayIndex5P2 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP_P2")))
					{
						if (CheckTheMode() == true)
						{
							replayingP2 = false;
						}
					}
				}
			}


			ImGui::Separator();

			//Saves Recordings. Needs to be adjusted.
			if (ImGui::Button(UiText("REC_SAVE_P1")))
			{
				if (CheckTheMode() == true)
				{
					if (!recording && replayAvailable5P1)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };

						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = "dat\0"; // extra '\0' for lpstrFilter

						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;
						if (GetSaveFileName(&ofn))
						{
							pRec = fopen(ofn.lpstrFile, "wb");
							fwrite(replayBuffer9, 550, recordedLength5P1, pRec);
							fclose(pRec);
						}
					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button(UiText("REC_SAVE_P2")))
			{
				if (CheckTheMode() == true)
				{
					if (!recording && replayAvailable5P2)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };

						char szFile[_MAX_PATH] = "RecP2";
						const char szExt[] = "dat\0"; // extra '\0' for lpstrFilter

						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetSaveFileName(&ofn))
						{
							pRec = fopen(ofn.lpstrFile, "wb");
							fwrite(replayBuffer10, 550, recordedLength5P2, pRec);
							fclose(pRec);

						}
					}
				}
			}

			if (ImGui::Button(UiText("REC_LOAD_P1")))
			{
				if (CheckTheMode() == true)
				{
					if (!recordingP1 && !replayingP1)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };
						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = ".dat\0"; // extra '\0' for lpstrFilter
						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetOpenFileName(&ofn))
						{
							if (ofn.lStructSize > (ReplayBufferSize * ReplayLength))
							{
								MessageBoxW(hWnd, Localizer::UiWideText("REC_TOO_LARGE").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);

							}
							else
							{
								//MessageBoxA(0,ofn.lpstrFile,0,MB_ICONINFORMATION);
								std::ifstream mRP(ofn.lpstrFile, std::ios::binary | std::ios::ate);
								mRP.unsetf(std::ios::skipws);
								std::ifstream::pos_type pos = mRP.tellg();
								int filesize = pos;

								if (filesize % 550 != 0)
								{
									MessageBoxW(hWnd, Localizer::UiWideText("REC_INVALID").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);
								}
								else
								{
									//Gets the file size, sets the needed variables to play the replay when loaded, 
									//and then copies the file into an array in memory.
									int length = pos / 550;
									replayAvailable5P1 = true;
									recordedLength5P1 = length;
									recordReplayIndex5P1 = 0;

									mRP.seekg(0, std::ios::beg);

									std::vector<BYTE> pChars;
									pChars.reserve(filesize);

									pChars.insert(pChars.begin(),
										std::istream_iterator<BYTE>(mRP),
										std::istream_iterator<BYTE>());

									//For loop for inserting the frames in the proper place. Based on code from recording.
									//Need to fix this.
									for (int i = 0; i < recordedLength5P1; i++)
									{
										memcpy(&replayBuffer9[i], &pChars[(i * 550)], ReplayBufferSize);
									}
								}


							}

						}


					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button(UiText("REC_LOAD_P2")))
			{
				if (CheckTheMode() == true)
				{
					if (!recordingP2 && !replayingP2)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };
						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = ".dat\0"; // extra '\0' for lpstrFilter
						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetOpenFileName(&ofn))
						{
							if (ofn.lStructSize > (ReplayBufferSize * ReplayLength))
							{
								MessageBoxW(hWnd, Localizer::UiWideText("REC_TOO_LARGE").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);

							}
							else
							{
								//MessageBoxA(0,ofn.lpstrFile,0,MB_ICONINFORMATION);
								std::ifstream mRP(ofn.lpstrFile, std::ios::binary | std::ios::ate);
								mRP.unsetf(std::ios::skipws);
								std::ifstream::pos_type pos = mRP.tellg();
								int filesize = pos;

								if (filesize % 550 != 0)
								{
									MessageBoxW(hWnd, Localizer::UiWideText("REC_INVALID").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);
								}
								else
								{
									//Gets the file size, sets the needed variables to play the replay when loaded, 
									//and then copies the file into an array in memory.
									int length = pos / 550;
									replayAvailable5P2 = true;
									recordedLength5P2 = length;
									recordReplayIndex5P2 = 0;

									mRP.seekg(0, std::ios::beg);

									std::vector<BYTE> pChars;
									pChars.reserve(filesize);

									pChars.insert(pChars.begin(),
										std::istream_iterator<BYTE>(mRP),
										std::istream_iterator<BYTE>());

									//For loop for inserting the frames in the proper place. Based on code from recording.
									for (int i = 0; i < recordedLength5P2; i++)
									{
										memcpy(&replayBuffer10[i], &pChars[(i * 550)], ReplayBufferSize);
									}
								}


							}

						}


					}
				}
			}

			break;

		case 1:
		default:

			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_BOTH"))) {
					if (CheckTheMode() == true)
					{
						//recording = true;
						//replaying = false;
						recordingP1 = true;
						replayingP1 = false;
						recordingP2 = true;
						replayingP2 = false;
						//recordReplayIndex = 0;
						recordReplayIndexP1 = 0;
						recordReplayIndexP2 = 0;

					}
				}
			}
			//ImGui::SameLine();
			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_P1"))) {
					if (CheckTheMode() == true)
					{
						recordingP1 = true;
						replayingP1 = false;
						//recording = false;
						//replaying = false;
						recordingP2 = false;
						replayingP2 = false;
						recordReplayIndexP1 = 0;
					}
				}
				ImGui::SameLine();
			}
			//ImGui::SameLine();
			if (!recordingP1 && !recordingP2) {
				if (ImGui::Button(UiText("REC_P2"))) {
					if (CheckTheMode() == true)
					{
						recordingP2 = true;
						replayingP2 = false;
						//recording = false;
						//replaying = false;
						recordingP1 = false;
						replayingP1 = false;
						recordReplayIndexP2 = 0;
					}
				}
			}

			else if (recordReplayIndexP1 > 90 || recordReplayIndexP2 > 90) {
				if (ImGui::Button(UiText("REC_STOP"))) {
					if (CheckTheMode() == true)
					{
						//recording = false;
						recordingP1 = false;
						recordingP2 = false;
						recordedLengthP1 = recordReplayIndexP1 - 1;
						recordedLengthP2 = recordReplayIndexP2 - 1;
					}
				}
			}

			//Playback Code Below.
			ImGui::Separator();

			if (replayAvailableP1 && replayAvailableP2 && !recordingP1 && !recordingP2) {
				if (!replayingP1 && !replayingP2) {
					if (ImGui::Button(UiText("REC_PLAY_BOTH"))) {
						if (CheckTheMode() == true)
						{

							replayingP1 = true;
							replayingP2 = true;
							recordReplayIndex = 0;
							recordReplayIndexP1 = 0;
							recordReplayIndexP2 = 0;

						}

					}
				}
				else if (recordReplayIndexP1 > 90 || recordReplayIndexP2 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP")))
					{
						if (CheckTheMode() == true)
						{
							replayingP1 = false;
							replayingP2 = false;

						}
					}
				}
			}

			if (replayAvailableP1 && !recordingP1) {
				if (!replayingP1) {
					if (ImGui::Button(UiText("REC_PLAY_P1"))) {
						if (CheckTheMode() == true)
						{
							replayingP1 = true;
							recordReplayIndexP1 = 0;
						}

					}
				}
				else if (recordReplayIndexP1 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP_P1")))
					{
						if (CheckTheMode() == true)
						{
							replayingP1 = false;
						}
					}
				}
				ImGui::SameLine();
			}

			if (replayAvailableP2 && !recordingP2) {
				if (!replayingP2) {
					if (ImGui::Button(UiText("REC_PLAY_P2"))) {
						if (CheckTheMode() == true)
						{
							replayingP2 = true;
							recordReplayIndexP2 = 0;
						}

					}
				}
				else if (recordReplayIndexP2 > 90) {
					if (ImGui::Button(UiText("REC_PLAY_STOP_P2")))
					{
						if (CheckTheMode() == true)
						{
							replayingP2 = false;
						}
					}
				}
			}


			ImGui::Separator();

			//Saves Recordings. Needs to be adjusted.
			if (ImGui::Button(UiText("REC_SAVE_P1")))
			{
				if (CheckTheMode() == true)
				{
					if (!recording && replayAvailableP1)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };

						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = "dat\0"; // extra '\0' for lpstrFilter

						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;
						if (GetSaveFileName(&ofn))
						{
							pRec = fopen(ofn.lpstrFile, "wb");
							fwrite(replayBuffer, 550, recordedLengthP1, pRec);
							fclose(pRec);
						}
					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button(UiText("REC_SAVE_P2")))
			{
				if (CheckTheMode() == true)
				{
					if (!recording && replayAvailableP2)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };

						char szFile[_MAX_PATH] = "RecP2";
						const char szExt[] = "dat\0"; // extra '\0' for lpstrFilter

						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetSaveFileName(&ofn))
						{
							pRec = fopen(ofn.lpstrFile, "wb");
							fwrite(replayBuffer2, 550, recordedLengthP2, pRec);
							fclose(pRec);

						}
					}
				}
			}

			//Loads Recordings. To Be Continued....
			if (ImGui::Button(UiText("REC_LOAD_P1")))
			{
				if (CheckTheMode() == true)
				{
					if (!recordingP1 && !replayingP1)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };
						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = ".dat\0"; // extra '\0' for lpstrFilter
						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetOpenFileName(&ofn))
						{
							if (ofn.lStructSize > (ReplayBufferSize * ReplayLength))
							{
								MessageBoxW(hWnd, Localizer::UiWideText("REC_TOO_LARGE").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);

							}
							else
							{
								//MessageBoxA(0,ofn.lpstrFile,0,MB_ICONINFORMATION);
								std::ifstream mRP(ofn.lpstrFile, std::ios::binary | std::ios::ate);
								mRP.unsetf(std::ios::skipws);
								std::ifstream::pos_type pos = mRP.tellg();
								int filesize = pos;

								if (filesize % 550 != 0)
								{
									MessageBoxW(hWnd, Localizer::UiWideText("REC_INVALID").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);
								}
								else
								{
									//Gets the file size, sets the needed variables to play the replay when loaded, 
									//and then copies the file into an array in memory.
									int length = pos / 550;
									replayAvailableP1 = true;
									recordedLengthP1 = length;
									recordReplayIndexP1 = 0;

									mRP.seekg(0, std::ios::beg);

									std::vector<BYTE> pChars;
									pChars.reserve(filesize);

									pChars.insert(pChars.begin(),
										std::istream_iterator<BYTE>(mRP),
										std::istream_iterator<BYTE>());

									//For loop for inserting the frames in the proper place. Based on code from recording.
									//Need to fix this.
									for (int i = 0; i < recordedLengthP1; i++)
									{

										memcpy(&replayBuffer[i], &pChars[(i * 550)], ReplayBufferSize);
									}
								}


							}

						}


					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button(UiText("REC_LOAD_P2")))
			{
				if (CheckTheMode() == true)
				{
					if (!recordingP2 && !replayingP2)
					{
						OPENFILENAME ofn = { sizeof(OPENFILENAME) };
						char szFile[_MAX_PATH] = "RecP1";
						const char szExt[] = ".dat\0"; // extra '\0' for lpstrFilter
						ofn.hwndOwner = GetConsoleWindow();
						ofn.lpstrFile = szFile; // <--------------------- initial file name
						ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
						ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
						ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

						if (GetOpenFileName(&ofn))
						{
							if (ofn.lStructSize > (ReplayBufferSize * ReplayLength))
							{
								MessageBoxW(hWnd, Localizer::UiWideText("REC_TOO_LARGE").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);

							}
							else
							{
								//MessageBoxA(0,ofn.lpstrFile,0,MB_ICONINFORMATION);
								std::ifstream mRP(ofn.lpstrFile, std::ios::binary | std::ios::ate);
								mRP.unsetf(std::ios::skipws);
								std::ifstream::pos_type pos = mRP.tellg();
								int filesize = pos;

								if (filesize % 550 != 0)
								{
									MessageBoxW(hWnd, Localizer::UiWideText("REC_INVALID").c_str(), L"UMVC3 Training Tools", MB_ICONINFORMATION);
								}
								else
								{
									//Gets the file size, sets the needed variables to play the replay when loaded, 
									//and then copies the file into an array in memory.
									int length = pos / 550;
									replayAvailableP2 = true;
									recordedLengthP2 = length;
									recordReplayIndexP2 = 0;

									mRP.seekg(0, std::ios::beg);

									std::vector<BYTE> pChars;
									pChars.reserve(filesize);

									pChars.insert(pChars.begin(),
										std::istream_iterator<BYTE>(mRP),
										std::istream_iterator<BYTE>());

									//For loop for inserting the frames in the proper place. Based on code from recording.
									for (int i = 0; i < recordedLengthP2; i++)
									{

										memcpy(&replayBuffer2[i], &pChars[(i * 550)], ReplayBufferSize);
									}
								}


							}

						}


					}
				}
			}

			break;
		}

		if (RecordingSlot)
		{
			ImGui::TextUnformatted(UiText("REC_SLOT_TITLE"));
			if (ImGui::SliderInt(UiText("REC_SLOT"), &RecordingSlot, 1, 5))
			{
				if (CheckTheMode() == true)
				{

				}
			}
		}
		ImGui::EndTabItem();
	}
}

DWORD WINAPI MainThread(LPVOID lpReserved)
{
	TrainingTeamEditor::InitializeLog(static_cast<HMODULE>(lpReserved));
	GetASIPath();
	const auto folder = std::filesystem::path(path).parent_path();
	Localizer::Initialize(folder);

	//Waits a hot moment to ensure that the game's own D3D device is active before doing anything.
	Sleep(3000);

	// Starts Constructing a separate window to open. Based on code from deleted account xyzzy on cpluscplus.
	static const TCHAR WindowName_sz[] = TEXT("TT Port Powered by Imgui");
	static const TCHAR WindowTitle_sz[] = TEXT("UMVC3 Training Tools");

	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0, 0, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, WindowName_sz, NULL };

	if (RegisterClassEx(&wc) == 0)
	{
		MessageBoxW(nullptr, Localizer::UiWideText("UI_WINDOW_CLASS").c_str(), L"UMVC3 Training Tools", MB_OK | MB_ICONERROR);
		return E_FAIL;
	}

	//Note to self: Base Window resolution Can be adjusted here. Also can adjust whether or not Minimize/Maximize/Close buttons appear in 3rd parameter.
	HWND hWnd = CreateWindow(WindowName_sz, WindowTitle_sz, WS_OVERLAPPEDWINDOW, 100, 100, 480, 720, NULL, NULL, wc.hInstance, NULL);

	//This checks that the window was created successfully.
	if (hWnd == NULL)
	{
		MessageBoxW(nullptr, Localizer::UiWideText("UI_WINDOW_CREATE").c_str(), L"UMVC3 Training Tools", MB_OK | MB_ICONERROR);
		return E_FAIL;
	}

	//Time to initialize DirectX9. Returns 1 if it fails.
	if (!CreateD3DDevice(hWnd))
	{
		CleanupD3DDevice();
		UnregisterClass(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	//Now to show the window.
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);

	//Gets the HWND window size.
	RECT clientRect;
	GetClientRect(hWnd, &clientRect);
	int width = clientRect.right - clientRect.left;
	int height = clientRect.bottom - clientRect.top;

	IMGUI_CHECKVERSION();
	//Hopefully separates the context enough from the game so it doesn't interefer wiht the game's own context.
	ImGui::CreateContext();

	ImVec2 wndSize((float)width, (float)height);

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.DisplaySize = wndSize;

	const auto mainFontPath = (folder / "Resources" / "FontToUse.ttf").string();
	io.Fonts->AddFontFromFileTTF(mainFontPath.c_str(), 17.0f);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX9_Init(D3ddev);
	TrainingShortcut::Initialize(D3ddev);
	TrainingPad::Initialize(static_cast<HMODULE>(lpReserved), hWnd);
	ReleaseUpdate::CheckAtStartup(hWnd);
	bool quickTrainingSetup = false;
	bool recordingHookInstalled = false;
	HWND returnToGame = nullptr;
	const auto hideTool = [&]()
	{
		const bool hadFocus = GetForegroundWindow() == hWnd;
		ShowWindow(hWnd, SW_HIDE);
		if (hadFocus && returnToGame)
			SetForegroundWindow(returnToGame);
	};

	ZeroMemory(&msg, sizeof(msg));

	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}

		const auto foreground = GetForegroundWindow();
		DWORD foregroundPid = 0;
		const bool gameFocused = foreground && foreground != hWnd
			&& GetWindowThreadProcessId(foreground, &foregroundPid) && foregroundPid == GetCurrentProcessId();
		if (gameFocused)
			returnToGame = foreground;

		TrainingPad::Poll();
		const bool padTools = gameFocused && TrainingPad::Pressed(TrainingPad::Shortcut::Tools);
		const bool toolToggleDown = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
		const bool keyboardTools = (gameFocused || foreground == hWnd) && toolToggleDown && !toolToggleWasDown;
		if (padTools || keyboardTools) {
			ShowToolWindow = !ShowToolWindow;
			quickTrainingSetup = false;
		}

		toolToggleWasDown = toolToggleDown;

		const bool openQuickSetup = TrainingShortcut::TakeSetupRequest(
			gameFocused && TrainingPad::Pressed(TrainingPad::Shortcut::QuickTraining));
		if (openQuickSetup)
		{
			TrainingTeamEditor::OpenQuickSetup();
			quickTrainingSetup = true;
			ShowToolWindow = true;
		}

		const bool focusTool = openQuickSetup || ((padTools || keyboardTools) && ShowToolWindow);
		if (ShowToolWindow) {
			if (!IsWindowVisible(hWnd))
				ShowWindow(hWnd, SW_SHOW);
			if (focusTool)
				SetForegroundWindow(hWnd);
			if (openQuickSetup)
			{
				TrainingTeamEditor::TraceQuickShortcut(IsWindowVisible(hWnd)
					? "setup window shown" : "setup window could not be shown");
			}
		}
		else if (IsWindowVisible(hWnd)) {
			hideTool();
		}

		const bool runTrainingUpdates = TrainingTeamEditor::Update();
		if (gameFocused && !focusTool && runTrainingUpdates && TrainingPad::Pressed(TrainingPad::Shortcut::ResetTraining))
		{
			const auto direction = TrainingPad::HeldHorizontalDirection();
			std::optional<TrainingTeamEditor::XPositions> positions;
			if (direction == TrainingPad::HorizontalDirection::Left)
				positions = TrainingTeamEditor::LeftCorner;
			else if (direction == TrainingPad::HorizontalDirection::Right)
				positions = TrainingTeamEditor::RightCorner;
			TrainingTeamEditor::RequestTrainingRestart(positions);
		}
		if (runTrainingUpdates)
			TrainingTeamEditor::UpdateTrainingRestart(ShowToolWindow && !quickTrainingSetup);
		if (!recordingHookInstalled && sAction && runTrainingUpdates) {
			Trampoline* tramp = Trampoline::MakeTrampoline(GetModuleHandle(nullptr));
			InjectHook(_addr(0x140289c5a), tramp->Jump(FUN_1402b41b0), HookType::Call);
			recordingHookInstalled = true;
		}

		if (!ShowToolWindow)
		{
			TrainingShortcut::DispatchTraining();
			Sleep(8);
			continue;
		}

		RECT clientRect{};
		GetClientRect(hWnd, &clientRect);
		const int width = clientRect.right - clientRect.left;
		const int height = clientRect.bottom - clientRect.top;
		const ImVec2 wndSize(static_cast<float>(width), static_cast<float>(height));
		if (width > 0 && height > 0
			&& (static_cast<UINT>(width) != d3dpp.BackBufferWidth || static_cast<UINT>(height) != d3dpp.BackBufferHeight))
			ResizeD3D9Device(width, height);

		const bool toolFocused = GetForegroundWindow() == hWnd;
		// ImGui can close a dropdown with B during NewFrame.
		const bool hadPopup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
		ImGui_ImplWin32_NewFrame();
		TrainingPad::UpdateNavigation(toolFocused);
		ImGui_ImplDX9_NewFrame();
		ImGui::NewFrame();

		if (toolFocused && !hadPopup && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))
		{
			quickTrainingSetup = false;
			ShowToolWindow = false;
		}

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(wndSize, ImGuiCond_Always);
		if (focusTool)
			ImGui::SetNextWindowFocus();

		// ImGui UI Starts Here!
		if (ShowToolWindow && quickTrainingSetup)
		{
			ImGui::Begin("###QuickTrainingSetup", &ShowToolWindow, flags);
			TheMenuBar();
			TrainingPad::DrawSettings();
			if (TrainingTeamEditor::DrawQuickSetup())
			{
				quickTrainingSetup = false;
				ShowToolWindow = false;
			}
			ImGui::End();
		}
		else if (ShowToolWindow) {
			ImGui::Begin("###TrainingTools", &ShowToolWindow, flags);
			TrainingPad::DrawSettings();
			if (ImGui::Button(UiText("UI_HIDE")))
				ShowToolWindow = false;
			//ImGui::ShowMetricsWindow();

			// 
			//The Menu Bar.
			TheMenuBar();

			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(70, 255, 70, 255));
			ImGui::TextUnformatted(UiText("ACTIVATED_TRUE"));
			ImGui::TextUnformatted(UiText("ACTIVATED_TRUE2"));
			ImGui::PopStyleColor();

			//Should Speak for Itself.
			if (CheckTheMode() == true)
			{
				GetMainPointers();
				CheckIfInMatch();
				if (InMatch)
				{
					if (runTrainingUpdates)
						TickUpdates();
					if (ImGui::BeginTabBar("##tabs"))
					{
						if (runTrainingUpdates)
							TheExtraOptionsTab();
						TrainingTeamEditor::Draw();
						if (runTrainingUpdates)
						{
							//The other menus.
							TheStatusOptionsTab();
							TheCharacterOptionsTab();
							TheExtraDataTab();
							TheIncomingStuffTab();
							TheRecordPlaybackTab();
							//TheDebugStuffTab();
#if DEBUG
							//TheDebugStuffTab();
#endif
						}

						ImGui::EndTabBar();
					}

				}
				else
				{
					ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255));
					//ImGui::Text("Choose the characters and get into a match.");
					ImGui::TextUnformatted(UiText("ACTIVATED_NOTINMATCH"));
					ImGui::PopStyleColor();
				}
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 70, 255));
				//ImGui::Text("Get to Training Mode so this tool can do its thing.");
				ImGui::TextUnformatted(UiText("ACTIVATED_INMAINMENU"));
				ImGui::PopStyleColor();
			}

			ImGui::End();
		}

		ImGui::EndFrame();
		if (!ShowToolWindow)
		{
			hideTool();
			TrainingShortcut::DispatchTraining();
			continue;
		}
		D3ddev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
			D3DCOLOR_RGBA(45, 45, 48, 255), 1.0f, 0);


		if (D3ddev->BeginScene() >= 0) {
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			D3ddev->EndScene();
		}

		if (FAILED(D3ddev->Present(NULL, NULL, NULL, NULL)))
			Sleep(8);
	}

	TrainingPad::Shutdown();
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupD3DDevice();
	DestroyWindow(hWnd);
	UnregisterClass(wc.lpszClassName, GetModuleHandle(NULL));

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{

	case DLL_PROCESS_ATTACH:
		if (CheckGame())
		{
			DisableThreadLibraryCalls(hMod);
			CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
			//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

			//OnInitializeHook();

		}
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;

	}
	return TRUE;

}
