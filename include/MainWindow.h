#pragma once
#include <QComboBox>
#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <memory>

#include "AudioEngine.h"
#include "DeviceManager.h"
#include "SettingsManager.h"

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void onInputDeviceChanged(int index);
  void onOutputDeviceChanged(int index);
  void onBassChanged(int value);
  void onTrebleChanged(int value);
  void onVolumeChanged(int value);
  void onCompressChanged(int value);
  void onPresenceChanged(int value);
  void onWarmthChanged(int value);
  void onEchoChanged(int value);
  void onPresetChanged(int index);
  void onLoopbackToggle();
  void onDevicesChanged();
  void onPeakMeterTick();
  void onEngineError(const QString &msg);

private:
  void setupUI();
  void applyStylesheet();
  void populateDevices();
  void startAudio();
  void applyPreset(const QString &name);
  void saveSettings();

  QWidget *makeSectionCard(const QString &title);
  QWidget *makeSliderRow(const QString &label, QSlider *&sliderOut,
                         QLabel *&dbLabelOut, int minVal, int maxVal,
                         int defaultVal);

  std::unique_ptr<DeviceManager> m_deviceMgr;
  std::unique_ptr<AudioEngine> m_audioEngine;
  std::unique_ptr<SettingsManager> m_settings;

  QComboBox *m_inputCombo = nullptr;
  QComboBox *m_outputCombo = nullptr;
  QSlider *m_bassSlider = nullptr;
  QSlider *m_trebleSlider = nullptr;
  QSlider *m_volumeSlider = nullptr;
  QSlider *m_compressSlider = nullptr;
  QSlider *m_presenceSlider = nullptr;
  QSlider *m_warmthSlider = nullptr;
  QSlider *m_echoSlider = nullptr;
  QLabel *m_bassLabel = nullptr;
  QLabel *m_trebleLabel = nullptr;
  QLabel *m_volumeLabel = nullptr;
  QLabel *m_compressLabel = nullptr;
  QLabel *m_presenceLabel = nullptr;
  QLabel *m_warmthLabel = nullptr;
  QLabel *m_echoLabel = nullptr;
  QComboBox *m_presetCombo = nullptr;
  QPushButton *m_loopbackBtn = nullptr;
  QProgressBar *m_peakMeter = nullptr;
  QTimer *m_peakTimer = nullptr;

  bool m_loading = false;
};
