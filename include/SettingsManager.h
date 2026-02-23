#pragma once
#include <QString>

struct AppSettings {
  QString inputDeviceId;
  QString outputDeviceId;
  double bassDb = 0.0;
  double trebleDb = 0.0;
  double volumeDb = 0.0;
  double echoAmount = 0.0;
  QString preset = QStringLiteral("Flat");
};

class SettingsManager {
public:
  SettingsManager();
  ~SettingsManager();

  AppSettings load();
  void save(const AppSettings &s);

private:
  class Impl;
  Impl *m_impl = nullptr;
};
