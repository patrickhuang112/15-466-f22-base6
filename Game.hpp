#pragma once



#include <glm/glm.hpp>

#include "Gamel.hpp"

#include <string>
#include <list>
#include <unordered_set>
#include <random>

struct Connection;

//Game state, separate from rendering.

//Currently set up for a "client sends controls" / "server sends whole state" situation.

enum class Message : uint8_t {
	C2S_Controls = 1, //Greg!
	S2C_State = 's',
	S2C_Over = 2,
	//...
};

//used to represent a control input:
struct Button {
	uint8_t downs = 0; //times the button has been pressed
	bool pressed = false; //is the button pressed now
};

//state of one player in the game:
struct Player {
	Player() {
		c2s_comp.clear();
		c2s_foropp.clear();
		s2c_opp.clear();
		s2c_self.clear();
		s2c_delete.clear();

	}
	//player inputs (sent from client):
	void send_controls_message(Connection *connection);

	//returns 'false' if no message or not a controls message,
	//returns 'true' if read a controls message,
	//throws on malformed controls message
	bool recv_controls_message(Connection *connection);
	std::unordered_set<std::string> words;
	Gamel::Playerl p;
	std::string name = "";

	bool game_over = false;	

	// Connection related info
	std::list<std::string> s2c_opp;
	std::list<std::string> s2c_self;
	std::list<std::string> s2c_delete;

	std::list<std::string> c2s_comp;
	std::list<std::string> c2s_foropp;

	float self_score = 0.f;
	float opp_score = 0.f;
};

struct Game {

	// Game logic here
	Gamel::Gamel g;

	Player *p1 = nullptr;
	Player *p2 = nullptr;

	float total_elapsed = 0.f;
	bool can_make_word = true;
	bool game_over = false;

	std::list< Player > players; //(using list so they can have stable addresses)
	Player *spawn_player(); //add player the end of the players list (may also, e.g., play some spawn anim)
	void remove_player(Player *); //remove player from game (may also, e.g., play some despawn anim)

	std::mt19937 mt; //used for spawning players
	uint32_t next_player_number = 1; //used for naming players

	Game();

	//state update function:
	void update(float elapsed);

	//constants:
	//the update rate on the server:
	inline static constexpr float Tick = 1.0f / 30.0f;

	//arena size:
	inline static constexpr glm::vec2 ArenaMin = glm::vec2(-0.75f, -1.0f);
	inline static constexpr glm::vec2 ArenaMax = glm::vec2( 0.75f,  1.0f);

	//player constants:
	inline static constexpr float PlayerRadius = 0.06f;
	inline static constexpr float PlayerSpeed = 2.0f;
	inline static constexpr float PlayerAccelHalflife = 0.25f;
	

	//---- communication helpers ----

	//used by client:
	//set game state from data in connection buffer
	// (return true if data was read)
	bool recv_state_message(Connection *connection, Player *connection_player);

	//used by server:
	//send game state.
	//  Will move "connection_player" to the front of the front of the sent list.
	void send_state_message(Connection *connection, Player *connection_player = nullptr) const;
};
