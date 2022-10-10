#include "Game.hpp"

#include "Connection.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>

#include <glm/gtx/norm.hpp>

#define BIG_ENOUGH_ARRAY 50

union float_to_int{
	float f;
	uint32_t i;	
};

void Player::send_controls_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;

	/* [type, score, and then
	 	completed size/string 
		available to opp size/string
		]
	*/
	connection.send(Message::C2S_Controls);

	float_to_int scoreu;	
	scoreu.f = self_score;

	uint8_t l0 = static_cast<uint8_t>(scoreu.i);
	uint8_t l8 = static_cast<uint8_t>(scoreu.i >> 8);
	uint8_t l16 = static_cast<uint8_t>(scoreu.i >> 16);
	uint8_t l24 = static_cast<uint8_t>(scoreu.i >> 24);
	connection.send(l24);
	connection.send(l16);
	connection.send(l8);
	connection.send(l0);

	uint8_t complen = static_cast<uint8_t>(c2s_comp.length());
	connection.send(complen);
	for (char c : c2s_comp) {
		connection.send(static_cast<uint8_t>(c));	
	}

	uint8_t foropplen = static_cast<uint8_t>(c2s_foropp.length());
	connection.send(foropplen);
	for (char c : c2s_foropp) {
		connection.send(static_cast<uint8_t>(c));	
	}

	c2s_comp = "";
	c2s_foropp = "";
}

bool Player::recv_controls_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;

	auto &recv_buffer = connection.recv_buffer;
	

	float_to_int scoreu;	
	if (recv_buffer.size() < 6) {
		return false;	
	}
	if (recv_buffer[0] != uint8_t(Message::C2S_Controls)) return false;
	uint32_t size = (uint32_t(recv_buffer[1]) << 24)
				  |	(uint32_t(recv_buffer[2]) << 16)
	              | (uint32_t(recv_buffer[3]) << 8)
	              |  uint32_t(recv_buffer[4]);
	scoreu.i = size;
	
	char chars[BIG_ENOUGH_ARRAY];
	size_t message_size = 5;
	size_t complen = recv_buffer[message_size];
	message_size++;
	std::string comps = "";
	if (complen > 0) {
		for (size_t i = 0; i < complen; ++i) {
			chars[i] = static_cast<char>(recv_buffer[message_size + i]);
		}
		chars[complen] = '\0';
		message_size += complen;
		comps = std::string(chars);
	}	

	size_t foropplen = recv_buffer[message_size];
	message_size++;
	std::string foropp = "";
	if (foropplen > 0) {
		for (size_t i = 0; i < foropplen; ++i) {
			chars[i] = static_cast<char>(recv_buffer[message_size + i]);
		}
		chars[foropplen] = '\0';
		message_size += foropplen;
		foropp = std::string(chars);
	}	

	self_score = scoreu.f;
	c2s_comp = comps;
	c2s_foropp = foropp;
	c2s_received = true;
	
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + message_size);

	return true;
}


//-----------------------------------------

Game::Game() : mt(0x15466666) {
}

Player *Game::spawn_player() {
	players.emplace_back();
	Player &player = players.back();

	return &player;
}

void Game::remove_player(Player *player) {
	bool found = false;
	for (auto pi = players.begin(); pi != players.end(); ++pi) {
		if (&*pi == player) {
			players.erase(pi);
			found = true;
			if (player == p1) {
				p1 = nullptr;	
			}
			else if (player == p2) {
				p2 = nullptr;
			}
			break;
		}
	}
	assert(found);
}

void Game::update(float elapsed) {
	// Dont do anything until we have two players
	if (p1 == nullptr || p2 == nullptr) {
		return;
	}
	if (p1->c2s_received) {
		p2->opp_score = p1->self_score;	
		if (p1->c2s_comp != "") {
			if (p2->words.find(p1->c2s_comp) == p2->words.end()) {
				assert(p1->words.find(p1->c2s_comp) != p1->words.end());
				p1->words.erase(p1->c2s_comp);	
			}
			else {
				p2->words.erase(p2->c2s_comp);
			}
			// Either way signal that the word is done and to be deleted
			p2->s2c_delete = p1->c2s_comp;
		}

		if (p1->c2s_foropp != "") {
			p2->s2c_opp = p1->c2s_foropp;
		}

		p1->c2s_comp = "";
		p1->c2s_foropp = "";
		p1->c2s_received = false;
	}	
	if (p2->c2s_received) {
		p1->opp_score = p2->self_score;	
		if (p2->c2s_comp != "") {
			if (p1->words.find(p2->c2s_comp) == p1->words.end()) {
				assert(p2->words.find(p2->c2s_comp) != p2->words.end());
				p2->words.erase(p2->c2s_comp);	
			}
			else {
				p1->words.erase(p1->c2s_comp);
			}
			// Either way signal that the word is done and to be deleted
			p1->s2c_delete = p2->c2s_comp;
		}

		if (p2->c2s_foropp != "") {
			p1->s2c_opp = p2->c2s_foropp;
		}

		p2->c2s_foropp = "";
		p2->c2s_comp = "";
		p2->c2s_received = false;
	}	

	if (static_cast<uint32_t>(total_elapsed) % Gamel::NEW_WORD_CYCLE) {
		if (can_make_word) {
			const std::string& s1 = g.create_new_word();
			const std::string& s2 = g.create_new_word();
			p1->words.emplace(s1);
			p2->words.emplace(s2);
			p1->s2c_self = s1;
			p2->s2c_self = s2;
			can_make_word = false;
		}
	}
	else {
		can_make_word = true;	
	}

	total_elapsed += elapsed;
}


void Game::send_state_message(Connection *connection_, Player *connection_player) const {
	assert(connection_);
	auto &connection = *connection_;

	/* [opponent score, 
	 	new string size/string 
		new opponent string to opp size/string
		delete existing string
		]
	*/

	connection.send(Message::S2C_State);

	float_to_int scoreu;	
	scoreu.f = connection_player->opp_score;

	uint8_t l0 = static_cast<uint8_t>(scoreu.i);
	uint8_t l8 = static_cast<uint8_t>(scoreu.i >> 8);
	uint8_t l16 = static_cast<uint8_t>(scoreu.i >> 16);
	uint8_t l24 = static_cast<uint8_t>(scoreu.i >> 24);
	connection.send(l24);
	connection.send(l16);
	connection.send(l8);
	connection.send(l0);

	uint8_t newlen = static_cast<uint8_t>(connection_player->s2c_self.length());
	connection.send(newlen);
	for (char c : connection_player->s2c_self) {
		connection.send(static_cast<uint8_t>(c));	
	}

	uint8_t opplen = static_cast<uint8_t>(connection_player->s2c_opp.length());
	connection.send(opplen);
	for (char c : connection_player->s2c_opp) {
		connection.send(static_cast<uint8_t>(c));	
	}

	uint8_t deletelen = static_cast<uint8_t>(connection_player->s2c_delete.length());
	connection.send(deletelen);
	for (char c : connection_player->s2c_delete) {
		connection.send(static_cast<uint8_t>(c));	
	}


	connection_player->s2c_self = "";
	connection_player->s2c_opp = "";
	connection_player->s2c_delete = "";

}

bool Game::recv_state_message(Connection *connection_, Player *p) {
	assert(connection_);
	auto &connection = *connection_;
	auto &recv_buffer = connection.recv_buffer;


	float_to_int scoreu;	
	if (recv_buffer.size() < 8) {
		return false;	
	}
	if (recv_buffer[0] != uint8_t(Message::S2C_State)) return false;
	uint32_t size = (uint32_t(recv_buffer[1]) << 24)
				  |	(uint32_t(recv_buffer[2]) << 16)
	              | (uint32_t(recv_buffer[3]) << 8)
	              |  uint32_t(recv_buffer[4]);
	scoreu.i = size;
	
	char chars[BIG_ENOUGH_ARRAY];
	size_t message_size = 5;
	size_t newlen = recv_buffer[message_size];
	message_size++;
	std::string news = "";
	if (newlen > 0) {
		for (size_t i = 0; i < newlen; ++i) {
			chars[i] = static_cast<char>(recv_buffer[message_size + i]);
		}
		chars[newlen] = '\0';
		message_size += newlen;
		news = std::string(chars);
	}	

	size_t opplen = recv_buffer[message_size];
	message_size++;
	std::string opps = "";	
	if (opplen > 0) {
		for (size_t i = 0; i < opplen; ++i) {
			chars[i] = static_cast<char>(recv_buffer[message_size + i]);
		}
		chars[opplen] = '\0';
		message_size += opplen;
		opps = std::string(chars);
	}

	size_t deletelen = recv_buffer[message_size];
	message_size++;
	std::string deletes = "";	
	if (deletelen > 0) {
		for (size_t i = 0; i < deletelen; ++i) {
			chars[i] = static_cast<char>(recv_buffer[message_size + i]);
		}
		chars[deletelen] = '\0';
		message_size += deletelen;
		deletes = std::string(chars);
	}	

	p->opp_score = scoreu.f;
	p->s2c_opp = opps;
	p->s2c_self = news;
	p->s2c_delete = deletes;
	p->s2c_received = true;

	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + message_size);

	return true;
}
