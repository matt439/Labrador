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
    // Named D3DDeviceNotify rather than DeviceNotify, which is now the seam's
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
    // TRIMMED TO WHAT THIS ENGINE ASKS FOR. Three capabilities came from the
    // upstream sample this file started as, and each was configured off at the
    // only construction site while its machinery still ran. They are named here
    // rather than left for the next reader to prove dead a second time.
    //
    //  - No depth buffer. The renderer draws 2D and nothing else, so the depth
    //    format parameter, the depth texture, its view and its per-frame
    //    DiscardView are gone rather than passed DXGI_FORMAT_UNKNOWN and
    //    skipped on every path.
    //  - No tearing. Nothing set the flag, so an IDXGIFactory5 probe measured a
    //    capability no Present could reach. Present is vsync-locked.
    //  - No HDR. Nothing set that flag either, and the window/output
    //    intersection walk it guarded - every adapter, every output, on every
    //    display change and every no-op resize - computed a value read only
    //    inside the branch the flag turns on.
    //
    // What is left of the options word is one runtime fact, whether this OS has
    // flip-model swap effects, so it is a bool and not a flags word with a
    // single permanently-set flag.
    class DeviceResources
    {
    public:
        // minFeatureLevel is a floor, and CreateDeviceResources truncates the
        // level array at it - so this default is the engine's real floor, and
        // the 9_3, 9_2 and 9_1 entries in that array are never requested while
        // nothing passes anything lower. Worth knowing before treating the
        // array as a statement of reach.
        DeviceResources(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
                        UINT backBufferCount = 2,
                        D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_10_0) noexcept;
        ~DeviceResources() = default;

        DeviceResources(DeviceResources&&) = default;
        DeviceResources& operator= (DeviceResources&&) = default;

        DeviceResources(DeviceResources const&) = delete;
        DeviceResources& operator= (DeviceResources const&) = delete;

        void CreateDeviceResources();
        void CreateWindowSizeDependentResources();
        void SetWindow(HWND window, int width, int height) noexcept;
        bool WindowSizeChanged(int width, int height);
        void HandleDeviceLost();
        void RegisterDeviceNotify(D3DDeviceNotify* deviceNotify) noexcept { m_deviceNotify = deviceNotify; }
        void Present();
        void UpdateColorSpace();

        // The accessors, and this is all of them.
        //
        // There were seventeen. Twelve had no caller anywhere in the repository
        // - the swap chain, the factory, the window, the feature level, the
        // back buffer texture, its format and count, the colour space, the
        // options word, and the three depth-stencil ones whose members no
        // longer exist. An accessor nobody calls is a promise about a member
        // that a second backend would have to keep.
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
        UINT                                            m_backBufferCount;
        D3D_FEATURE_LEVEL                               m_d3dMinFeatureLevel;

        // Cached device properties.
        HWND                                            m_window;
        D3D_FEATURE_LEVEL                               m_d3dFeatureLevel;
        RECT                                            m_outputSize;

        // Whether this OS has flip-model swap effects. Assumed until the
        // IDXGIFactory4 query in CreateDeviceResources says otherwise.
        bool                                            m_flipPresent;

        // The notify can be held directly as it owns the DeviceResources.
        D3DDeviceNotify*                                m_deviceNotify;
    };
}
