#include "DxgiCaptureEngine.h"
#include <QtCore/QDebug>
#include <QtGui/QPainter>

DxgiCaptureEngine::DxgiCaptureEngine(QObject* parent)
    : ICaptureEngine(parent) {}

DxgiCaptureEngine::~DxgiCaptureEngine() {
    shutdown();
}

bool DxgiCaptureEngine::initialize() {
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&m_factory));
    if (FAILED(hr)) {
        emit captureError("Failed to create DXGIFactory");
        return false;
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0;
         m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++i) {
        ComPtr<IDXGIOutput> output;
        for (UINT j = 0;
             adapter->EnumOutputs(j, &output) != DXGI_ERROR_NOT_FOUND;
             ++j) {
            ComPtr<IDXGIOutput1> output1;
            hr = output.As(&output1);
            if (FAILED(hr)) continue;

            DXGI_OUTPUT_DESC desc;
            output1->GetDesc(&desc);

            if (!desc.AttachedToDesktop) continue;

            MonitorDuplicator dup;
            dup.handle      = desc.Monitor;
            dup.output      = output1;
            dup.desc        = desc;
            dup.outputIndex = j;

            m_duplicators.append(dup);
        }
    }

    for (int i = 0; i < m_duplicators.size(); ++i) {
        if (!initDuplicator(i)) {
            emit captureError(QString("Failed to init duplicator for monitor %1").arg(i));
            return false;
        }
    }

    m_initialized = true;
    return true;
}

bool DxgiCaptureEngine::initDuplicator(int screenIndex) {
    if (screenIndex < 0 || screenIndex >= m_duplicators.size()) return false;
    auto& dup = m_duplicators[screenIndex];

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0
    };

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevels, ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION, &dup.device, nullptr, &dup.context);

    if (FAILED(hr)) return false;

    hr = dup.output->DuplicateOutput(dup.device.Get(), &dup.duplicator);
    if (FAILED(hr)) return false;

    return true;
}

bool DxgiCaptureEngine::recreateDuplicator(int screenIndex) {
    auto& dup = m_duplicators[screenIndex];
    dup.duplicator.Reset();
    return SUCCEEDED(dup.output->DuplicateOutput(dup.device.Get(), &dup.duplicator));
}

bool DxgiCaptureEngine::hasChanged(const QRect& /*region*/, int screenIndex) {
    if (!m_initialized) return false;
    if (screenIndex < 0 || screenIndex >= m_duplicators.size()) return false;
    auto& dup = m_duplicators[screenIndex];
    if (!dup.duplicator) return false;

    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    ComPtr<IDXGIResource> resource;
    HRESULT hr = dup.duplicator->AcquireNextFrame(0, &frameInfo, &resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) return false;
    if (FAILED(hr)) {
        recreateDuplicator(screenIndex);
        return false;
    }
    dup.duplicator->ReleaseFrame();
    return frameInfo.AccumulatedFrames > 0;
}

QImage DxgiCaptureEngine::captureRegion(const QRect& region, int screenIndex) {
    if (!m_initialized || screenIndex < 0 || screenIndex >= m_duplicators.size())
        return QImage();
    if (region.isEmpty()) return QImage();

    auto& dup = m_duplicators[screenIndex];

    // Clamp region to monitor-local bounds (D3D11 texture origin is
    // always (0,0) at the monitor top-left, unlike DesktopCoordinates
    // which uses virtual-screen coordinates for multi-monitor setups).
    RECT mon = dup.desc.DesktopCoordinates;
    QRect monitorRect(0, 0,
                      mon.right - mon.left, mon.bottom - mon.top);
    QRect clamped = region.intersected(monitorRect);
    if (clamped.isEmpty()) return QImage();

    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    ComPtr<IDXGIResource> resource;
    HRESULT hr = dup.duplicator->AcquireNextFrame(100, &frameInfo, &resource);
    if (FAILED(hr)) {
        recreateDuplicator(screenIndex);
        return QImage();
    }

    ComPtr<ID3D11Texture2D> texture;
    hr = resource.As(&texture);
    if (FAILED(hr)) {
        dup.duplicator->ReleaseFrame();
        return QImage();
    }

    D3D11_TEXTURE2D_DESC texDesc;
    texture->GetDesc(&texDesc);

    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width              = clamped.width();
    stagingDesc.Height             = clamped.height();
    stagingDesc.MipLevels          = 1;
    stagingDesc.ArraySize          = 1;
    stagingDesc.Format             = texDesc.Format;
    stagingDesc.SampleDesc.Count   = 1;
    stagingDesc.Usage              = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags     = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> stagingTexture;
    hr = dup.device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) {
        dup.duplicator->ReleaseFrame();
        return QImage();
    }

    D3D11_BOX box = {
        static_cast<UINT>(clamped.x()),
        static_cast<UINT>(clamped.y()), 0,
        static_cast<UINT>(clamped.x() + clamped.width()),
        static_cast<UINT>(clamped.y() + clamped.height()), 1
    };
    dup.context->CopySubresourceRegion(stagingTexture.Get(), 0, 0, 0, 0,
                                       texture.Get(), 0, &box);

    dup.duplicator->ReleaseFrame();

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = dup.context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return QImage();

    QImage img(clamped.width(), clamped.height(), QImage::Format_ARGB32);
    auto* dst = img.bits();
    auto* src = static_cast<const uchar*>(mapped.pData);
    for (int y = 0; y < clamped.height(); ++y) {
        for (int x = 0; x < clamped.width(); ++x) {
            size_t offset = y * mapped.RowPitch + x * 4;
            int b = src[offset];
            int g = src[offset + 1];
            int r = src[offset + 2];
            int a = 255;
            auto* pixel = dst + y * img.bytesPerLine() + x * 4;
            pixel[0] = b;
            pixel[1] = g;
            pixel[2] = r;
            pixel[3] = a;
        }
    }

    dup.context->Unmap(stagingTexture.Get(), 0);
    return img;
}

void DxgiCaptureEngine::shutdown() {
    for (auto& dup : m_duplicators) {
        dup.duplicator.Reset();
        dup.context.Reset();
        dup.device.Reset();
    }
    m_duplicators.clear();
    m_factory.Reset();
    m_initialized = false;
}
