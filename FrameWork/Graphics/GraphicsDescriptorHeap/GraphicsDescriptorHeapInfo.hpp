#pragma once

namespace Engine::Graphics
{
	struct DescriptorHeapInfo
	{
		int index;
		int size;

		/// <summary>
		/// 数値が範囲内かどうか
		/// </summary>
		/// <returns>true:正常値</returns>
		bool IsValid() const noexcept
		{
			return index >= 0 && size > 0;
		}
	};
}
