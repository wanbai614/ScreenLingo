#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <QtCore/QVector>
#include "ICaptureEngine.h"

using Microsoft::WRL::ComPtr;

struct MonitorDuplicator {
    HMONITOR                     handle;
    ComPtr<ID3D11Device>         device;
    ComPtr<ID3D11DeviceContext>  context;
    ComPtr<IDXGIOutput1>         output;
    ComPtr<IDXGIOutputDuplication> duplicator;
    UINT                         outputIndex;
    DXGI_OUTPUT_DESC             desc;
};

class DxgiCaptureEngine : public ICaptureEngine {
    Q_OBJECT

public:
    explicit DxgiCaptureEngine(QObject* parent = nullptr);
    ~DxgiCaptureEngine() override;

    bool initialize() override;
    QImage captureRegion(const QRect& region, int screenIndex) override;
    bool hasChanged(const QRect& region, int screenIndex) override;
    void shutdown() override;

private:
    bool initDuplicator(int screenIndex);
    bool recreateDuplicator(int screenIndex);

    QVector<MonitorDuplicator> m_duplicators;
    ComPtr<IDXGIFactory2>      m_factory;
    bool                       m_initialized = false;
};
