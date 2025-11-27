#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <chrono>

#include "led_driver.h"
#include "ipc_server.h"

// Global flag for clean shutdown
std::atomic<bool> running(true);

void signalHandler(int)
{
    std::cout << "\n[MAIN] Shutdown requested…" << std::endl;
    running = false;
}

int main()
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "==== Ambilight System Starting ====\n";

    // -------------------------------------------------------
    // 1. Create main components
    // -------------------------------------------------------

    const int NUM_LEDS = 60;

    LEDController led(NUM_LEDS, "/dev/spidev0.0");
    AmbientProcessor ambient(NUM_LEDS);
    PythonInterface py("/tmp/ambilight_socket");

    // -------------------------------------------------------
    // 2. Start Python IPC + Webserver in threads
    // -------------------------------------------------------

    std::thread pythonThread([&]() {
        if (!py.start())
        {
            std::cerr << "[PY] Failed to start Python Interface.\n";
            return;
        }

        while (running)
        {
            py.poll();   // handles incoming messages
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        py.stop();
    });

    // IPC-Server starten
    std::thread ipcThread([&driver]() {
        runIPCServer(&driver);
    });

    // -------------------------------------------------------
    // 3. Main LED loop
    // -------------------------------------------------------
    
    std::cout << "[MAIN] System running.\n";

    while (running)
    {
        // -----------------------------------------
        // (A) Capture Frame (hier erstmal nur dummy)
        // -----------------------------------------

        const int WIDTH = 32;
        const int HEIGHT = 18;
        static uint8_t dummyFrame[WIDTH * HEIGHT * 3];

        // Fake animation pattern
        static int t = 0;
        for (int i = 0; i < WIDTH * HEIGHT * 3; i += 3)
        {
            dummyFrame[i + 0] = (uint8_t)((sin(t * 0.05) * 0.5 + 0.5) * 255);
            dummyFrame[i + 1] = (uint8_t)((cos(t * 0.07) * 0.5 + 0.5) * 255);
            dummyFrame[i + 2] = 0;
        }
        t++;

        // -----------------------------------------
        // (B) Compute LED colors
        // -----------------------------------------
        std::vector<RGB> ledColors =
            ambient.processFrame(dummyFrame, WIDTH, HEIGHT);

        // -----------------------------------------
        // (C) LED → SPI output
        // -----------------------------------------
        led.setPixels(ledColors);
        led.send();

        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }

    // -------------------------------------------------------
    // 4. Shutdown
    // -------------------------------------------------------
    std::cout << "[MAIN] Stopping…\n";

    pythonThread.join();
    ipcThread.join();

    led.clear();
    led.send();

    std::cout << "==== Ambilight System Stopped ====\n";
    return 0;
}
