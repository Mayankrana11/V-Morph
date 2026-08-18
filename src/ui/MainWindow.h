#pragma once

#include <string>

namespace rtvcc {

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool initialize();
    void run();
    void shutdown();

    void setAudioEngine(class Application* app);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace rtvcc