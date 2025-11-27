// cpp/src/ipc_server.cpp
#include "ipc_server.h"
#include "led_driver.h"

#include <thread>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

static constexpr int PORT = 9000;

void runIPCServer(LEDDriver* driver) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;   // 0.0.0.0
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return;
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        return;
    }

    std::cout << "[IPC] Listening on port " << PORT << std::endl;

    while (true) {
        int client = accept(server_fd, nullptr, nullptr);
        if (client < 0) continue;

        std::thread([driver, client]() {
            char buf[1024];
            std::string cmd;

            while (true) {
                ssize_t n = read(client, buf, sizeof(buf));
                if (n <= 0) break;

                cmd.append(buf, n);

                // split by newline (Python sends "\n")
                size_t pos;
                while ((pos = cmd.find('\n')) != std::string::npos) {
                    std::string line = cmd.substr(0, pos);
                    cmd.erase(0, pos+1);

                    driver->handleCommand(line);
                }
            }

            close(client);
        }).detach();
    }

    close(server_fd);
}
