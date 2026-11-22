#pragma once

#include <Windows.h>
#include <optional>
#include "TrainingShortcut.h"

namespace TrainingTeamEditor
{
	struct XPositions
	{
		float p1;
		float p2;
	};
	inline constexpr XPositions LeftCorner{ -750.0f, -800.0f };
	inline constexpr XPositions RightCorner{ 750.0f, 800.0f };

	void InitializeLog(HMODULE module);
	void OpenQuickSetup();
	bool DrawQuickSetup();
	void TraceQuickShortcut(const char* event);
	void TraceQuickSelection(int p1Phase, int p2Phase, int mode);
	bool PrepareQuickMatch(const TrainingShortcut::TeamPicks& picks);
	bool IsTrainingPlayable();
	void Draw();
	bool Update();
	void RequestTrainingRestart(std::optional<XPositions> positions = std::nullopt);
	void UpdateTrainingRestart(bool watchNativeRestart);
}
