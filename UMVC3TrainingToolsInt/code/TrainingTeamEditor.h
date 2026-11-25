#pragma once

#include <Windows.h>
#include <optional>
#include "TrainingShortcut.h"

struct DummyReaction;

namespace TrainingTeamEditor
{
	struct XPositions
	{
		float p1;
		float p2;
	};
	inline constexpr XPositions Center{ -125.0f, 125.0f };
	inline constexpr XPositions LeftCorner{ -750.0f, -800.0f };
	inline constexpr XPositions RightCorner{ 750.0f, 800.0f };

	void InitializeLog(HMODULE module);
	void OpenQuickSetup();
	bool DrawQuickSetup();
	void DrawDummyReaction(const char* label, DummyReaction& reaction, bool allowRecordings);
	void ApplyQuickExercise(uintptr_t action);
	std::optional<XPositions> QuickResetPositions();
	void TraceQuickShortcut(const char* format, ...);
	void TraceQuickSelection(int p1Phase, int p2Phase, int mode);
	void TraceRecording(const char* format, ...);
	void FlushRecordingLog();
	void TraceWindowRenderer(const char* operation, HRESULT result);
	bool PrepareQuickMatch(const TrainingShortcut::TeamPicks& picks);
	uintptr_t GetPlayableTrainingAction();
	uintptr_t GetFightingAction();
	void Draw();
	void DrawPresetTab();
	void DrawLogMenu();
	bool Update();
	bool RequestTrainingRestart(std::optional<XPositions> positions = std::nullopt);
	void UpdateTrainingRestart(bool watchNativeRestart);
}
