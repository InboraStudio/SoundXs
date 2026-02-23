#include "MainWindow.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
#include <QPalette>
#include <QPixmap>

int main(int argc, char *argv[]) {

  CoInitializeEx(nullptr, COINIT_MULTITHREADED);

  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("SoundXs"));
  app.setApplicationVersion(QStringLiteral("1.0.0"));
  app.setOrganizationName(QStringLiteral("Inbora"));

  QPalette pal;
  pal.setColor(QPalette::Window, QColor(0x0F, 0x0F, 0x12));
  pal.setColor(QPalette::WindowText, Qt::white);
  pal.setColor(QPalette::Base, QColor(0x1A, 0x1A, 0x22));
  pal.setColor(QPalette::AlternateBase, QColor(0x12, 0x12, 0x18));
  pal.setColor(QPalette::Text, Qt::white);
  pal.setColor(QPalette::Button, QColor(0x1A, 0x1A, 0x22));
  pal.setColor(QPalette::ButtonText, Qt::white);
  pal.setColor(QPalette::Highlight, QColor(0x3A, 0x86, 0xFF));
  pal.setColor(QPalette::HighlightedText, Qt::white);
  app.setPalette(pal);

  QIcon appIcon;
  appIcon.addPixmap(QPixmap(QStringLiteral(":/logo.png")));
  app.setWindowIcon(appIcon);

  MainWindow win;
  win.setWindowIcon(appIcon);
  win.show();

  int result = app.exec();
  CoUninitialize();
  return result;
}
