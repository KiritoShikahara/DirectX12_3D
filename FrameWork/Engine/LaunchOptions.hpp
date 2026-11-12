#pragma once
#include <string>
#include <Windows.h>
#include <shellapi.h>

namespace Engine::Core
{
	/// <summary>
	/// 起動引数。自動テスト(スモークテスト・スクリーンショット)用
	///   --frames N        : Nフレーム後に終了
	///   --capture PATH    : 終了直前のフレームをPNG保存
	///   --scene N         : 開始シーン (0:Title 1:StageSelect 2:InGame 3:GameClear 4:GameOver 5:Tutorial)
	///   --enemy N         : InGameの敵ランク (0:Basic 1:Advanced 2:Boss)
	///   --autoplay        : 自動入力で戦闘を進める
	/// </summary>
	struct LaunchOptions
	{
		int Frames = 0;
		std::wstring CapturePath;
		int StartScene = -1;
		int EnemyRank = -1;
		bool AutoPlay = false;

		static LaunchOptions& Get()
		{
			static LaunchOptions options;
			return options;
		}

		bool IsAutomated()const
		{
			return Frames > 0;
		}

		void Parse()
		{
			int argc = 0;
			LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
			if (argv == nullptr)
			{
				return;
			}
			for (int i = 1; i < argc; i++)
			{
				const std::wstring arg = argv[i];
				const bool hasValue = (i + 1 < argc);
				if (arg == L"--frames" && hasValue) { Frames = _wtoi(argv[++i]); }
				else if (arg == L"--capture" && hasValue) { CapturePath = argv[++i]; }
				else if (arg == L"--scene" && hasValue) { StartScene = _wtoi(argv[++i]); }
				else if (arg == L"--enemy" && hasValue) { EnemyRank = _wtoi(argv[++i]); }
				else if (arg == L"--autoplay") { AutoPlay = true; }
			}
			LocalFree(argv);
		}
	};
}
