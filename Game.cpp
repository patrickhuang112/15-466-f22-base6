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

	if (c2s_comp.empty()) {
		connection.send(static_cast<uint8_t>(0));	
	}
	else {
		std::string s = c2s_comp.front();	
		c2s_comp.pop_front();
		uint8_t complen = static_cast<uint8_t>(s.length());
		connection.send(complen);
		for (char c : s) {
			connection.send(static_cast<uint8_t>(c));	
		}
		
	}
	if (c2s_foropp.empty()) {
		connection.send(static_cast<uint8_t>(0));	
	}
	else {
		std::string s = c2s_foropp.front();	
		c2s_foropp.pop_front();
		uint8_t foropplen = static_cast<uint8_t>(s.length());
		connection.send(foropplen);
		for (char c : s) {
			connection.send(static_cast<uint8_t>(c));	
		}
	}
}

bool Player::recv_controls_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;

	auto &recv_buffer = connection.recv_buffer;
	bool something = false;	

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
	if (complen > 0) {
		for (size_t i = 0; i < complen; ++i) {
			chars[i] = static_cast<char>(recv_buffer[message_size + i]);
		}
		chars[complen] = '\0';
		message_size += complen;
		std::string comps = std::string(chars);
		something = true;
		c2s_comp.emplace_back(comps);
	}	

	size_t foropplen = recv_buffer[message_size];
	message_size++;
	if (foropplen > 0) {
		for (size_t i = 0; i < foropplen; ++i) {
			chars[i] = static_cast<char>(recv_buffer[message_size + i]);
		}
		chars[foropplen] = '\0';
		message_size += foropplen;
		std::string foropp = std::string(chars);
		something = true;
		c2s_foropp.emplace_back(foropp);
	}	

	self_score = scoreu.f;
	
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + message_size);

	return !something;
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
	p2->opp_score = p1->self_score;	
	p1->opp_score = p2->self_score;

	while (!p1->c2s_comp.empty()) {
		std::string s = p1->c2s_comp.front();
		if (p2->words.find(s) == p2->words.end()) {
			assert(p1->words.find(s) != p1->words.end());
			p1->words.erase(s);	
		}
		else {
			assert(p2->words.find(s) != p2->words.end());
			p2->words.erase(s);
		}	
		p2->s2c_delete.emplace_back(s);
		p1->c2s_comp.pop_front();
	}
	while (!p1->c2s_foropp.empty()) {
		std::string s = p1->c2s_foropp.front();
		p2->s2c_opp.emplace_back(s);	
		p1->c2s_foropp.pop_front();
	}

	while (!p2->c2s_comp.empty()) {
		std::string s = p2->c2s_comp.front();
		if (p1->words.find(s) == p1->words.end()) {
			assert(p2->words.find(s) != p2->words.end());
			p2->words.erase(s);	
		}
		else {
			assert(p1->words.find(s) != p1->words.end());
			p1->words.erase(s);
		}	
		p1->s2c_delete.emplace_back(s);
		p2->c2s_comp.pop_front();
	}
	while (!p2->c2s_foropp.empty()) {
		std::string s = p2->c2s_foropp.front();
		p1->s2c_opp.emplace_back(s);	
		p2->c2s_foropp.pop_front();
	}


	if (static_cast<uint32_t>(total_elapsed) % Gamel::NEW_WORD_CYCLE) {
		if (can_make_word) {
			const std::string& s1 = g.create_new_word();
			const std::string& s2 = g.create_new_word();
			p1->words.emplace(s1);
			p2->words.emplace(s2);
			p1->s2c_self.emplace_back(s1);
			p2->s2c_self.emplace_back(s2);
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
	Player *p = connection_player;
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


	if (p->s2c_self.empty()) {
		connection.send(static_cast<uint8_t>(0));	
	}
	else {
		std::string s = p->s2c_self.front();	
		printf("Send state1: %s\n", s.c_str());
		uint8_t len = static_cast<uint8_t>(s.length());
		connection.send(len);
		for (char c : s) {
			connection.send(static_cast<uint8_t>(c));	
		}
		p->s2c_self.pop_front();
	}


	if (p->s2c_opp.empty()) {
		connection.send(static_cast<uint8_t>(0));	
	}
	else {
		std::string s = p->s2c_opp.front();	
		printf("Send state2: %s\n", s.c_str());
		uint8_t len = static_cast<uint8_t>(s.length());
		connection.send(len);
		for (char c : s) {
			connection.send(static_cast<uint8_t>(c));	
		}
		p->s2c_opp.pop_front();
	}


	if (p->s2c_delete.empty()) {
		connection.send(static_cast<uint8_t>(0));	
	}
	else {
		std::string s = p->s2c_delete.front();	
		printf("Send state3: %s\n", s.c_str());
		uint8_t len = static_cast<uint8_t>(s.length());
		connection.send(len);
		for (char c : s) {
			connection.send(static_cast<uint8_t>(c));	
		}
		p->s2c_delete.pop_front();
	}
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
	bool something = false;	
	char chars[BIG_ENOUGH_ARRAY];
	size_t message_size = 5;
	size_t newlen = recv_buffer[message_size];
	message_size++;
	if (newlen > 0) {
		for (size_t i = 0; i < newlen; ++i) {
			chars[i] = static_cast<char>(recv_buffer[message_size + i]);
		}
		chars[newlen] = '\0';
		message_size += newlen;
		std::string news = std::string(chars);
		p->s2c_self.emplace_back(news);
		something = true;
	}	

	size_t opplen = recv_buffer[message_size];
	message_size++;
	if (opplen > 0) {
		for (size_t i = 0; i < opplen; ++i) {
			chars[i] = static_cast<char>(recv_buffer[message_size + i]);
		}
		chars[opplen] = '\0';
		message_size += opplen;
		std::string opps = std::string(chars);
		something = true;
		p->s2c_opp.emplace_back(opps);
		printf("HERE2\n");
	}

	size_t deletelen = recv_buffer[message_size];
	message_size++;
	if (deletelen > 0) {
		for (size_t i = 0; i < deletelen; ++i) {
			chars[i] = static_cast<char>(recv_buffer[message_size + i]);
		}
		chars[deletelen] = '\0';
		message_size += deletelen;
		std::string deletes = std::string(chars);
		something = true;
		p->s2c_delete.emplace_back(deletes);
		printf("HERE3\n");
	}	

	p->opp_score = scoreu.f;

	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + message_size);

	return !something;
}
