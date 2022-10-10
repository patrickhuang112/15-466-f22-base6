#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <random>
#include <algorithm>
#include <array>
#include <string>


PlayMode::PlayMode(Client &client_) : client(client_) {
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_KEYUP) {
		switch(evt.key.keysym.sym) {
			case SDLK_a:
			case SDLK_b:
			case SDLK_c:
			case SDLK_d:
			case SDLK_e:
			case SDLK_f:
			case SDLK_g:
			case SDLK_h:
			case SDLK_i:
			case SDLK_j:
			case SDLK_k:
			case SDLK_l:
			case SDLK_m:
			case SDLK_n:
			case SDLK_o:
			case SDLK_p:
			case SDLK_q:
			case SDLK_r:
			case SDLK_s:
			case SDLK_t:
			case SDLK_u:
			case SDLK_v:
			case SDLK_w:
			case SDLK_x:
			case SDLK_y:
			case SDLK_z:
			case SDLK_1:
			case SDLK_2:
			case SDLK_3:
			case SDLK_4:
			case SDLK_5:
			case SDLK_6:
			case SDLK_7:
			case SDLK_8:
			case SDLK_9:
			case SDLK_0:
				player.process_letter(static_cast<char>(evt.key.keysym.sym));
				return true;	
		}
	}
	return false;
}

void PlayMode::update(float elapsed) {
	player.move_words(elapsed);

	//queue data for sending to server:
	playerconn.self_score = player.score;	

	if (!player.words_to_send_self.empty()) {
		playerconn.c2s_comp.emplace_back(player.words_to_send_self.front());
		player.words_to_send_self.pop_front();
	}	

	if (!player.words_to_send_opp.empty()) {
		playerconn.c2s_foropp.emplace_back(player.words_to_send_opp.front());
		player.words_to_send_opp.pop_front();
	}


	playerconn.send_controls_message(&client.connection);

	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush(); //DEBUG
			bool handled_message;
			try {
				do {
					handled_message = false;
					if (game.recv_state_message(c, &playerconn)) handled_message = true;
				} while (handled_message);
			} catch (std::exception const &e) {
				std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl;
				//quit the game:
				throw e;
			}
		}
	}, 0.0);

	player.oppscore = playerconn.opp_score;
	while (!playerconn.s2c_delete.empty()) {
		std::string s = playerconn.s2c_delete.front();
		playerconn.s2c_delete.pop_front();
		if (player.has_word(s)) {
			player.remove_word(s);	
		}
	}
	while (!playerconn.s2c_opp.empty()) {
		std::string s = playerconn.s2c_opp.front();
		playerconn.s2c_opp.pop_front();
		player.add_word(s, true);
	}
	while (!playerconn.s2c_self.empty()) {
		std::string s = playerconn.s2c_self.front();	
		playerconn.s2c_self.pop_front();
		player.add_word(s, false);
	}
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	glDisable(GL_DEPTH_TEST);
	// Clear previously drawn text
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	float aspect = float(drawable_size.x) / float(drawable_size.y);
	DrawLines lines(glm::mat4(
		1.0f / aspect, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	));

	for (auto w : player.words) {
		const std::string& s = w->s; 
		// Skip the first visited node which is always the root
		bool matching = true;
		size_t matched = 0;
		for (size_t i = 1; i < player.visited.size(); ++i) {
			auto trienode = player.visited[i];
			if (i-1 >= s.length() || trienode->c != s[i-1]) {
				matching = false;	
				break;
			}
			++matched;
		}
		glm::vec4 word_color = w->lifetime == Gamel::DEFAULT_WORD_LIFETIME ? Gamel::DEFAULT_COLOR : Gamel::OPPONENT_COLOR;
		for (size_t i = 0; i < s.length(); ++i) {
			float x = w->pos.x + i * Gamel::LETTER_WIDTH;
			glm::uvec4 color;
			if (matching && i < matched) {
				color = Gamel::CORRECT_COLOR;
			}	
			else {
				color = word_color;
			}
			// Assumming only alpha characters
			std::string text(1, s[i] - Gamel::LOWER_TO_UPPER_SHIFT); 
			lines.draw_text(text,
			glm::vec3(x, w->pos.y, 0.0),
			glm::vec3(Gamel::TEXT_SIZE, 0.0f, 0.0f), glm::vec3(0.0f, Gamel::TEXT_SIZE, 0.0f),
			color);
		}
	}

	// Draw Score at bottom
	constexpr float H = Gamel::HUD_SIZE;
	float ofs = 2.0f / drawable_size.y;
	std::string text = "Your score: " + std::to_string(player.score)  + "  Opponent's score: " + std::to_string(player.oppscore);
	lines.draw_text(text,
		glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
		glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
		Gamel::DEFAULT_COLOR);

	GL_ERRORS();















	/*
	static std::array< glm::vec2, 16 > const circle = [](){
		std::array< glm::vec2, 16 > ret;
		for (uint32_t a = 0; a < ret.size(); ++a) {
			float ang = a / float(ret.size()) * 2.0f * float(M_PI);
			ret[a] = glm::vec2(std::cos(ang), std::sin(ang));
		}
		return ret;
	}();

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	
	//figure out view transform to center the arena:
	float aspect = float(drawable_size.x) / float(drawable_size.y);
	float scale = std::min(
		2.0f * aspect / (Game::ArenaMax.x - Game::ArenaMin.x + 2.0f * Game::PlayerRadius),
		2.0f / (Game::ArenaMax.y - Game::ArenaMin.y + 2.0f * Game::PlayerRadius)
	);
	glm::vec2 offset = -0.5f * (Game::ArenaMax + Game::ArenaMin);

	glm::mat4 world_to_clip = glm::mat4(
		scale / aspect, 0.0f, 0.0f, offset.x,
		0.0f, scale, 0.0f, offset.y,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
	*/
	/*
	{
		DrawLines lines(world_to_clip);

		//helper:
		auto draw_text = [&](glm::vec2 const &at, std::string const &text, float H) {
			lines.draw_text(text,
				glm::vec3(at.x, at.y, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			float ofs = (1.0f / scale) / drawable_size.y;
			lines.draw_text(text,
				glm::vec3(at.x + ofs, at.y + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		};

		lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMin.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
		lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMax.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
		lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMin.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
		lines.draw(glm::vec3(Game::ArenaMax.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));

		for (auto const &player : game.players) {
			glm::u8vec4 col = glm::u8vec4(player.color.x*255, player.color.y*255, player.color.z*255, 0xff);
			if (&player == &game.players.front()) {
				//mark current player (which server sends first):
				lines.draw(
					glm::vec3(player.position + Game::PlayerRadius * glm::vec2(-0.5f,-0.5f), 0.0f),
					glm::vec3(player.position + Game::PlayerRadius * glm::vec2( 0.5f, 0.5f), 0.0f),
					col
				);
				lines.draw(
					glm::vec3(player.position + Game::PlayerRadius * glm::vec2(-0.5f, 0.5f), 0.0f),
					glm::vec3(player.position + Game::PlayerRadius * glm::vec2( 0.5f,-0.5f), 0.0f),
					col
				);
			}
			for (uint32_t a = 0; a < circle.size(); ++a) {
				lines.draw(
					glm::vec3(player.position + Game::PlayerRadius * circle[a], 0.0f),
					glm::vec3(player.position + Game::PlayerRadius * circle[(a+1)%circle.size()], 0.0f),
					col
				);
			}

			draw_text(player.position + glm::vec2(0.0f, -0.1f + Game::PlayerRadius), player.name, 0.09f);
		}
	}
	*/
	
}
