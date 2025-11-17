#include "game.h"

Game::Game()
{
	// Initialise the background texture and sprite.
	// FIXME: loadFromFile returns a bool if texture was loaded successfully. We should use it to check for errors.
	backgroundTexture.loadFromFile("Assets/tileLightSand.png");
	backgroundTexture.setRepeated(true);

	// Replace placeholder texture with a proper background texture, now that we have it.
	background.setTexture(backgroundTexture);
	background.setTextureRect(sf::IntRect({ 0, 0 }, { 640, 480 }));

	// Set default tank position to be the centre of the window.
	tank.position = { 320, 240 };
}

void Game::HandleEvents(const std::optional<sf::Event> event)
{
	// Handle key press events passed from window.
	if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
		if (keyPressed->scancode == sf::Keyboard::Scancode::W) {
			tank.isMoving.forward = true;
			tank.isMoving.backward = false;
		}
		else if (keyPressed->scancode == sf::Keyboard::Scancode::S) {
			tank.isMoving.forward = false;
			tank.isMoving.backward = true;
		}
		if (keyPressed->scancode == sf::Keyboard::Scancode::A) {
			tank.isMoving.left = true;
			tank.isMoving.right = false;
		}
		else if (keyPressed->scancode == sf::Keyboard::Scancode::D) {
			tank.isMoving.left = false;
			tank.isMoving.right = true;
		}
		if (keyPressed->scancode == sf::Keyboard::Scancode::D) {

		}
	}

	// Handle key release events passed from window.
	else if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>()) {
		if (keyReleased->scancode == sf::Keyboard::Scancode::W)
			tank.isMoving.forward = false;
		if (keyReleased->scancode == sf::Keyboard::Scancode::S)
			tank.isMoving.backward = false;
		if (keyReleased->scancode == sf::Keyboard::Scancode::A)
			tank.isMoving.left = false;
		if (keyReleased->scancode == sf::Keyboard::Scancode::D)
			tank.isMoving.right = false;
	}
}

void Game::Update(float dt)
{
	tank.Update(dt);
}

void Game::NetworkUpdate(float dt, TankMessage data) {
	// Force position update from network data.
	tank.position = { data.x, data.y };
	// Update tank with new position.
	// NOTE: This assumets no inputs were detected and so the tank will only move according to 
	// network updates. This is not ideal and prone to unexpected behaviour if game is extended
	// to be fully multiplayer. 
	tank.Update(dt);
}

void Game::Render(sf::RenderWindow& window)
{
	window.draw(background);
	tank.Render(window);
}

TankMessage Game::GetNetworkUpdate()
{
	return { tank.position.x, tank.position.y };
}