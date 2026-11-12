#pragma once
#include"System/CSV/CsvLoader.hpp"

namespace Engine::System
{

	struct SaveData
	{
		//	識別用
		int id = -1;
		//	0:未討伐 1:青討伐 2:黄討伐 3:赤討伐
		int ClearLevel = 0;

		CSV_BINDABLE(SaveData, obj.id, obj.ClearLevel);
	};
}