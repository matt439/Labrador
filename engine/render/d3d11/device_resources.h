//
// DeviceResources.h - A wrapper for the Direct3D 11 device and swapchain
//

#pragma once

#include <Windows.h>
#include <wrl/client.h>
#include <d3d11_1.h>
#include <dxgi1_6.h>

namespace labrador
{
    // Provides an interface for an application that owns DeviceResources to be notified of the device being lost or created.
    //
    // Named D3DDeviceNotify rather than DeviceNotify, which is the seam's
    // (engine/render/renderer.h) and is what a game implements. Renderer::Impl
    // implements this one and forwards; nothing outside this folder sees it.
    interface D3DDeviceNotify
    {
        virtual void OnDeviceLost() = 0;
        virtual void OnDeviceRestored() = 0;

    protected:
        ~D3DDeviceNotify() = default;
    };

    // Controls all the DirectX device resources.
    //
    // ONLY WHAT THIS ENGINE ASKS FOR. Three capabilities a general device
    // wrapper offers are absent, and each is named here so a reader need not
    // look for it.
    //
    //  - No depth buffer. The renderer draws 2D and nothing else, so there is
    //    no depth format, depth texture, depth view or per-frame DiscardView.
    //  - No tearing. Present takes the sync interval Renderer::set_vsync
    //    chose, and an interval of 0 without the tearing flag is composed in a
    //    window rather than torn.
    //  - No HDR. The back buffer is always sRGB, so there is no colour-space
    //    choice and no window/output intersection walk to make one.
    //
    // The back-buffer count (2) and the feature-level floor (10_0) are fixed
    // in device_resources.cpp, whose level list starts at the floor.
    //
    // The one runtime option is whether this OS has flip-model swap effects,
    // so it is a bool and not a flags word.
    class DeviceResources
    {
    public:
        explicit DeviceResources(DXGI_FORMAT backBufferFormat) noexcept;
        ~DeviceResources() = default;

        DeviceResources(DeviceResources&&) = default;
        DeviceResources& operator= (DeviceResources&&) = default;

        DeviceResources(DeviceResources const&) = delete;
        DeviceResources& operator= (DeviceResources const&) = delete;

        void CreateDeviceResources();
        void CreateWindowSizeDependentResources();
        void SetWindow(HWND window, int width, int height) noexcept;
        bool WindowSizeChanged(int width, int height);
        void RegisterDeviceNotify(D3DDeviceNotify* deviceNotify) noexcept { m_deviceNotify = deviceNotify; }
        void Present(UINT syncInterval);

        // The accessors, and this is all of them. Each has a caller; an
        // accessor nobody calls is a promise about a member that would still
        // have to be kept.
        RECT                    GetOutputSize() const noexcept          { return m_outputSize; }
        auto                    GetD3DDevice() const noexcept           { return m_d3dDevice.Get(); }
        auto                    GetD3DDeviceContext() const noexcept    { return m_d3dContext.Get(); }
        ID3D11RenderTargetView*	GetRenderTargetView() const noexcept    { return m_d3dRenderTargetView.Get(); }
        D3D11_VIEWPORT          GetScreenViewport() const noexcept      { return m_screenViewport; }

        // Performance events
        void PIXBeginEvent(_In_z_ const wchar_t* name)
        {
            m_d3dAnnotation->BeginEvent(name);
        }

        void PIXEndEvent()
        {
            m_d3dAnnotation->EndEvent();
        }

        void PIXSetMarker(_In_z_ const wchar_t* name)
        {
            m_d3dAnnotation->SetMarker(name);
        }

    private:
        // Called from inside this class only, where the order around them is
        // kept: a device loss found by Present or ResizeBuffers, and a stale
        // factory found after a present or a resize.
        void HandleDeviceLost();
        void UpdateColorSpace();

        void CreateFactory();
        void GetHardwareAdapter(IDXGIAdapter1** ppAdapter);

        // Direct3D objects.
        Microsoft::WRL::ComPtr<IDXGIFactory2>               m_dxgiFactory;
        Microsoft::WRL::ComPtr<ID3D11Device1>               m_d3dDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext1>        m_d3dContext;
        Microsoft::WRL::ComPtr<IDXGISwapChain1>             m_swapChain;
        Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation>   m_d3dAnnotation;

        // Direct3D rendering objects.
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_renderTarget;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_d3dRenderTargetView;
        D3D11_VIEWPORT                                  m_screenViewport;

        // Direct3D properties.
        DXGI_FORMAT                                     m_backBufferFormat;

        // Cached device properties.
        HWND                                            m_window;
        RECT                                            m_outputSize;

        // Whether this OS has flip-model swap effects. Assumed until the
        // IDXGIFactory4 query in CreateDeviceResources says otherwise.
        bool                                            m_flipPresent;

        // The notify can be held directly as it owns the DeviceResources.
        D3DDeviceNotify*                                m_deviceNotify;
    };
}
