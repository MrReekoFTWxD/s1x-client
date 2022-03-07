#include <std_include.hpp>
#include "engine.hpp"
#include "context.hpp"

#include "../../../component/ui_scripting.hpp"
#include "../../../component/game_module.hpp"


#include <utils/io.hpp>
#include <utils/string.hpp>

namespace ui_scripting::lua::engine
{
	namespace
	{
		auto& get_scripts()
		{
			static std::vector<std::unique_ptr<context>> scripts{};
			return scripts;
		}

		

		void load_scripts(const std::string& script_dir)
		{
			if (!utils::io::directory_exists(script_dir))
			{
				return;
			}

			const auto scripts = utils::io::list_files(script_dir);

			for (const auto& script : scripts)
			{
				if (std::filesystem::is_directory(script) && utils::io::file_exists(script + "/__init__.lua"))
				{
					get_scripts().push_back(std::make_unique<context>(script));
				}
			}
		}
	}

	void start()
	{
		clear_converted_functions();
		get_scripts().clear();
		load_scripts(game_module::get_host_module().get_folder() + "/data/ui_scripts/");
		load_scripts("s1x/ui_scripts/");
		load_scripts("data/ui_scripts/");

		if (!game::mod_folder.empty())
		{
			load_scripts(utils::string::va("%s/ui_scripts/", game::mod_folder.data()));
		}
	}

	void handle_key_event(const int key, const int down)
	{
		event event;
		event.name = down
			? "keydown"
			: "keyup";
		event.arguments = { key };

		ui_scripting::lua::engine::notify(event);
	}

	void handle_char_event(const int key)
	{
		std::string key_str = { (char)key };
		event event;
		event.name = "keypress";
		event.arguments = { key_str };

		ui_scripting::lua::engine::notify(event);
	}

	void stop()
	{
		clear_converted_functions();
		get_scripts().clear();
	}

	void ui_event(const std::string& type, const std::vector<int>& arguments)
	{
		if (type == "key")
		{
			handle_key_event(arguments[0], arguments[1]);
		}

		if (type == "char")
		{
			handle_char_event(arguments[0]);
		}
	}

	void notify(const event& e)
	{
		for (auto& script : get_scripts())
		{
			script->notify(e);
		}
	}

	void run_frame()
	{
		for (auto& script : get_scripts())
		{
			script->run_frame();
		}
	}
}
