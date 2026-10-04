#include "tools/linesweeper_capture/png_file.h"

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace capture
{
	namespace
	{
		[[noreturn]] void refuse(const std::filesystem::path& path,
			const char* step)
		{
			throw std::runtime_error("Could not write " + path.string() + ": " +
				step + ".");
		}
	}

	void write_png(const std::filesystem::path& path, int width, int height,
		const std::vector<unsigned char>& rgba)
	{
		const std::size_t pixel_count = static_cast<std::size_t>(width) *
			static_cast<std::size_t>(height);

		if (width <= 0 || height <= 0 || rgba.size() != pixel_count * 4)
		{
			throw std::invalid_argument("write_png was handed " +
				std::to_string(rgba.size()) + " bytes for a " +
				std::to_string(width) + "x" + std::to_string(height) +
				" frame.");
		}

		// Blue, green, red: the order the PNG encoder stores without a
		// conversion of its own, so the pixels it compresses are exactly
		// these.
		std::vector<unsigned char> bgr(pixel_count * 3);
		for (std::size_t pixel = 0; pixel < pixel_count; ++pixel)
		{
			bgr[pixel * 3 + 0] = rgba[pixel * 4 + 2];
			bgr[pixel * 3 + 1] = rgba[pixel * 4 + 1];
			bgr[pixel * 3 + 2] = rgba[pixel * 4 + 0];
		}

		ComPtr<IWICImagingFactory> factory;
		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
			CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
		{
			refuse(path, "WIC is unavailable");
		}

		ComPtr<IWICStream> stream;
		if (FAILED(factory->CreateStream(&stream)) ||
			FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)))
		{
			refuse(path, "it could not be opened for writing");
		}

		ComPtr<IWICBitmapEncoder> encoder;
		ComPtr<IWICBitmapFrameEncode> frame;
		ComPtr<IPropertyBag2> properties;
		if (FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr,
				&encoder)) ||
			FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) ||
			FAILED(encoder->CreateNewFrame(&frame, &properties)) ||
			FAILED(frame->Initialize(properties.Get())) ||
			FAILED(frame->SetSize(static_cast<UINT>(width),
				static_cast<UINT>(height))))
		{
			refuse(path, "the PNG frame could not be started");
		}

		// The encoder may answer with a format of its own, so the frame is
		// handed a bitmap that says what it holds and WIC converts if it has
		// to - the same arrangement golden_image.cpp makes, for its reason.
		WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
		ComPtr<IWICBitmap> source;
		if (FAILED(frame->SetPixelFormat(&format)) ||
			FAILED(factory->CreateBitmapFromMemory(static_cast<UINT>(width),
				static_cast<UINT>(height), GUID_WICPixelFormat24bppBGR,
				static_cast<UINT>(width) * 3, static_cast<UINT>(bgr.size()),
				bgr.data(), &source)))
		{
			refuse(path,
				"the frame could not be described to the PNG encoder");
		}

		if (FAILED(frame->WriteSource(source.Get(), nullptr)) ||
			FAILED(frame->Commit()) ||
			FAILED(encoder->Commit()))
		{
			refuse(path, "the pixels could not be written");
		}
	}
}
