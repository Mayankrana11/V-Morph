#include "MainWindow.h"

namespace rtvcc {

MainWindow::MainWindow() = default;
MainWindow::~MainWindow() = default;

bool MainWindow::initialize() { return true; }
void MainWindow::run() {}
void MainWindow::shutdown() {}
void MainWindow::setAudioEngine(Application*) {}

} // namespace rtvcc