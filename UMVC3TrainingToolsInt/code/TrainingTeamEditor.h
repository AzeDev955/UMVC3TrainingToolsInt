#pragma once

#include <Windows.h>
#include "TrainingShortcut.h"

namespace TrainingTeamEditor
{
	void InitializeLog(HMODULE module);
	void OpenQuickSetup();
	bool DrawQuickSetup();
	void TraceQuickShortcut(const char* event);
	void TraceQuickSelection(int p1Phase, int p2Phase, int mode);
	bool PrepareQuickMatch(const TrainingShortcut::TeamPicks& picks);
	void Draw();
	bool Update();
}
