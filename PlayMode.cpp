#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "load_save_png.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

#include <random>

GLuint VN_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > VN_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("VN.pnct"));
	VN_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > VN_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("VN.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = VN_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = VN_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
	});
});

Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("dusty-floor.opus"));
});


Load< Sound::Sample > honk_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("honk.wav"));
});


PlayMode::PlayMode() : scene(*VN_scene) {
	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//initialize drawable names
	for (auto &drawable : scene.drawables) {
		if (drawable.transform->name == "TextLine1") text_line_1 = &drawable;
		else if (drawable.transform->name == "TextLine2") text_line_2 = &drawable;
		else if (drawable.transform->name == "TextLine3") text_line_3 = &drawable;
		else if (drawable.transform->name == "Background") background = &drawable;
		else if (drawable.transform->name == "Illust") illust = &drawable;
	}
	if (text_line_1 == nullptr) throw std::runtime_error("Line 1 not found.");
	if (text_line_2 == nullptr) throw std::runtime_error("Line 2 not found.");
	if (text_line_3 == nullptr) throw std::runtime_error("Line 3 not found.");
	if (background == nullptr) throw std::runtime_error("Background not found.");
	if (illust == nullptr) throw std::runtime_error("Illust not found.");

	//intialize text renderer
	text_renderer = new TextRenderer("filibuster!\nWOW", data_path("font/NotoSerif.ttf"));

	//start music loop playing:
	// (note: position will be over-ridden in update())
	leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, glm::vec3(0.0, 0.0, 0.0), 10.0f);

	// overwrite the texture of one mesh

	for (Scene::Drawable &drawable : scene.drawables) {
		drawable.blended = true;
		Scene::Drawable *ptr = &drawable;
		drawable.pipeline.set_uniforms = [ptr]() {
			glUniform4fv(lit_color_texture_program->TINT_vec4, 1, glm::value_ptr(ptr->tint));
			glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 4);
			glUniform3f(lit_color_texture_program->LIGHT_DIRECTION_vec3, 0.0f, 0.0f, -1.0f);
			glUniform3f(lit_color_texture_program->LIGHT_ENERGY_vec3, 0.0f, 0.0f, 0.0f);
		};
	}

	// load the entire script
	std::ifstream file(data_path("script.txt"));

    if(!file.is_open()) throw std::runtime_error("Script missing.");
    std::stringstream buffer;
	buffer << file.rdbuf();
	std::string script = buffer.str();
    file.close();

	// copy text to parser
	parser = StoryParser(script);

	// Write sample line
	draw_text(text_line_1, "Schenley Birdwalk Simulator");
}

PlayMode::~PlayMode() {
	text_renderer->~TextRenderer();
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_ESCAPE) {
			SDL_SetWindowRelativeMouseMode(Mode::window, false);
			return true;
		} else if (evt.key.key == SDLK_A) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			if (honk_oneshot) honk_oneshot->stop();
			honk_oneshot = Sound::play_3D(*honk_sample, 0.3f, glm::vec3(4.6f, -7.8f, 6.9f)); //hardcoded position of front of car, from blender
			proceed.downs += 1;
			proceed.pressed = true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			proceed.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == false) {
			SDL_SetWindowRelativeMouseMode(Mode::window, true);
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == true) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	// story progerssion
	if (proceed.downs) {
		std::vector<std::string> lines = parser.get_next_lines(0);
		for (size_t idx = 0; idx < lines.size() && idx < 3; ++idx) {
			if (idx == 0) {
				draw_text(text_line_1, lines[idx]);
			} else if (idx == 1) {
				draw_text(text_line_2, lines[idx]);
			} else if (idx == 2) {
				draw_text(text_line_3, lines[idx]);
			}
		}
		for (size_t idx = lines.size(); idx < 3; ++idx) {
			if (idx == 0) {
				draw_text(text_line_1, " ");
			} else if (idx == 1) {
				draw_text(text_line_2, " ");
			} else if (idx == 2) {
				draw_text(text_line_3, " ");
			}
		}
	}

	

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	proceed.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}

// Helper function to draw one single line of text onto one of the line objects
void PlayMode::draw_text(Scene::Drawable *line, std::string text) {
	const float scale_factor = 0.04f;
	// set text
	text_renderer->text = text;
	size_t width = 0;
	size_t height = 0;
	std::vector<glm::u8vec4> pixels = text_renderer->Rasterize(100, width, height);
	
	line->transform->scale = glm::vec3(
		static_cast<float>(width) / FONT_SIZE * scale_factor,
		static_cast<float>(height) / FONT_SIZE * scale_factor,
		1.0f
	);

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_RGBA, // this is RGBA
		width, height,
		0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	line->pipeline.textures[0].texture = tex;
	line->pipeline.textures[0].target = GL_TEXTURE_2D;
}


PlayMode::StoryParser::StoryParser() {
	storylines = std::vector<std::string>(0);
}

// Story parsing functionalities
PlayMode::StoryParser::StoryParser(std::string const &script): script(script) {
	std::stringstream ss(script);
	
	// go through the entire string
	// ref: https://stackoverflow.com/questions/13172158/c-split-string-by-line
	std::string line;
	while (std::getline(ss, line, '\n')) {
		// filter out empty strings also, we don't need it
		if (line.length()) {
			storylines.emplace_back(line);
		}
	}

	for (int idx = 0; idx < storylines.size(); idx++) {
		if (storylines[idx][0] != '=') {
			continue;
		}
		line = storylines[idx].substr(1);
		line.erase(0, line.find_first_not_of(' '));
		tag_to_line[line] = idx;
	}
}

// Handling next line
std::vector<std::string> PlayMode::StoryParser::get_next_lines(int index_selected) {
	if (line_num >= storylines.size()) {
		return std::vector<std::string>(0);
	}

	std::string jump_flag = "";
	if (storylines[line_num][0] == '~') {
		std::cout << "~\n";
		// probe for options
		int probe = line_num + 1;
		int option_now = -1;
		bool found = false;
		while (option_now < 1 && !found) {
			if (probe >= storylines.size()) {
				break;
			}
			// * = option
			if (storylines[probe][0] == '*') {
				option_now++;
			}
			if (option_now != index_selected) {
				continue;
			}
			// selected option; probe for `>`
			probe++;
			while (probe < storylines.size()) {
				if (storylines[probe][0] == '>') {
					// found the jump flag
					line_num = probe;
					found = true;
					break;
				}
			}
		}
	} else {
		// continue to next line first
		line_num++;
		while (line_num < storylines.size()) {
			if (storylines[line_num][0] == '>' || 
				storylines[line_num][0] == '~' ||
				storylines[line_num][0] == '+' ||
				storylines[line_num][0] == ':'
			) {
				break;
			}
			line_num++;
		}
	}
	// If line num exceeded again, return nothing
	if (line_num >= storylines.size()) {
		return std::vector<std::string>(0);
	}

	// If we are on `>`, jump immediately
	if (storylines[line_num][0] == '>') {
		std::string tag = storylines[line_num].substr(1);
		tag.erase(0, tag.find_first_not_of(' '));
		int jump_to = tag_to_line[tag];
		// there is a chance jump_to is invalid?
		std::cout << "JUMP:" << tag << " (" << jump_to << ")" << std::endl;
		line_num = jump_to + 1;
	}

	std::vector<std::string> results(0);
	// act differently based on the type of line
	if (storylines[line_num][0] == ':') {
		// A regular text
		results.emplace_back(storylines[line_num].substr(1));
		int probe = line_num + 1;
		while (probe < storylines.size()) {
			if (storylines[probe][0] == '>' || 
				storylines[probe][0] == '~' ||
				storylines[probe][0] == '+' ||
				storylines[probe][0] == ':'
			) {
				break;
			} else if (storylines[probe][0] == '=') {
				// nothing
			} else {
				results.emplace_back(storylines[probe]);
			}
			probe++;
		}
	} else if (storylines[line_num][0] == '+') {
		results.emplace_back(storylines[line_num].substr(0));
	} else if (storylines[line_num][0] == '~') {
		results.emplace_back(storylines[line_num].substr(1));
		int probe = line_num + 1;
		int cnt = 0;
		while (probe < storylines.size()) {
			if (storylines[probe][0] == '~' ||
				storylines[probe][0] == '+' ||
				storylines[probe][0] == ':'
			) {
				break;
			} else if (storylines[probe][0] == '=') {
				// nothing
			} else if (storylines[probe][0] == '*') {
				// an option
				results.emplace_back(storylines[probe]);
				cnt += 1;
			} else {
				// ?
			}
			if (cnt >= 2) {
				break;
			}
			probe++;
		}
	}

	for (std::string l : results) {
		std::cout << l << std::endl;
	}

	return results;
}