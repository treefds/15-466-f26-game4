#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <unordered_map>

#include "TextRenderer.hpp"

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//helper functions
	void draw_text(Scene::Drawable *line, std::string text);
	void load_illust(std::string name);

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, proceed;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//text lines
	Scene::Drawable *text_line_1 = nullptr;
	Scene::Drawable *text_line_2 = nullptr;
	Scene::Drawable *text_line_3 = nullptr;

	// bg & image
	Scene::Drawable *background = nullptr;
	Scene::Drawable *illust = nullptr;
	

	//music coming from the tip of the leg (as a demonstration):
	std::shared_ptr< Sound::PlayingSample > leg_tip_loop;

	//car honk sound:
	std::shared_ptr< Sound::PlayingSample > honk_oneshot;
	
	//camera:
	Scene::Camera *camera = nullptr;

	//text renderer object
	TextRenderer *text_renderer;

	//result
	std::vector<std::string> spotted;
	bool pelicaned = false;
	int bird_count = 0;
	bool ending_mode = false;
	int ending_counter = 0;

	//anim
	float time_since_proceed = 0.0f;
	float time_since_image_update = 0.0f;
	int current_option = 0;
	bool choice_mode = false;

	// Story Parser
	struct StoryParser {
		// the story script
		std::string script;
		// current line num
		int line_num = 0;

		StoryParser();
		StoryParser(std::string const &script);
		std::vector<std::string> get_next_lines(int index_selected);

	private:
		// parsed storylines
		std::vector<std::string> storylines;

		std::unordered_map<std::string, int> tag_to_line { }; 
	} parser { };

};
