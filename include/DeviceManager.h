#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>

struct DeviceInfo {
    QString id;
    QString name;
    bool isDefault = false;
};

class DeviceManager : public QObject, public IMMNotificationClient {
    Q_OBJECT
public:
    explicit DeviceManager(QObject* parent = nullptr);
    ~DeviceManager() override;

    bool initialize();
    void shutdown();

    QVector<DeviceInfo> renderDevices() const;
    QVector<DeviceInfo> captureDevices() const;

    QString defaultRenderDeviceId() const;
    QString defaultCaptureDeviceId() const;

signals:
    void devicesChanged();
    void deviceRemoved(const QString& id);

private:
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppv) override;
    ULONG   STDMETHODCALLTYPE AddRef()  override;
    ULONG   STDMETHODCALLTYPE Release() override;

    
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override;

    QVector<DeviceInfo> enumerateDevices(EDataFlow flow) const;

    IMMDeviceEnumerator* m_enumerator = nullptr;
    std::atomic<ULONG>   m_refCount{1};
};
