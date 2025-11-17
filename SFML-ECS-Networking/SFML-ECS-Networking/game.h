#pragma once
#include "tank.h"

class Game
{
public:
	Game();

	void HandleEvents(const std::optional<sf::Event> event);
	void Update(float dt);
	void NetworkUpdate(float dt, TankMessage data);
	void Render(sf::RenderWindow &window);
	TankMessage GetNetworkUpdate();

private:
	Tank tank = Tank("green"); // player object

	// Temporary placeholder texture, make sue to replace before rendering the sprite.
	sf::Texture placeholder = sf::Texture(sf::Vector2u(1, 1));
	sf::Texture backgroundTexture;

	// This can (and probably should) be replaced with std::optional or a unique pointer, 
	// to remove the need to use placeholder textures for sprite initialisation.
	sf::Sprite background = sf::Sprite(placeholder);
};

