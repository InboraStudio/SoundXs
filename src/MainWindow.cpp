#include "MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QSpacerItem>
#include <QVBoxLayout>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_deviceMgr(std::make_unique<DeviceManager>(this)),
      m_audioEngine(std::make_unique<AudioEngine>(this)),
      m_settings(std::make_unique<SettingsManager>()) {
  setWindowTitle(QStringLiteral("SoundXs"));
  setFixedWidth(560);
  setMinimumHeight(640);

  applyStylesheet();
  setupUI();

  if (!m_deviceMgr->initialize()) {
    QMessageBox::critical(
        this, QStringLiteral("Error"),
        QStringLiteral("Failed to initialize audio device manager.\n"
                       "Please ensure you have audio devices connected."));
  }

  connect(m_deviceMgr.get(), &DeviceManager::devicesChanged, this,
          &MainWindow::onDevicesChanged);
  connect(m_audioEngine.get(), &AudioEngine::engineError, this,
          &MainWindow::onEngineError);

  populateDevices();
  startAudio();

  m_peakTimer = new QTimer(this);
  connect(m_peakTimer, &QTimer::timeout, this, &MainWindow::onPeakMeterTick);
  m_peakTimer->start(80); // ~12 fps update
}

MainWindow::~MainWindow() {
  m_peakTimer->stop();
  m_audioEngine->stop();
  m_deviceMgr->shutdown();
}

void MainWindow::closeEvent(QCloseEvent *event) {
  saveSettings();
  event->accept();
}
void MainWindow::applyStylesheet() {
  QFile f(QStringLiteral(":/style.qss"));
  if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    setStyleSheet(QString::fromUtf8(f.readAll()));
  }
}
void MainWindow::setupUI() {

  auto *scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scroll->verticalScrollBar()->setSingleStep(20);
  scroll->verticalScrollBar()->setPageStep(100);
  setCentralWidget(scroll);

  auto *central = new QWidget;
  central->setObjectName(QStringLiteral("centralWidget"));
  scroll->setWidget(central);

  auto *root = new QVBoxLayout(central);
  root->setContentsMargins(24, 24, 24, 24);
  root->setSpacing(24);

  auto *header = new QWidget;
  auto *hlay = new QHBoxLayout(header);
  hlay->setContentsMargins(0, 0, 0, 10);

  auto *appName = new QLabel(QStringLiteral("SoundXs"));
  appName->setObjectName(QStringLiteral("appTitle"));

  auto *tagline = new QLabel(QStringLiteral("PRECISION TONE. ZERO NOISE."));
  tagline->setObjectName(QStringLiteral("tagline"));
  tagline->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  hlay->addWidget(appName);
  hlay->addStretch();
  hlay->addWidget(tagline);
  root->addWidget(header);

  {
    auto *card = makeSectionCard(QStringLiteral("DEVICES"));
    auto *cardVLay = qobject_cast<QVBoxLayout *>(card->layout());
    auto *content = new QWidget;
    auto *grid = new QGridLayout(content);
    grid->setSpacing(10);
    grid->setContentsMargins(0, 10, 0, 5);

    auto *inLabel = new QLabel(QStringLiteral("Input"));
    auto *outLabel = new QLabel(QStringLiteral("Output"));
    inLabel->setObjectName(QStringLiteral("fieldLabel"));
    outLabel->setObjectName(QStringLiteral("fieldLabel"));
    inLabel->setFixedWidth(60);
    outLabel->setFixedWidth(60);

    m_inputCombo = new QComboBox;
    m_outputCombo = new QComboBox;
    m_inputCombo->setObjectName(QStringLiteral("deviceCombo"));
    m_outputCombo->setObjectName(QStringLiteral("deviceCombo"));

    grid->addWidget(inLabel, 0, 0);
    grid->addWidget(m_inputCombo, 0, 1);
    grid->addWidget(outLabel, 1, 0);
    grid->addWidget(m_outputCombo, 1, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnMinimumWidth(0, 60);

    cardVLay->addWidget(content);
    root->addWidget(card);
  }
  {
    auto *card = makeSectionCard(QStringLiteral("TONE CONTROLS"));
    auto *cardVLay = qobject_cast<QVBoxLayout *>(card->layout());
    cardVLay->setSpacing(12);

    auto *bassRow = makeSliderRow(QStringLiteral("Bass"), m_bassSlider,
                                  m_bassLabel, -120, 120, 0);
    auto *trebleRow = makeSliderRow(QStringLiteral("Treble"), m_trebleSlider,
                                    m_trebleLabel, -120, 120, 0);

    cardVLay->addWidget(bassRow);
    cardVLay->addWidget(trebleRow);
    root->addWidget(card);
  }
  {
    auto *card = makeSectionCard(QStringLiteral("OUTPUT"));
    auto *cardVLay = qobject_cast<QVBoxLayout *>(card->layout());
    cardVLay->setSpacing(12);

    auto *volRow = makeSliderRow(QStringLiteral("Volume"), m_volumeSlider,
                                 m_volumeLabel, -200, 100, 0);
    cardVLay->addWidget(volRow);

    auto *presetRow = new QWidget;
    auto *play = new QHBoxLayout(presetRow);
    play->setContentsMargins(0, 8, 0, 4);
    play->setSpacing(10);

    auto *preLabel = new QLabel(QStringLiteral("Preset"));
    preLabel->setObjectName(QStringLiteral("fieldLabel"));
    preLabel->setFixedWidth(60); 
    m_presetCombo = new QComboBox;
    m_presetCombo->setObjectName(QStringLiteral("deviceCombo"));
    m_presetCombo->addItems({QStringLiteral("Flat"),
                             QStringLiteral("Bass Boost"),
                             QStringLiteral("Crisp Voice"),
                             QStringLiteral("Warm"), QStringLiteral("Vivid")});

    play->addWidget(preLabel);
    play->addWidget(m_presetCombo, 1);
    play->addSpacing(70);
    cardVLay->addWidget(presetRow);

   
    auto *loopRow = new QWidget;
    auto *llay = new QHBoxLayout(loopRow);
    llay->setContentsMargins(0, 10, 0, 0);

    m_loopbackBtn = new QPushButton(QStringLiteral("Capture Desktop Audio"));
    m_loopbackBtn->setObjectName(QStringLiteral("loopbackBtn"));
    m_loopbackBtn->setCheckable(true);
    m_loopbackBtn->setFixedHeight(32);

    connect(m_loopbackBtn, &QPushButton::clicked, this,
            &MainWindow::onLoopbackToggle);

    llay->addStretch();
    llay->addWidget(m_loopbackBtn, 1);
    llay->addStretch();

    cardVLay->addWidget(loopRow);
    root->addWidget(card);
  }
 
  {
    auto *card = makeSectionCard(QStringLiteral("ENHANCER"));
    auto *cardVLay = qobject_cast<QVBoxLayout *>(card->layout());
    cardVLay->setSpacing(12);

    auto *compRow = makeSliderRow(QStringLiteral("Compress"), m_compressSlider,
                                  m_compressLabel, -400, 0, 0);
    auto *presRow = makeSliderRow(QStringLiteral("Presence"), m_presenceSlider,
                                  m_presenceLabel, -120, 120, 0);
    auto *warmRow = makeSliderRow(QStringLiteral("Warmth"), m_warmthSlider,
                                  m_warmthLabel, -120, 120, 0);
    auto *echoRow = makeSliderRow(QStringLiteral("Echo"), m_echoSlider,
                                  m_echoLabel, 0, 100, 0);

    cardVLay->addWidget(compRow);
    cardVLay->addWidget(presRow);
    cardVLay->addWidget(warmRow);
    cardVLay->addWidget(echoRow);
    root->addWidget(card);
  }

  m_peakMeter = new QProgressBar;
  m_peakMeter->setObjectName(QStringLiteral("peakMeter"));
  m_peakMeter->setRange(0, 1000);
  m_peakMeter->setValue(0);
  m_peakMeter->setTextVisible(false);
  m_peakMeter->setFixedHeight(8);
  root->addWidget(m_peakMeter);
  auto *footer = new QLabel(QStringLiteral("by Inbora  •  v1.0"));
  footer->setObjectName(QStringLiteral("footer"));
  footer->setAlignment(Qt::AlignCenter);
  root->addWidget(footer);
  connect(m_inputCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &MainWindow::onInputDeviceChanged);
  connect(m_outputCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &MainWindow::onOutputDeviceChanged);
  connect(m_bassSlider, &QSlider::valueChanged, this,
          &MainWindow::onBassChanged);
  connect(m_trebleSlider, &QSlider::valueChanged, this,
          &MainWindow::onTrebleChanged);
  connect(m_volumeSlider, &QSlider::valueChanged, this,
          &MainWindow::onVolumeChanged);
  connect(m_compressSlider, &QSlider::valueChanged, this,
          &MainWindow::onCompressChanged);
  connect(m_presenceSlider, &QSlider::valueChanged, this,
          &MainWindow::onPresenceChanged);
  connect(m_warmthSlider, &QSlider::valueChanged, this,
          &MainWindow::onWarmthChanged);
  connect(m_echoSlider, &QSlider::valueChanged, this,
          &MainWindow::onEchoChanged);
  connect(m_presetCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &MainWindow::onPresetChanged);
}
QWidget *MainWindow::makeSectionCard(const QString &title) {
  auto *card = new QWidget;
  card->setObjectName(QStringLiteral("card"));
  auto *lay = new QVBoxLayout(card);
  lay->setContentsMargins(20, 14, 20, 16);
  lay->setSpacing(0);

  auto *titleLabel = new QLabel(title);
  titleLabel->setObjectName(QStringLiteral("sectionTitle"));
  lay->addWidget(titleLabel);
  return card;
}

QWidget *MainWindow::makeSliderRow(const QString &label, QSlider *&sliderOut,
                                   QLabel *&dbLabelOut, int minVal, int maxVal,
                                   int defaultVal) {
  auto *row = new QWidget;
  auto *lay = new QHBoxLayout(row);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(10);

  auto *nameLabel = new QLabel(label);
  nameLabel->setObjectName(QStringLiteral("fieldLabel"));
  nameLabel->setFixedWidth(60);

  sliderOut = new QSlider(Qt::Horizontal);
  sliderOut->setRange(minVal, maxVal);
  sliderOut->setValue(defaultVal);
  sliderOut->setSingleStep(1);
  sliderOut->setPageStep(10);
  sliderOut->setObjectName(QStringLiteral("dspSlider"));

  dbLabelOut = new QLabel(QStringLiteral("0.0 dB"));
  dbLabelOut->setObjectName(QStringLiteral("dbLabel"));
  dbLabelOut->setFixedWidth(70);
  dbLabelOut->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  lay->addWidget(nameLabel);
  lay->addWidget(sliderOut, 1);
  lay->addWidget(dbLabelOut);
  return row;
}
void MainWindow::populateDevices() {
  m_loading = true;

  AppSettings saved = m_settings->load();
  m_inputCombo->clear();
  m_inputCombo->addItem(QStringLiteral("Default Input"), QString{});
  const auto captures = m_deviceMgr->captureDevices();
  int selIn = 0;
  for (const auto &d : captures) {
    m_inputCombo->addItem(d.name, d.id);
    if (d.id == saved.inputDeviceId)
      selIn = m_inputCombo->count() - 1;
  }
  m_inputCombo->setCurrentIndex(selIn);
  m_outputCombo->clear();
  m_outputCombo->addItem(QStringLiteral("Default Output"), QString{});
  const auto renders = m_deviceMgr->renderDevices();
  int selOut = 0;
  for (const auto &d : renders) {
    m_outputCombo->addItem(d.name, d.id);
    if (d.id == saved.outputDeviceId)
      selOut = m_outputCombo->count() - 1;
  }
  m_outputCombo->setCurrentIndex(selOut);
  m_bassSlider->setValue(static_cast<int>(saved.bassDb * 10.0));
  m_trebleSlider->setValue(static_cast<int>(saved.trebleDb * 10.0));
  m_volumeSlider->setValue(static_cast<int>(saved.volumeDb * 10.0));

  auto updateLabel = [](QLabel *lbl, int raw) {
    double db = raw / 10.0;
    lbl->setText(QString::asprintf("%+.1f dB", db));
  };
  updateLabel(m_bassLabel, m_bassSlider->value());
  updateLabel(m_trebleLabel, m_trebleSlider->value());
  updateLabel(m_volumeLabel, m_volumeSlider->value());
  int pi = m_presetCombo->findText(saved.preset);
  if (pi >= 0)
    m_presetCombo->setCurrentIndex(pi);

  m_loading = false;
}
void MainWindow::startAudio() {
  QString inId = m_inputCombo->currentData().toString();
  QString outId = m_outputCombo->currentData().toString();
  m_audioEngine->dsp()->setBass(m_bassSlider->value() / 10.0);
  m_audioEngine->dsp()->setTreble(m_trebleSlider->value() / 10.0);
  m_audioEngine->dsp()->setVolume(m_volumeSlider->value() / 10.0);
  m_audioEngine->dsp()->setPresence(m_presenceSlider->value() / 10.0);
  m_audioEngine->dsp()->setWarmth(m_warmthSlider->value() / 10.0);
  m_audioEngine->dsp()->setCompressor(m_compressSlider->value() / 10.0);

  m_audioEngine->start(inId, outId);
}
void MainWindow::onInputDeviceChanged(int) {
  if (m_loading)
    return;
  m_audioEngine->setInputDevice(m_inputCombo->currentData().toString());
  saveSettings();
}

void MainWindow::onOutputDeviceChanged(int) {
  if (m_loading)
    return;
  m_audioEngine->setOutputDevice(m_outputCombo->currentData().toString());
  saveSettings();
}

void MainWindow::onBassChanged(int value) {
  double db = value / 10.0;
  m_bassLabel->setText(QString::asprintf("%+.1f dB", db));
  m_audioEngine->dsp()->setBass(db);
  if (!m_loading)
    saveSettings();
}

void MainWindow::onTrebleChanged(int value) {
  double db = value / 10.0;
  m_trebleLabel->setText(QString::asprintf("%+.1f dB", db));
  m_audioEngine->dsp()->setTreble(db);
  if (!m_loading)
    saveSettings();
}

void MainWindow::onVolumeChanged(int value) {
  double db = value / 10.0;
  m_volumeLabel->setText(QString::asprintf("%+.1f dB", db));
  m_audioEngine->dsp()->setVolume(db);
  if (!m_loading)
    saveSettings();
}

void MainWindow::onCompressChanged(int value) {
  double db = value / 10.0; // 0 to -40 dB threshold
  m_compressLabel->setText(QString::asprintf("%.0f dB", db));
  m_audioEngine->dsp()->setCompressor(db);
  if (!m_loading)
    saveSettings();
}

void MainWindow::onPresenceChanged(int value) {
  double db = value / 10.0;
  m_presenceLabel->setText(QString::asprintf("%+.1f dB", db));
  m_audioEngine->dsp()->setPresence(db);
  if (!m_loading)
    saveSettings();
}

void MainWindow::onWarmthChanged(int value) {
  double db = value / 10.0;
  m_warmthLabel->setText(QString::asprintf("%+.1f dB", db));
  m_audioEngine->dsp()->setWarmth(db);
  if (!m_loading)
    saveSettings();
}

void MainWindow::onEchoChanged(int value) {
  m_echoLabel->setText(QString::asprintf("%d %%", value));
  m_audioEngine->dsp()->setEcho(
      value / 10.0);
  if (!m_loading)
    saveSettings();
}

void MainWindow::onPresetChanged(int index) {
  if (m_loading)
    return;
  applyPreset(m_presetCombo->itemText(index));
}

struct AppInfo {
  QString title;
  uint32_t pid;
};

BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
  if (!IsWindowVisible(hwnd))
    return TRUE;

  WCHAR title[256];
  GetWindowTextW(hwnd, title, 256);
  if (wcslen(title) == 0)
    return TRUE;

  
  if (GetWindow(hwnd, GW_OWNER) != (HWND)0)
    return TRUE;

  DWORD pid;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid == GetCurrentProcessId())
    return TRUE; 

  auto *list = reinterpret_cast<QList<AppInfo> *>(lParam);
  list->append(
      {QString::fromWCharArray((const wchar_t *)title), (uint32_t)pid});
  return TRUE;
}

void MainWindow::onLoopbackToggle() {
  if (!m_audioEngine)
    return;

  if (m_loopbackBtn->isChecked()) {
    QList<AppInfo> apps;
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&apps));

    if (apps.isEmpty()) {
      QMessageBox::information(this, QStringLiteral("SoundXs"),
                               QStringLiteral("No active windows found."));
      m_loopbackBtn->setChecked(false);
      return;
    }

    QStringList items;
    for (const auto &app : apps)
      items << app.title;

    bool ok;
    QString item = QInputDialog::getItem(
        this, QStringLiteral("Select Source"),
        QStringLiteral("Which app do you want to capture?"), items, 0, false,
        &ok);

    if (ok && !item.isEmpty()) {
      uint32_t selectedPid = 0;
      for (const auto &app : apps) {
        if (app.title == item) {
          selectedPid = app.pid;
          break;
        }
      }

      m_audioEngine->setLoopbackEnabled(true, selectedPid);
      m_loopbackBtn->setText(item.left(15) + QStringLiteral("... ON"));
    } else {
      m_loopbackBtn->setChecked(false);
    }
  } else {
    m_audioEngine->setLoopbackEnabled(false, 0);
    m_loopbackBtn->setText(QStringLiteral("Capture Desktop Audio"));
  }
  saveSettings();
}

void MainWindow::applyPreset(const QString &name) {
  m_loading = true;
  if (name == QStringLiteral("Flat")) {
    m_bassSlider->setValue(0);
    m_trebleSlider->setValue(0);
    m_volumeSlider->setValue(0);
    m_echoSlider->setValue(0);
  } else if (name == QStringLiteral("Bass Boost")) {
    m_bassSlider->setValue(80);    // +8 dB
    m_trebleSlider->setValue(-20); // -2 dB
    m_volumeSlider->setValue(-30); // -3 dB
  } else if (name == QStringLiteral("Crisp Voice")) {
    m_bassSlider->setValue(-30);  // -3 dB
    m_trebleSlider->setValue(50); // +5 dB
    m_volumeSlider->setValue(0);
  } else if (name == QStringLiteral("Warm")) {
    m_bassSlider->setValue(40);    // +4 dB
    m_trebleSlider->setValue(-40); // -4 dB
    m_volumeSlider->setValue(0);
  } else if (name == QStringLiteral("Vivid")) {
    m_bassSlider->setValue(30);   // +3 dB
    m_trebleSlider->setValue(40); // +4 dB
    m_volumeSlider->setValue(0);
  }
  m_loading = false;
  m_audioEngine->dsp()->setBass(m_bassSlider->value() / 10.0);
  m_audioEngine->dsp()->setTreble(m_trebleSlider->value() / 10.0);
  m_audioEngine->dsp()->setVolume(m_volumeSlider->value() / 10.0);
  m_bassLabel->setText(
      QString::asprintf("%+.1f dB", m_bassSlider->value() / 10.0));
  m_trebleLabel->setText(
      QString::asprintf("%+.1f dB", m_trebleSlider->value() / 10.0));
  m_volumeLabel->setText(
      QString::asprintf("%+.1f dB", m_volumeSlider->value() / 10.0));
  m_audioEngine->dsp()->setEcho(m_echoSlider->value() / 10.0);
  m_echoLabel->setText(QString::asprintf("%d %%", m_echoSlider->value()));
}

void MainWindow::onDevicesChanged() {
  bool wasRunning = m_audioEngine->isRunning();
  if (wasRunning)
    m_audioEngine->stop();
  populateDevices();
  if (wasRunning)
    startAudio();
}

void MainWindow::onPeakMeterTick() {
  float peak = m_audioEngine->peakLevel();
  m_peakMeter->setValue(static_cast<int>(peak * 1000.0f));
  if (peak > 0.9f) {
    m_peakMeter->setObjectName(QStringLiteral("peakMeterClip"));
  } else if (peak > 0.7f) {
    m_peakMeter->setObjectName(QStringLiteral("peakMeterWarn"));
  } else {
    m_peakMeter->setObjectName(QStringLiteral("peakMeter"));
  }
  m_peakMeter->style()->unpolish(m_peakMeter);
  m_peakMeter->style()->polish(m_peakMeter);
}

void MainWindow::onEngineError(const QString &msg) {
  QMessageBox::warning(this, QStringLiteral("Audio Engine"), msg);
}

void MainWindow::saveSettings() {
  AppSettings s;
  s.inputDeviceId = m_inputCombo->currentData().toString();
  s.outputDeviceId = m_outputCombo->currentData().toString();
  s.bassDb = m_bassSlider->value() / 10.0;
  s.trebleDb = m_trebleSlider->value() / 10.0;
  s.volumeDb = m_volumeSlider->value() / 10.0;
  s.preset = m_presetCombo->currentText();
  m_settings->save(s);
}
