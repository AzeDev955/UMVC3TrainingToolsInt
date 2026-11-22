#pragma once

#include <array>
#include <cstdint>

struct IDirect3DDevice9;

namespace TrainingShortcut
{
	struct FighterPick
	{
		int32_t id;
		int32_t assist;
	};
	using TeamPicks = std::array<FighterPick, 6>;
	constexpr int StageCount = 18;

	void Initialize(IDirect3DDevice9* toolDevice);
	bool TakeSetupRequest();
	bool StartTraining(const TeamPicks& picks, int stage);
	void DispatchTraining();
}
