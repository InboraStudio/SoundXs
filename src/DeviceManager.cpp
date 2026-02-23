#include "DeviceManager.h"

#define INITGUID
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <QString>
#include <QMetaObject>

DeviceManager::DeviceManager(QObject* parent)
    : QObject(parent) {}

DeviceManager::~DeviceManager() {
    shutdown();
}

bool DeviceManager::initialize() {
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&m_enumerator)
    );
    if (FAILED(hr)) return false;

    m_enumerator->RegisterEndpointNotificationCallback(this);
    return true;
}

void DeviceManager::shutdown() {
    if (m_enumerator) {
        m_enumerator->UnregisterEndpointNotificationCallback(this);
        m_enumerator->Release();
        m_enumerator = nullptr;
    }
}
QVector<DeviceInfo> DeviceManager::enumerateDevices(EDataFlow flow) const {
    QVector<DeviceInfo> result;
    if (!m_enumerator) return result;

    IMMDeviceCollection* collection = nullptr;
    if (FAILED(m_enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &collection)))
        return result;

    IMMDevice* defaultDevice = nullptr;
    LPWSTR defaultId = nullptr;
    if (SUCCEEDED(m_enumerator->GetDefaultAudioEndpoint(flow, eConsole, &defaultDevice))) {
        defaultDevice->GetId(&defaultId);
        defaultDevice->Release();
    }

    UINT count = 0;
    collection->GetCount(&count);

    for (UINT i = 0; i < count; ++i) {
        IMMDevice* device = nullptr;
        if (FAILED(collection->Item(i, &device))) continue;

        LPWSTR deviceId = nullptr;
        device->GetId(&deviceId);

        IPropertyStore* store = nullptr;
        QString friendlyName = QStringLiteral("Unknown Device");
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store))) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &pv))) {
                friendlyName = QString::fromWCharArray(pv.pwszVal);
                PropVariantClear(&pv);
            }
            store->Release();
        }

        DeviceInfo info;
        info.id        = QString::fromWCharArray(deviceId);
        info.name      = friendlyName;
        info.isDefault = (defaultId && wcscmp(deviceId, defaultId) == 0);

        result.append(info);

        CoTaskMemFree(deviceId);
        device->Release();
    }

    if (defaultId) CoTaskMemFree(defaultId);
    collection->Release();
    return result;
}

QVector<DeviceInfo> DeviceManager::renderDevices()  const { return enumerateDevices(eRender);  }
QVector<DeviceInfo> DeviceManager::captureDevices() const { return enumerateDevices(eCapture); }

QString DeviceManager::defaultRenderDeviceId() const {
    if (!m_enumerator) return {};
    IMMDevice* dev = nullptr;
    if (FAILED(m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &dev)))
        return {};
    LPWSTR id = nullptr;
    dev->GetId(&id);
    QString result = QString::fromWCharArray(id);
    CoTaskMemFree(id);
    dev->Release();
    return result;
}

QString DeviceManager::defaultCaptureDeviceId() const {
    if (!m_enumerator) return {};
    IMMDevice* dev = nullptr;
    if (FAILED(m_enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &dev)))
        return {};
    LPWSTR id = nullptr;
    dev->GetId(&id);
    QString result = QString::fromWCharArray(id);
    CoTaskMemFree(id);
    dev->Release();
    return result;
}

HRESULT STDMETHODCALLTYPE DeviceManager::QueryInterface(REFIID iid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient)) {
        *ppv = static_cast<IMMNotificationClient*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DeviceManager::AddRef()  { return m_refCount.fetch_add(1) + 1; }
ULONG STDMETHODCALLTYPE DeviceManager::Release() { return m_refCount.fetch_sub(1) - 1; }

HRESULT STDMETHODCALLTYPE DeviceManager::OnDeviceStateChanged(LPCWSTR, DWORD) {
    QMetaObject::invokeMethod(this, &DeviceManager::devicesChanged, Qt::QueuedConnection);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceManager::OnDeviceAdded(LPCWSTR) {
    QMetaObject::invokeMethod(this, &DeviceManager::devicesChanged, Qt::QueuedConnection);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceManager::OnDeviceRemoved(LPCWSTR pwstrDeviceId) {
    QString id = QString::fromWCharArray(pwstrDeviceId);
    QMetaObject::invokeMethod(this, [this, id]() {
        emit deviceRemoved(id);
        emit devicesChanged();
    }, Qt::QueuedConnection);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceManager::OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR) {
    QMetaObject::invokeMethod(this, &DeviceManager::devicesChanged, Qt::QueuedConnection);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceManager::OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) {
    return S_OK;
}
