#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <string>
#include <thread>
#include "game_server.h"
#include "multiplayer_game.h"
#include "utils.h"
#include <limits>
#include <regex>

/**
 * Validates port in 1024-65535 range to ensure safe, non-privileged use per IANA standards.
 * @param port Unsigned short for 16-bit port value as in TCP/IP.
 * @return Bool indicating validity for easy conditional checks.
 */
bool IsValidPort(unsigned short port) {
    return port >= 1024 && port <= 65535;
}

/**
 * Validates IPv4 address format using regex or "localhost" for network input sanitization.
 * @param ip Const string ref for efficient IP string validation without copies.
 * @return Bool for quick error handling in connection setup.
 */
bool IsValidIPAddress(const std::string& ip) {
    std::regex ipPattern("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    return std::regex_match(ip, ipPattern) || ip == "localhost";
}

/**
 * Ensures player name is 1-50 chars of printable ASCII (32-126) to prevent display/network issues.
 * @param name Const string ref for efficient validation of user input.
 * @return Bool to confirm safe usage in game.
 */
bool IsValidPlayerName(const std::string& name) {
    if (name.empty() || name.length() > 50) return false;
    for (char c : name) if (c < 32 || c > 126) return false;
    return true;
}

/**
 * Checks if color is in allowed list {"red", "blue", "green", "black"} for game consistency.
 * @param color Const string ref for efficient color string check.
 * @return Bool for validation in player setup.
 */
bool IsValidColor(const std::string& color) {
    const std::vector<std::string> validColors = { "red", "blue", "green", "black" };
    return std::find(validColors.begin(), validColors.end(), color) != validColors.end();
}

/**
 * Runs server: prompts port, initializes, updates in loop until stopped, handles errors.
 * @return Int: 0 success, -1 failure for main program exit codes.
 */
int runServer() {
    Utils::printMsg("Starting Tank Game Server...");
    unsigned short port = 53000;
    std::cout << "Enter server port (default 53000): ";
    std::string input;
    std::getline(std::cin, input);
    if (!input.empty()) {
        try {
            int tempPort = std::stoi(input);
            if (tempPort < 0 || tempPort > 65535) {
                Utils::printMsg("Error: Port out of range (0-65535), using default 53000", error);
                port = 53000;
            }
            else if (!IsValidPort(static_cast<unsigned short>(tempPort))) {
                Utils::printMsg("Error: Port must be between 1024 and 65535, using default 53000", error);
                port = 53000;
            }
            else {
                port = static_cast<unsigned short>(tempPort);
                Utils::printMsg("Using port: " + std::to_string(port));
            }
        }
        catch (const std::exception& e) {
            Utils::printMsg("Error: Invalid port input (" + input + "), using default 53000 - " + std::string(e.what()), error);
            port = 53000;
        }
    }
    else {
        Utils::printMsg("Using default port: 53000");
    }
    GameServer server(port);
    if (!server.Initialize()) {
        Utils::printMsg("Failed to initialize server", error);
        return -1;
    }
    Utils::printMsg("Server running. Press Enter to stop server...");
    auto localIP = sf::IpAddress::getLocalAddress();
    if (localIP) {
        Utils::printMsg("Players can connect to: " + localIP.value().toString() + ":" + std::to_string(port));
    }
    else {
        Utils::printMsg("Players can connect to: localhost:" + std::to_string(port));
    }
    sf::Clock clock;
    bool running = true;
    std::thread inputThread([&running]() {
        std::string input;
        std::getline(std::cin, input);
        running = false;
        });
    while (running && server.IsRunning()) {
        float deltaTime = clock.restart().asSeconds();
        if (deltaTime < 0 || !std::isfinite(deltaTime)) {
            Utils::printMsg("Warning: Invalid delta time, skipping update", warning);
            continue;
        }
        server.Update(deltaTime);
        sf::sleep(sf::milliseconds(1));
    }
    Utils::printMsg("Shutting down server...", warning);
    server.Shutdown();
    if (inputThread.joinable()) {
        try {
            inputThread.join();
        }
        catch (const std::exception& e) {
            Utils::printMsg("Error: Exception joining input thread - " + std::string(e.what()), error);
        }
    }
    Utils::printMsg("Server stopped", success);
    return 0;
}

/**
 * Runs client: prompts name/IP/port/color, connects, runs game loop with events/update/render.
 * @return Int: 0 success, -1 failure for program control.
 */
int runClient() {
    Utils::printMsg("Tank Game Client Starting...");
    std::string playerName;
    std::string serverIP = "127.0.0.1";
    unsigned short serverPort = 53000;
    std::string preferredColor = "green";
    std::cout << "Enter your player name: ";
    std::getline(std::cin, playerName);
    if (!IsValidPlayerName(playerName)) {
        Utils::printMsg("Warning: Invalid player name, using default 'Player'", warning);
        playerName = "Player";
    }
    else {
        Utils::printMsg("Using player name: " + playerName);
    }
    std::cout << "Enter server IP (default 127.0.0.1): ";
    std::string input;
    std::getline(std::cin, input);
    if (!input.empty()) {
        if (!IsValidIPAddress(input)) {
            Utils::printMsg("Error: Invalid IP address (" + input + "), using default 127.0.0.1", error);
            serverIP = "127.0.0.1";
        }
        else {
            serverIP = input;
            Utils::printMsg("Using server IP: " + serverIP);
        }
    }
    else {
        Utils::printMsg("Using default server IP: 127.0.0.1");
    }
    std::cout << "Enter server port (default 53000): ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        try {
            int tempPort = std::stoi(input);
            if (tempPort < 0 || tempPort > 65535) {
                Utils::printMsg("Error: Port out of range (0-65535), using default 53000", error);
                serverPort = 53000;
            }
            else if (!IsValidPort(static_cast<unsigned short>(tempPort))) {
                Utils::printMsg("Error: Port must be between 1024 and 65535, using default 53000", error);
                serverPort = 53000;
            }
            else {
                serverPort = static_cast<unsigned short>(tempPort);
                Utils::printMsg("Using server port: " + std::to_string(serverPort));
            }
        }
        catch (const std::exception& e) {
            Utils::printMsg("Error: Invalid port input (" + input + "), using default 53000 - " + std::string(e.what()), error);
            serverPort = 53000;
        }
    }
    else {
        Utils::printMsg("Using default server port: 53000");
    }
    std::cout << "Enter preferred color (red/blue/green/black, default green): ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        if (!IsValidColor(input)) {
            Utils::printMsg("Error: Invalid color (" + input + "), using default green", error);
            preferredColor = "green";
        }
        else {
            preferredColor = input;
            Utils::printMsg("Using preferred color: " + preferredColor);
        }
    }
    else {
        Utils::printMsg("Using default color: green");
    }
    sf::RenderWindow window;
    try {
        window.create(sf::VideoMode({ 1280, 960 }), "Tank Game - Multiplayer Client (" + playerName + ")");
        window.setFramerateLimit(60);
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Failed to create window - " + std::string(e.what()), error);
        return -1;
    }
    MultiplayerGame game;
    if (!game.Initialize(playerName, preferredColor)) {
        Utils::printMsg("Failed to initialize game", error);
        return -1;
    }
    game.SetWindow(&window);
    Utils::printMsg("Connecting to server " + serverIP + ":" + std::to_string(serverPort) + "...");
    if (!game.ConnectToServer(serverIP, serverPort)) {
        Utils::printMsg("Failed to connect to server", error);
        Utils::printMsg("Make sure the server is running and accessible");
        return -1;
    }
    Utils::printMsg("Connected to server successfully!", success);
    Utils::printMsg("Use WASD to move your tank, mouse to aim barrel. Press ESC to quit.");
    sf::Clock clock;
    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();
        if (deltaTime < 0 || !std::isfinite(deltaTime)) {
            Utils::printMsg("Warning: Invalid delta time, skipping update", warning);
            continue;
        }
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                Utils::printMsg("Window closed", warning);
                window.close();
            }
            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
                    Utils::printMsg("ESC pressed, closing game", warning);
                    window.close();
                }
            }
            try {
                game.HandleEvents(event);
            }
            catch (const std::exception& e) {
                Utils::printMsg("Error: Exception handling events - " + std::string(e.what()), error);
            }
        }
        try {
            game.Update(deltaTime);
        }
        catch (const std::exception& e) {
            Utils::printMsg("Error: Exception in game update - " + std::string(e.what()), error);
        }
        if (!game.IsConnected()) {
            Utils::printMsg("Lost connection to server", error);
            window.close();
        }
        try {
            window.clear();
            game.Render(window);
            window.display();
        }
        catch (const std::exception& e) {
            Utils::printMsg("Error: Exception during rendering - " + std::string(e.what()), error);
            window.close();
        }
        static float titleUpdateTimer = 0;
        titleUpdateTimer += deltaTime;
        if (titleUpdateTimer >= 1.0f) {
            try {
                size_t playerCount = game.GetPlayerCount();
                window.setTitle("Tank Game - Multiplayer Client (" + playerName + ") - " + std::to_string(playerCount) + " players");
                titleUpdateTimer = 0;
            }
            catch (const std::exception& e) {
                Utils::printMsg("Error: Exception updating window title - " + std::string(e.what()), error);
            }
        }
    }
    Utils::printMsg("Game shutting down...", warning);
    try {
        game.Shutdown();
    }
    catch (const std::exception& e) {
        Utils::printMsg("Error: Exception during game shutdown - " + std::string(e.what()), error);
    }
    Utils::printMsg("Game closed", success);
    return 0;
}

/**
 * Main: prompts mode choice (1 server, 2 client), runs corresponding function.
 * @return Int: 0 success, -1 invalid choice for error reporting.
 */
int main() {
    Utils::printMsg("Tank Game - Multiplayer");
    std::cout << "Choose mode:\n";
    std::cout << "1. Start Server\n";
    std::cout << "2. Join as Player\n";
    std::cout << "Enter choice (1 or 2): ";
    std::string choice;
    std::getline(std::cin, choice);
    if (choice == "1") {
        Utils::printMsg("Starting server mode...");
        return runServer();
    }
    else if (choice == "2") {
        Utils::printMsg("Starting client mode...");
        return runClient();
    }
    else {
        Utils::printMsg("Error: Invalid choice (" + choice + "). Must be '1' or '2'", error);
        return -1;
    }
}