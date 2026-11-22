#include "../pch.h"
#include "TrainingTeamEditor.h"

#include "../gui/imgui/imgui.h"
#include "umvc3menu.h"
#include "umvc3utils.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <numeric>
#include <string_view>
#include <utility>

namespace
{
	constexpr int SlotsPerTeam = 3;
	constexpr int SlotCount = 2 * SlotsPerTeam;
	constexpr uintptr_t SelectionOffset = 0x40;
	constexpr uintptr_t ActionFlagsOffset = 0xc4;
	constexpr uintptr_t ActionStepOffset = 0xd8;
	constexpr uintptr_t NextActionStepOffset = 0xdc;
	constexpr uintptr_t RestartOffset = 0x118;
	constexpr uintptr_t CharacterReloadCountdownOffset = 0x1210;
	constexpr uintptr_t LoadedAssistOffset = 0x2dbc; // mAssistType on the loaded fighter.
	constexpr const char* AssistTypes[] = { "Alpha", "Beta", "Gamma" };
	// Native stage-selector IDs, in the order used by the game's stage resolver.
	constexpr std::array<const char*, TrainingShortcut::StageCount> Stages{
		"Bonne Wonderland", "Kattlelox Island", "Demon Village Redux", "Demon Village",
		"Chaos at Tricell", "Tricell Laboratory", "Days of Future Past", "Metro City",
		"City That Never Sleeps", "Daily Bugle", "Asgard: Sea of Space", "Asgard",
		"S.H.I.E.L.D. Air Show", "S.H.I.E.L.D. Helicarrier", "Shadowland", "Hand Hideout",
		"Danger Room", "Training Room"
	};
	constexpr int FightingStep = 5;
	constexpr int RestartStep = 12;
	constexpr uint32_t RestartTraining = 3;
	constexpr uint32_t ReloadPlayer1 = 0x1000;
	constexpr uint32_t ReloadPlayer2 = 0x2000;
	constexpr uint32_t ReloadTeams = ReloadPlayer1 | ReloadPlayer2;
	constexpr DWORD WatchIntervalMs = 500;
	constexpr ULONGLONG RestartTimeoutMs = 12000;

	struct Fighter
	{
		const char* name;
		const char* gameClass;
		std::array<const char*, std::size(AssistTypes)> assists;
	};

	// Indexes match the base game's character IDs. Assist names are in character select order.
	// https://steamcommunity.com/app/357190/discussions/0/4299250686371974156/
	// Assist names follow the game's mnchs.arc and mnchs_msg.arc data.
	constexpr Fighter Roster[] = {
		{ "", "", {} },
		{ "Ryu", "uRyu", { "Shoryuken", "Hadoken", "Tatsumaki Senpukyaku" } },
		{ "Chun-Li", "uChunli", { "Kikoken", "Tenshokyaku", "Hyakuretsukyaku" } },
		{ "Akuma", "uGouki", { "Gohadoken", "Tatsumaki Zankukyaku", "Hyakki Gojin" } },
		{ "Chris", "uChris", { "Combination Punch", "Gun Fire", "Grenade Toss" } },
		{ "Wesker", "uWesker", { "Ghost Butterfly", "Samurai Edge (Lower Shot)", "Jaguar Dash" } },
		{ "Viewtiful Joe", "uVJoe", { "Voomerang", "Groovy Uppercut", "Shocking Pink" } },
		{ "Dante", "uDante", { "Jam Session", "Crystal", "Weasel Shot" } },
		{ "Trish", "uTrish", { "Trick \"Hopscotch\"", "Trick \"Peekaboo\"", "Low Voltage" } },
		{ "Frank West", "uFrank", { "Shopping Cart", "Tools of Survival", "Pick Me Up" } },
		{ "Spencer", "uSpencer", { "Wire Grapple (H. Shot)", "Wire Grapple (Slant Shot)", "Armor Piercer" } },
		{ "Arthur", "uArthur", { "Heavenly Slash", "Dagger Toss", "Fire Bottle Toss" } },
		{ "Amaterasu", "uAmaterasu", { "Solar Flare", "Cold Star", "Bloom" } },
		{ "Zero", "uZero", { "Ryuenjin", "Hadangeki", "Shippuga" } },
		{ "Tron Bonne", "uTron", { "Bonne Strike", "Gustaff Fire", "Bandit Boulder" } },
		{ "Morrigan", "uMorrigan", { "Shadow Blade", "Soul Fist", "Dark Harmonizer" } },
		{ "Hsien-Ko", "uLeilei", { "Senpu Bu", "Henkyo Ki", "Anki Hou" } },
		{ "Felicia", "uFelicia", { "Rolling Buckler / Rolling Slide", "Sand Splash", "Cat Spike" } },
		{ "C. Viper", "uCViper", { "Thunder Knuckle", "Seismic Hammer", "Burning Kick" } },
		{ "Haggar", "uHaggar", { "Double Lariat", "Violent Ax", "Steel Pipe" } },
		{ "Jill", "uJill", { "Cartwheel Kick", "Arrow Kick", "Somersault Kick" } },
		{ "Strider Hiryu", "uHiryu", { "Ame-no-Murakumo", "Gram", "Vajra" } },
		{ "Vergil", "uVergil", { "Judgment Cut", "Rising Sun", "Rapid Slash" } },
		{ "Phoenix Wright", "uNaruhodo", { "Paperwork (High)", "Press the Witness", "Get 'em, Missile!" } },
		{ "Firebrand", "uRedArremer", { "Hell Spitfire", "Demon Missile (Charge)", "Demon Missile (Swoop)" } },
		{ "Nemesis", "uNemesis", { "Clothesline Rocket", "Launcher Slam", "Rocket Launcher" } },
		{ "Spider-Man", "uSpiderMan", { "Web Ball", "Web Swing", "Spider Sting" } },
		{ "Captain America", "uCapAmerica", { "Shield Slash", "Stars & Stripes", "Charging Star" } },
		{ "Wolverine", "uWolverine", { "Tornado Claw", "Berserker Slash", "Berserker Barrage" } },
		{ "Magneto", "uMagneto", { "Electromagnetic Disruptor", "Hyper Gravitation", "Force Field" } },
		{ "Hulk", "uHulk", { "Gamma Wave", "Anti-Air Gamma Charge", "Gamma Charge" } },
		{ "She-Hulk", "uSheHulk", { "Torpedo", "Clothesline", "Somersault Kick+" } },
		{ "Taskmaster", "uTaskMaster", { "Aim Master (H. Shot)", "Aim Master (Parabolic Shot)", "Aim Master (Vertical Shot)" } },
		{ "Iron Man", "uIronMan", { "Unibeam", "Repulsor Blast", "Smart Bomb" } },
		{ "Thor", "uThor", { "Mighty Spark", "Mighty Smash", "Mighty Strike" } },
		{ "Doctor Doom", "uDrDoom", { "Plasma Beam", "Hidden Missiles", "Molecular Shield" } },
		{ "Phoenix", "uPhoenix", { "TK Shot", "TK Overdrive", "TK Trap" } },
		{ "Shuma-Gorath", "uShuma", { "Mystic Ray", "Mystic Stare", "Mystic Smash" } },
		{ "MODOK", "uModok", { "Barrier", "Balloon Bomb", "Psionic Blast" } },
		{ "Dormammu", "uDormammu", { "Dark Hole", "Purification", "Liberation" } },
		{ "Deadpool", "uDeadPool", { "Quick Work", "Katana-Rama!", "Trigger Happy" } },
		{ "Storm", "uStorm", { "Whirlwind", "Double Typhoon", "Lightning Attack" } },
		{ "Super-Skrull", "uSuperSkrull", { "Stone Smite", "Orbital Grudge", "Tenderizer" } },
		{ "Sentinel", "uSentinel", { "Sentinel Force (Charge)", "Sentinel Force (Bomb)", "Rocket Punch" } },
		{ "X-23", "uX23", { "Neck Slice", "Ankle Slice", "Crescent Scythe" } },
		{ "Nova", "uNova", { "Gravimetric Pulse", "Centurion Rush", "Nova Strike" } },
		{ "Rocket Raccoon", "uRRaccoon", { "Spitfire Twice", "Claymore", "Pendulum" } },
		{ "Ghost Rider", "uGhostRider", { "Chain of Rebuttal", "Heartless Spire", "Hellfire" } },
		{ "Iron Fist", "uIronFist", { "Dragon's Touch", "Crescent Heel", "Rising Fang" } },
		{ "Doctor Strange", "uDrStrange", { "Daggers of Denak", "Eye of Agamotto", "Bolts of Balthakk" } },
		{ "Hawkeye", "uHawkEye", { "Quick Shot (Greyhound)", "Trick Shot (Violet Fizz)", "Rag Time Shot (Kamikaze)" } }
	};

	constexpr auto FighterDisplayOrder = []
	{
		std::array<int, std::size(Roster) - 1> ids{};
		std::iota(ids.begin(), ids.end(), 1);
		std::sort(ids.begin(), ids.end(), [](int left, int right)
		{
			const std::string_view leftName = Roster[left].name;
			const std::string_view rightName = Roster[right].name;
			const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
			return std::lexicographical_compare(leftName.begin(), leftName.end(), rightName.begin(), rightName.end(),
				[lower](char a, char b) { return lower(a) < lower(b); });
		});
		return ids;
	}();

	constexpr uint32_t ClassId(const char* name)
	{
		uint32_t crc = 0xffffffff;
		for (; *name; ++name)
		{
			crc ^= static_cast<unsigned char>(*name);
			for (int bit = 0; bit < 8; ++bit)
				crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
		}
		return crc & 0x7fffffff;
	}

	static_assert(ClassId(Roster[1].gameClass) == 0x33f0cd9f);
	static_assert(ClassId(Roster[22].gameClass) == 0x2b5384f5);
	static_assert(ClassId(Roster[47].gameClass) == 0x298a709d);

	struct Selection
	{
		uint32_t side;
		int32_t id;
		uint32_t classId;
		int32_t color;
		uint32_t flags;
		int32_t assist;
		std::array<unsigned char, 0x40> optionsAfterAssist;

		bool operator==(const Selection&) const = default;
	};
	static_assert(sizeof(Selection) == 0x58);
	static_assert(offsetof(Selection, color) == 0x0c);
	static_assert(offsetof(Selection, assist) == 0x14);

	using Teams = std::array<Selection, SlotCount>;
	using Classes = std::array<uintptr_t, SlotCount>;

	struct LoadedFighter
	{
		uintptr_t gameClass;
		int32_t assist;

		bool operator==(const LoadedFighter&) const = default;
	};
	using LoadedTeams = std::array<LoadedFighter, SlotCount>;

	struct DraftSlot
	{
		int source;
		int id;
		int assist;
	};

	struct Match
	{
		uintptr_t settings{};
		uintptr_t action{};
		Teams teams{};
	};

	struct Trial
	{
		Match before;
		Teams proposed;
		LoadedTeams originalFighters;
		Classes expectedClasses;
		ULONGLONG started = 0;
		bool pending = false;
		bool uncertain = false;
		unsigned number = 0;
		uint32_t reloadFlags = 0;
	};

	struct ActionState
	{
		uint32_t flags{};
		uint32_t restart{};
		int step = -1;
		int nextStep = -1;
	};

	Match draftFrom;
	std::array<DraftSlot, SlotCount> draft{};
	std::array<DraftSlot, SlotCount> quickDraft{{
		{ 0, 1, 0 }, { 1, 2, 0 }, { 2, 3, 0 },
		{ 3, 28, 0 }, { 4, 27, 0 }, { 5, 26, 0 }
	}};
	int quickStage = 0; // Bonne Wonderland.
	const char* quickSetupStatus = "";
	Trial trial;
	const char* status = "";
	constexpr wchar_t LogName[] = L"UMVC3TrainingToolsInt-Teams.log";
	std::filesystem::path logPath;
	SRWLOCK logLock = SRWLOCK_INIT;
	bool logAvailable = false;
	bool logInTemp = false;
	unsigned attemptCount = 0;
	std::atomic<ULONGLONG> lastEditorUpdate{ 0 };

	bool Log(unsigned attempt, const char* format, ...)
	{
		AcquireSRWLockExclusive(&logLock);
		FILE* file = nullptr;
		const bool opened = !logPath.empty() && _wfopen_s(&file, logPath.c_str(), L"a") == 0;
		if (opened)
		{
			SYSTEMTIME time{};
			GetLocalTime(&time);
			std::fprintf(file, "%04d-%02d-%02d %02d:%02d:%02d.%03d attempt=%u ",
				time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, attempt);
			va_list args;
			va_start(args, format);
			std::vfprintf(file, format, args);
			va_end(args);
			std::fputc('\n', file);
			std::fclose(file);
		}
		ReleaseSRWLockExclusive(&logLock);
		return opened;
	}

	bool OpenLog(const std::filesystem::path& directory, const std::filesystem::path& previous)
	{
		std::error_code error;
		std::filesystem::create_directories(directory, error);
		if (error)
			return false;

		logPath = directory / LogName;
		DWORD moveError = ERROR_SUCCESS;
		if (std::filesystem::is_regular_file(previous, error) && !MoveFileW(previous.c_str(), logPath.c_str()))
		{
			moveError = GetLastError();
			if (moveError == ERROR_ALREADY_EXISTS || moveError == ERROR_FILE_EXISTS)
			{
				SYSTEMTIME time{};
				GetLocalTime(&time);
				wchar_t archiveName[96];
				swprintf_s(archiveName, L"UMVC3TrainingToolsInt-Teams-legacy-%04d%02d%02d-%02d%02d%02d-%lu.log",
					time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, GetCurrentProcessId());
				const auto archive = directory / archiveName;
				moveError = MoveFileW(previous.c_str(), archive.c_str()) ? ERROR_SUCCESS : GetLastError();
			}
		}

		const bool opened = Log(0, "Team editor with native reload started (pid=%lu)", GetCurrentProcessId());
		if (opened && moveError != ERROR_SUCCESS && moveError != ERROR_FILE_NOT_FOUND)
			Log(0, "Could not move the previous log (Windows error %lu)", moveError);
		return opened;
	}

	template <typename T>
	bool Read(uintptr_t address, T& value)
	{
		SIZE_T bytes = 0;
		return address && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
			&value, sizeof(value), &bytes) && bytes == sizeof(value);
	}

	template <typename T>
	bool Write(uintptr_t address, const T& value)
	{
		SIZE_T bytes = 0;
		return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address),
			&value, sizeof(value), &bytes) && bytes == sizeof(value);
	}

	uintptr_t PointerAt(uintptr_t address)
	{
		uintptr_t value = 0;
		Read(address, value);
		return value;
	}

	bool SupportsTeamReload()
	{
		// The native restart dispatches both team reload flags through sCharacter.
		constexpr std::array<uint8_t, 13> player1Check{
			0xba, 0x00, 0x10, 0x00, 0x00, 0x48, 0x8b, 0xce, 0xe8, 0x33, 0x15, 0x00, 0x00
		};
		constexpr std::array<uint8_t, 13> player2Check{
			0xba, 0x00, 0x20, 0x00, 0x00, 0x48, 0x8b, 0xce, 0xe8, 0x0d, 0x15, 0x00, 0x00
		};
		std::array<uint8_t, 13> actual{};
		return Read(_addr(0x14000a300), actual) && actual == player1Check
			&& Read(_addr(0x14000a326), actual) && actual == player2Check;
	}

	bool ReadAction(uintptr_t action, ActionState& state)
	{
		return action && Read(action + ActionFlagsOffset, state.flags)
			&& Read(action + RestartOffset, state.restart)
			&& Read(action + ActionStepOffset, state.step)
			&& Read(action + NextActionStepOffset, state.nextStep);
	}

	bool IsFighting(const ActionState& state)
	{
		return state.step == FightingStep && state.nextStep == -1 && !state.restart;
	}

	void ClearReloadFlags(uintptr_t action, uint32_t flags)
	{
		InterlockedAnd(reinterpret_cast<volatile LONG*>(action + ActionFlagsOffset), static_cast<LONG>(~flags));
	}

	bool ReadMatch(Match& match)
	{
		match.settings = PointerAt(_addr(0x140d50e58));
		int mode = 0;
		if (!match.settings || !Read(match.settings + 0x34c, mode) || mode != 5)
			return false;

		match.action = PointerAt(_addr(0x140d47e68));
		return match.action && Read(match.settings + SelectionOffset, match.teams);
	}

	bool ReadLoadedTeams(LoadedTeams& teams)
	{
		const auto characters = PointerAt(_addr(0x140d44a70));
		if (!characters)
			return false;
		for (int side = 0; side < 2; ++side)
		{
			const auto chain = PointerAt(characters + (side == 0 ? 0x58 : 0x328));
			if (!chain)
				return false;
			const auto next = PointerAt(chain + 0x10);
			if (!next)
				return false;
			const auto first = PointerAt(chain + 0x8);
			const auto second = PointerAt(next + 0x8);
			const auto third = second ? PointerAt(second + 0x18) : 0;
			const uintptr_t fighters[] = { first, second, third };
			for (int position = 0; position < SlotsPerTeam; ++position)
			{
				const auto fighter = fighters[position];
				auto& loaded = teams[side * SlotsPerTeam + position];
				if (!Read(fighter, loaded.gameClass) || !loaded.gameClass
					|| !Read(fighter + LoadedAssistOffset, loaded.assist))
					return false;
			}
		}
		return true;
	}

	bool InRoster(int id)
	{
		return id > 0 && id < static_cast<int>(std::size(Roster));
	}

	bool ValidPicks(const TrainingShortcut::TeamPicks& picks)
	{
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			const auto pick = picks[slot];
			if (!InRoster(pick.id) || pick.assist < 0 || pick.assist >= IM_ARRAYSIZE(AssistTypes))
				return false;
			for (int previous = slot / SlotsPerTeam * SlotsPerTeam; previous < slot; ++previous)
			{
				if (picks[previous].id == pick.id)
					return false;
			}
		}
		return true;
	}

	bool ValidSelection(const Teams& teams)
	{
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			const auto& fighter = teams[slot];
			if (fighter.side != static_cast<uint32_t>(slot / SlotsPerTeam)
				|| !InRoster(fighter.id) || fighter.classId != ClassId(Roster[fighter.id].gameClass))
				return false;
		}
		return true;
	}

	bool SameSelection(const Teams& left, const Teams& right)
	{
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			if (left[slot].side != right[slot].side || left[slot].id != right[slot].id
				|| left[slot].classId != right[slot].classId || left[slot].assist != right[slot].assist)
				return false;
		}
		return true;
	}

	void LogTeams(unsigned attempt, const char* label, const Teams& teams)
	{
		Log(attempt, "%s (fighter/assist) P1=[%d/%d,%d/%d,%d/%d] P2=[%d/%d,%d/%d,%d/%d]", label,
			teams[0].id, teams[0].assist, teams[1].id, teams[1].assist, teams[2].id, teams[2].assist,
			teams[3].id, teams[3].assist, teams[4].id, teams[4].assist, teams[5].id, teams[5].assist);
	}

	void LogClasses(unsigned attempt, const char* label, const Classes& classes)
	{
		const auto base = static_cast<uintptr_t>(GetUMVC3EntryPoint());
		auto offset = [base](uintptr_t address) { return static_cast<unsigned long long>(address ? address - base : 0); };
		Log(attempt, "%s P1=[%llX,%llX,%llX] P2=[%llX,%llX,%llX]", label,
			offset(classes[0]), offset(classes[1]), offset(classes[2]), offset(classes[3]), offset(classes[4]), offset(classes[5]));
	}

	void LogLoadedTeams(unsigned attempt, const char* label, const LoadedTeams& teams)
	{
		Classes classes{};
		for (int slot = 0; slot < SlotCount; ++slot)
			classes[slot] = teams[slot].gameClass;
		LogClasses(attempt, label, classes);
		Log(attempt, "%s assists P1=[%d,%d,%d] P2=[%d,%d,%d]", label,
			teams[0].assist, teams[1].assist, teams[2].assist, teams[3].assist, teams[4].assist, teams[5].assist);
	}

	DWORD WINAPI WatchRestart(void* argument)
	{
		// Sample independently so a blocked render loop cannot hide the restart state.
		const auto attempt = static_cast<unsigned>(reinterpret_cast<uintptr_t>(argument));
		const auto started = GetTickCount64();
		for (;;)
		{
			const auto lastUpdate = lastEditorUpdate.load();
			const auto now = GetTickCount64();
			const auto settings = PointerAt(_addr(0x140d50e58));
			const auto action = PointerAt(_addr(0x140d47e68));
			int mode = -1;
			Read(settings ? settings + 0x34c : 0, mode);
			ActionState actionState;
			const bool haveAction = ReadAction(action, actionState);
			const auto characters = PointerAt(_addr(0x140d44a70));
			int loadCountdown = -1;
			if (characters)
				Read(characters + CharacterReloadCountdownOffset, loadCountdown);
			Log(attempt, "watch +%llu ms mode=%d settings=%p action=%p restart=%d step=%d next=%d flags=0x%08X reload_countdown=%d editor_update_age_ms=%llu",
				now - started, mode, reinterpret_cast<void*>(settings), reinterpret_cast<void*>(action),
				haveAction ? static_cast<int>(actionState.restart) : -1, actionState.step,
				actionState.nextStep, actionState.flags, loadCountdown, now - lastUpdate);

			Teams selected{};
			if (mode == 5 && Read(settings + SelectionOffset, selected))
				LogTeams(attempt, "watch selection", selected);
			LoadedTeams loaded{};
			if (mode == 5 && ReadLoadedTeams(loaded))
				LogLoadedTeams(attempt, "watch loaded", loaded);
			else
				Log(attempt, "watch loaded classes unavailable");

			if (now - started >= RestartTimeoutMs + WatchIntervalMs)
				break;
			Sleep(WatchIntervalMs);
		}
		Log(attempt, "watch finished");
		return 0;
	}

	void DrawTeamDraft(std::array<DraftSlot, SlotCount>& slots)
	{
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			const int position = slot % SlotsPerTeam;
			if (position == 0)
				ImGui::SeparatorText(slot == 0 ? "Player 1" : "Player 2");
			ImGui::PushID(slot);
			char label[16];
			std::snprintf(label, sizeof(label), "Slot %d", position + 1);
			const int current = slots[slot].id;
			if (ImGui::BeginCombo(label, Roster[current].name))
			{
				for (int id : FighterDisplayOrder)
				{
					if (ImGui::Selectable(Roster[id].name, id == current))
						slots[slot].id = id;
					if (id == current)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(position == 0);
			if (ImGui::ArrowButton("Up", ImGuiDir_Up))
				std::swap(slots[slot], slots[slot - 1]);
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(position == SlotsPerTeam - 1);
			if (ImGui::ArrowButton("Down", ImGuiDir_Down))
				std::swap(slots[slot], slots[slot + 1]);
			ImGui::EndDisabled();
			auto& assist = slots[slot].assist;
			const auto& names = Roster[slots[slot].id].assists;
			const bool knownAssist = assist >= 0 && assist < IM_ARRAYSIZE(AssistTypes);
			if (ImGui::BeginCombo("Assist", knownAssist ? names[assist] : "Unknown"))
			{
				for (int type = 0; type < IM_ARRAYSIZE(AssistTypes); ++type)
				{
					char option[64];
					std::snprintf(option, sizeof(option), "%s - %s", AssistTypes[type], names[type]);
					const bool selected = type == assist;
					if (ImGui::Selectable(option, selected))
						assist = type;
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::PopID();
		}
	}

	void ResetDraft(const Match& match)
	{
		draftFrom = match;
		for (int slot = 0; slot < SlotCount; ++slot)
			draft[slot] = { slot, match.teams[slot].id, match.teams[slot].assist };
	}

	void Apply(const Match& shown)
	{
		const unsigned attempt = ++attemptCount;
		Log(attempt, "Apply and reload clicked");
		if (!SupportsTeamReload())
		{
			status = "Team reload is unavailable for this executable.";
			Log(attempt, "native reload signature mismatch");
			return;
		}
		if (recording || recordingP1 || recordingP2 || replaying || replayingP1 || replayingP2)
		{
			status = "Stop recording or playback before editing teams.";
			Log(attempt, "blocked by active recording or playback");
			return;
		}

		Match current;
		LoadedTeams loaded{};
		ActionState action;
		if (!ReadMatch(current) || current.settings != shown.settings || current.action != shown.action
			|| !SameSelection(current.teams, shown.teams) || !ReadLoadedTeams(loaded)
			|| !ReadAction(current.action, action) || !IsFighting(action) || (action.flags & ReloadTeams))
		{
			status = "Wait until both teams are ready and the round has started.";
			Log(attempt, "match preflight failed settings=%p action=%p restart=%u step=%d next=%d flags=0x%08X",
				reinterpret_cast<void*>(current.settings), reinterpret_cast<void*>(current.action),
				action.restart, action.step, action.nextStep, action.flags);
			return;
		}

		Teams proposed{};
		Classes expected{};
		uint32_t reloadFlags = 0;
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			const auto edit = draft[slot];
			const int first = (slot / SlotsPerTeam) * SlotsPerTeam;
			for (int other = first; other < slot; ++other)
			{
				if (edit.id == draft[other].id)
				{
					status = "Choose three different fighters per team.";
					Log(attempt, "duplicate fighter id=%d side=%d", edit.id, slot / SlotsPerTeam + 1);
					return;
				}
			}
			proposed[slot] = current.teams[edit.source];
			proposed[slot].id = edit.id;
			proposed[slot].classId = ClassId(Roster[edit.id].gameClass);
			proposed[slot].assist = edit.assist;
			if (proposed[slot] != current.teams[slot])
				reloadFlags |= slot < SlotsPerTeam ? ReloadPlayer1 : ReloadPlayer2;
			for (int original = 0; original < SlotCount; ++original)
			{
				if (current.teams[original].id == edit.id)
				{
					expected[slot] = loaded[original].gameClass;
					break;
				}
			}
		}

		if (!reloadFlags)
		{
			status = "Change a fighter, assist, or team order first.";
			Log(attempt, "no selection changes");
			return;
		}

		LogTeams(attempt, "before", current.teams);
		LogTeams(attempt, "requested", proposed);
		LogLoadedTeams(attempt, "before loaded", loaded);
		LogClasses(attempt, "expected class RVAs (0=unknown)", expected);
		const auto selectionAddress = current.settings + SelectionOffset;
		const bool wroteSelection = Write(selectionAddress, proposed);
		const auto selectionError = wroteSelection ? 0 : GetLastError();
		uint32_t previousFlags = 0;
		if (wroteSelection)
			previousFlags = static_cast<uint32_t>(InterlockedOr(
				reinterpret_cast<volatile LONG*>(current.action + ActionFlagsOffset), static_cast<LONG>(reloadFlags)));
		const bool wroteRestart = wroteSelection && Write(current.action + RestartOffset, RestartTraining);
		const auto restartError = wroteRestart || !wroteSelection ? 0 : GetLastError();
		if (wroteSelection)
			Log(attempt, "requested native reload flags=0x%08X previous action flags=0x%08X", reloadFlags, previousFlags);
		Log(attempt, "write selection=%d error=%lu; write restart=%d error=%lu", wroteSelection, selectionError,
			wroteRestart, restartError);
		if (!wroteRestart)
		{
			if (wroteSelection)
				ClearReloadFlags(current.action, reloadFlags);
			const bool restored = Write(selectionAddress, current.teams);
			Log(attempt, "restore after failed write=%d", restored);
			status = restored ? "Could not apply the change. Original settings restored."
				: "Could not restore the original settings. Start a fresh Training match.";
			if (!restored)
			{
				trial.before = current;
				trial.uncertain = true;
			}
			return;
		}

		trial = { current, proposed, loaded, expected, GetTickCount64(), true, false, attempt, reloadFlags };
		status = "Reload requested. Waiting for the game to load the teams...";
		if (logAvailable)
		{
			const HANDLE watcher = CreateThread(nullptr, 0, WatchRestart, reinterpret_cast<void*>(static_cast<uintptr_t>(attempt)), 0, nullptr);
			if (watcher)
				CloseHandle(watcher);
			else
				Log(attempt, "could not start restart watcher error=%lu", GetLastError());
		}
	}
}

void TrainingTeamEditor::OpenQuickSetup()
{
	quickSetupStatus = "";
}

bool TrainingTeamEditor::DrawQuickSetup()
{
	ImGui::SeparatorText("Quick Training");
	ImGui::Combo("Stage", &quickStage, Stages.data(), static_cast<int>(Stages.size()));
	DrawTeamDraft(quickDraft);
	if (*quickSetupStatus)
		ImGui::TextWrapped("%s", quickSetupStatus);

	if (ImGui::Button("Start Training"))
	{
		TrainingShortcut::TeamPicks picks{};
		for (int slot = 0; slot < SlotCount; ++slot)
			picks[slot] = { quickDraft[slot].id, quickDraft[slot].assist };
		if (!ValidPicks(picks))
		{
			TraceQuickShortcut("Start Training rejected: invalid fighter or assist selection");
			quickSetupStatus = "Choose three different fighters and an assist for each team.";
		}
		else if (TrainingShortcut::StartTraining(picks, quickStage))
			return true;
		else
			quickSetupStatus = "Return to the main menu to start a Training match.";
	}
	ImGui::SameLine();
	return ImGui::Button("Cancel");
}

void TrainingTeamEditor::RequestTrainingRestart()
{
	Match match{};
	ActionState action{};
	if (ReadMatch(match) && ReadAction(match.action, action) && IsFighting(action) && !(action.flags & ReloadTeams))
		Write(match.action + RestartOffset, RestartTraining);
}

void TrainingTeamEditor::TraceQuickShortcut(const char* event)
{
	Log(0, "quick training: %s", event);
}

void TrainingTeamEditor::TraceQuickSelection(int p1Phase, int p2Phase, int mode)
{
	Log(0, "quick training: selector phases P1=%d P2=%d mode=%d", p1Phase, p2Phase, mode);
}

bool TrainingTeamEditor::PrepareQuickMatch(const TrainingShortcut::TeamPicks& picks)
{
	const auto settings = PointerAt(_addr(0x140d50e58));
	int mode = 0;
	Teams current{};
	if (!settings || !Read(settings + 0x34c, mode) || mode != 5
		|| !Read(settings + SelectionOffset, current) || !ValidPicks(picks))
	{
		Log(0, "quick training: selection preflight failed");
		return false;
	}

	auto proposed = current;
	for (int slot = 0; slot < SlotCount; ++slot)
	{
		if (proposed[slot].id != picks[slot].id)
			proposed[slot].color = 0;
		proposed[slot].side = slot / SlotsPerTeam;
		proposed[slot].id = picks[slot].id;
		proposed[slot].classId = ClassId(Roster[picks[slot].id].gameClass);
		proposed[slot].assist = picks[slot].assist;
	}
	if (!Write(settings + SelectionOffset, proposed))
	{
		const bool restored = Write(settings + SelectionOffset, current);
		Log(0, "quick training: could not stage selections, original selection restored=%d", restored);
		return false;
	}
	LogTeams(0, "quick training: teams staged", proposed);
	return true;
}

void TrainingTeamEditor::InitializeLog(HMODULE module)
{
	wchar_t modulePath[MAX_PATH]{};
	const auto length = GetModuleFileNameW(module, modulePath, MAX_PATH);
	if (length && length < MAX_PATH)
	{
		const auto root = std::filesystem::path(modulePath).parent_path();
		logAvailable = OpenLog(root / L"logs", root / LogName);
	}
	if (!logAvailable)
	{
		std::error_code error;
		const auto temp = std::filesystem::temp_directory_path(error);
		if (!error)
		{
			logAvailable = OpenLog(temp / L"UMVC3TrainingToolsInt" / L"logs", temp / LogName);
			logInTemp = logAvailable;
		}
	}
}

bool TrainingTeamEditor::Update()
{
	lastEditorUpdate.store(GetTickCount64());
	if (!trial.pending && !trial.uncertain)
		return true;

	Match match;
	if (!ReadMatch(match))
	{
		if (trial.pending && GetTickCount64() - trial.started < RestartTimeoutMs)
			return false;
		Log(trial.number, "editor ended trial: Training match unavailable");
		trial = {};
		draftFrom = {};
		status = "The Training match ended. Changes are no longer pending.";
		return true;
	}
	if (trial.uncertain && match.settings == trial.before.settings && match.action == trial.before.action)
		return false;
	if (trial.uncertain)
	{
		trial = {};
		draftFrom = {};
		status = "New Training match detected.";
		return true;
	}

	ActionState action;
	LoadedTeams loaded{};
	const bool haveLoaded = ReadLoadedTeams(loaded);
	const bool finishedRestart = ReadAction(match.action, action) && IsFighting(action);
	const bool sameMatch = match.settings == trial.before.settings && match.action == trial.before.action;
	if (sameMatch && finishedRestart && haveLoaded && loaded != trial.originalFighters
		&& SameSelection(match.teams, trial.proposed))
	{
		bool matches = true;
		bool known = true;
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			const auto expected = trial.expectedClasses[slot];
			known &= expected != 0;
			matches &= expected ? loaded[slot].gameClass == expected
				: loaded[slot].gameClass != trial.originalFighters[slot].gameClass;
			matches &= loaded[slot].assist == trial.proposed[slot].assist;
		}
		if (matches)
		{
			if (action.flags & trial.reloadFlags)
				ClearReloadFlags(match.action, trial.reloadFlags);
			trial.pending = false;
			restarted = true;
			restartTimer = 0;
			Log(trial.number, "editor completed trial: assists verified, all class IDs verifiable=%d", known);
			LogLoadedTeams(trial.number, "confirmed loaded", loaded);
			status = known ? "Loaded fighters and assists match the requested teams."
				: "Assists verified. Check the new fighter in-game.";
			return true;
		}
	}

	if (GetTickCount64() - trial.started < RestartTimeoutMs)
		return false;

	trial.pending = false;
	Log(trial.number, "editor timed out: fighting=%d step=%d next=%d restart=%u flags=0x%08X loaded_readable=%d loaded_unchanged=%d",
		finishedRestart, action.step, action.nextStep, action.restart, action.flags,
		haveLoaded, haveLoaded && loaded == trial.originalFighters);
	if (haveLoaded)
		LogLoadedTeams(trial.number, "loaded at timeout", loaded);
	if (sameMatch && finishedRestart && haveLoaded && loaded == trial.originalFighters)
	{
		if (action.flags & trial.reloadFlags)
			ClearReloadFlags(match.action, trial.reloadFlags);
		if (SameSelection(match.teams, trial.before.teams)
			|| (SameSelection(match.teams, trial.proposed) && Write(match.settings + SelectionOffset, trial.before.teams)))
		{
			draftFrom = {};
			Log(trial.number, "editor confirmed original selection restored");
			status = "The game kept the original fighters and assists. Original selection restored.";
			return true;
		}
	}

	trial.uncertain = true;
	Log(trial.number, "editor could not confirm teams; further edits disabled for this match");
	status = action.restart || action.step == RestartStep ? "The Training reload did not finish. See the log."
		: "Could not confirm the loaded teams. See the log.";
	return false;
}

void TrainingTeamEditor::Draw()
{
	if (!ImGui::BeginTabItem("Teams (Experimental)"))
		return;

	ImGui::TextWrapped("Edit fighters and assists, then apply the changes in Training.");
	if (logAvailable)
		ImGui::TextDisabled("%s", logInTemp ? "Log: %TEMP%\\UMVC3TrainingToolsInt\\logs\\UMVC3TrainingToolsInt-Teams.log"
			: "Log: logs\\UMVC3TrainingToolsInt-Teams.log");
	else
		ImGui::TextDisabled("Could not create the team editor log.");
	if (*status)
		ImGui::TextWrapped("%s", status);

	Match match;
	if (trial.uncertain || !ReadMatch(match) || !ValidSelection(match.teams))
	{
		if (!trial.uncertain)
			ImGui::TextWrapped("Enter a Training match with the standard roster to edit teams.");
		ImGui::EndTabItem();
		return;
	}
	if (!trial.pending && (match.settings != draftFrom.settings || match.action != draftFrom.action
		|| !SameSelection(match.teams, draftFrom.teams)))
		ResetDraft(match);

	ImGui::BeginDisabled(trial.pending);
	DrawTeamDraft(draft);

	if (ImGui::Button("Apply and reload"))
		Apply(match);
	ImGui::SameLine();
	if (ImGui::Button("Reset draft"))
	{
		ResetDraft(match);
		status = "";
	}
	ImGui::EndDisabled();
	ImGui::EndTabItem();
}
