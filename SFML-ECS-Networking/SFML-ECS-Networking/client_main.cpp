#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <string>
#include "multiplayer_game.h"
#include "utils.h"

int main() {
    Utils::printMsg("Tank Game Client Starting...");

    // Get player information
    std::string playerName;
    std::string serverIP = "127.0.0.1";
    unsigned short serverPort = 53000;
    std::string preferredColor = "green";

    std::cout << "Enter your player name: ";
    std::getline(std::cin, playerName);
    if (playerName.empty()) {
        playerName = "Player";
    }

    std::cout << "Enter server IP (default 127.0.0.1): ";
    std::string input;
    std::getline(std::cin, input);
    if (!input.empty()) {
        serverIP = input;
    }

    std::cout << "Enter server port (default 53000): ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        try {
            serverPort = static_cast<unsigned short>(std::stoi(input));
        }
        catch (const std::exception&) {
            Utils::printMsg("Invalid port, using default 53000", MessageType::warning);
            serverPort = 53000;
        }
    }

    std::cout << "Enter preferred color (red/blue/green/black, default green): ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        preferredColor = input;
    }

    // Create game window
    sf::RenderWindow window(sf::VideoMode({ 640, 480 }),
        "Tank Game - Multiplayer Client (" + playerName + ")");
    window.setFramerateLimit(60);

    // Initialize game
    MultiplayerGame game;
    if (!game.Initialize(playerName, preferredColor)) {
        Utils::printMsg("Failed to initialize game", MessageType::error);
        return -1;
    }

    // Connect to server
    Utils::printMsg("Connecting to server " + serverIP + ":" + std::to_string(serverPort) + "...");
    if (!game.ConnectToServer(serverIP, serverPort)) {
        Utils::printMsg("Failed to connect to server", MessageType::error);
        Utils::printMsg("Make sure the server is running and accessible");
        return -1;
    }

    Utils::printMsg("Connected to server successfully!", MessageType::success);
    Utils::printMsg("Use WASD to move your tank. Press ESC to quit.");

    // Game loop
    sf::Clock clock;

    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();

        // Handle window events
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                Utils::printMsg("Window closed", MessageType::warning);
                window.close();
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
                    Utils::printMsg("ESC pressed, closing game", MessageType::warning);
                    window.close();
                }
            }

            // Pass input events to game
            game.HandleEvents(event);
        }

        // Update game
        game.Update(deltaTime);

        // Check connection status
        if (!game.IsConnected()) {
            Utils::printMsg("Lost connection to server", MessageType::error);
            window.close();
        }

        // Render game
        window.clear();
        game.Render(window);
        window.display();

        // Update window title with player count
        static float titleUpdateTimer = 0;
        titleUpdateTimer += deltaTime;
        if (titleUpdateTimer >= 1.0f) { // Update every second
            size_t playerCount = game.GetPlayerCount();
            window.setTitle("Tank Game - Multiplayer Client (" + playerName + ") - " +
                std::to_string(playerCount) + " players");
            titleUpdateTimer = 0;
        }
    }

    Utils::printMsg("Game shutting down...", MessageType::warning);
    game.Shutdown();
    Utils::printMsg("Game closed", MessageType::success);

    return 0;
}