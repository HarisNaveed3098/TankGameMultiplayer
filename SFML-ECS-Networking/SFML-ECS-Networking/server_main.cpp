#include <SFML/Network.hpp>
#include <iostream>
#include <string>
#include <thread>
#include "game_server.h"
#include "utils.h"

int main() {
    Utils::printMsg("Tank Game Server Starting...");

    // Get server port
    unsigned short port = 53000;
    std::cout << "Enter server port (default 53000): ";
    std::string input;
    std::getline(std::cin, input);

    if (!input.empty()) {
        try {
            port = static_cast<unsigned short>(std::stoi(input));
        }
        catch (const std::exception&) {
            Utils::printMsg("Invalid port, using default 53000", warning);
            port = 53000;
        }
    }

    // Create and initialize server
    GameServer server(port);
    if (!server.Initialize()) {
        Utils::printMsg("Failed to initialize server", error);
        return -1;
    }

    Utils::printMsg("Server running. Press Enter to stop server...");

    // Get local IP address with proper optional handling
    auto localIP = sf::IpAddress::getLocalAddress();
    if (localIP) {
        Utils::printMsg("Players can connect to: " + localIP.value().toString() + ":" + std::to_string(port));
    }
    else {
        Utils::printMsg("Players can connect to: localhost:" + std::to_string(port));
    }

    // Server update loop
    sf::Clock clock;
    bool running = true;

    // Start a thread to check for input (simple way to stop server)
    std::thread inputThread([&running]() {
        std::string input;
        std::getline(std::cin, input);
        running = false;
        });

    while (running && server.IsRunning()) {
        float deltaTime = clock.restart().asSeconds();
        server.Update(deltaTime);

        // Small sleep to prevent 100% CPU usage
        sf::sleep(sf::milliseconds(1));
    }

    // Clean shutdown
    Utils::printMsg("Shutting down server...", warning);
    server.Shutdown();

    // Wait for input thread to finish
    if (inputThread.joinable()) {
        inputThread.join();
    }

    Utils::printMsg("Server stopped", success);
    return 0;
}