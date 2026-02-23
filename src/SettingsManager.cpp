#include "SettingsManager.h"
#include <QSettings>

class SettingsManager::Impl {
public:
  QSettings settings{QStringLiteral("Inbora"), QStringLiteral("SoundXs")};
};

SettingsManager::SettingsManager() : m_impl(new Impl) {}
SettingsManager::~SettingsManager() { delete m_impl; }

AppSettings SettingsManager::load() {
  AppSettings s;
  auto &st = m_impl->settings;
  s.inputDeviceId =
      st.value(QStringLiteral("inputDeviceId"), QString{}).toString();
  s.outputDeviceId =
      st.value(QStringLiteral("outputDeviceId"), QString{}).toString();
  s.bassDb = st.value(QStringLiteral("bassDb"), 0.0).toDouble();
  s.trebleDb = st.value(QStringLiteral("trebleDb"), 0.0).toDouble();
  s.volumeDb = st.value(QStringLiteral("volumeDb"), 0.0).toDouble();
  s.echoAmount = st.value(QStringLiteral("echoAmount"), 0.0).toDouble();
  s.preset =
      st.value(QStringLiteral("preset"), QStringLiteral("Flat")).toString();
  return s;
}

void SettingsManager::save(const AppSettings &s) {
  auto &st = m_impl->settings;
  st.setValue(QStringLiteral("inputDeviceId"), s.inputDeviceId);
  st.setValue(QStringLiteral("outputDeviceId"), s.outputDeviceId);
  st.setValue(QStringLiteral("bassDb"), s.bassDb);
  st.setValue(QStringLiteral("trebleDb"), s.trebleDb);
  st.setValue(QStringLiteral("volumeDb"), s.volumeDb);
  st.setValue(QStringLiteral("echoAmount"), s.echoAmount);
  st.setValue(QStringLiteral("preset"), s.preset);
  st.sync();
}
