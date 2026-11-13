#include "engine/render/dxgi_device_kind.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace labrador
{
	RenderDeviceKind dxgi_device_kind(std::uint32_t flags,
		std::uint32_t vendor_id, std::uint32_t device_id,
		std::string_view device_name)
	{
		if ((flags & dxgi_adapter_flag_software) != 0)
		{
			return RenderDeviceKind::software;
		}

		if (vendor_id == microsoft_basic_render_vendor_id &&
			device_id == microsoft_basic_render_device_id)
		{
			return RenderDeviceKind::software;
		}

		std::string lowered(device_name);
		std::transform(lowered.begin(), lowered.end(), lowered.begin(),
			[](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});

		return lowered.find("microsoft basic render") != std::string::npos
			? RenderDeviceKind::software : RenderDeviceKind::hardware;
	}
}
