CMP425 / CMP501 
Lab 3 Application-Layer Protocol Example - by Andrei Boiko
Built using SFML Sockets

### The "Game"
This is a very simple prototype for a tank game. At the moment it draws a single tank consisting
of two sprites, one for the body and one for the gun barrel. The tank can be rotated using A and D keys
and moved forward and backward using W and S respectively.
The game can be run in two modes: Player and Observer.
- Player can move the tank around using keyboard controls.
- Observer can recieve UDP packets from Player and update the game from those packets.

### The protocol
At the moment we are only sending the position of the tank to the observer, rotation, tank colour, etc. are not transmitted.
The send rate of the packets can be adjusted using the send_rate variable in main.cpp
"# Tank" 
