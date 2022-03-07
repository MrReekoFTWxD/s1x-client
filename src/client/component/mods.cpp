#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "game/game.hpp"

#include "component/command.hpp"
#include "component/game_console.hpp"
#include "component/console.hpp"
#include "component/scheduler.hpp"

#include <utils/hook.hpp>
#include <utils/io.hpp>
#include <utils/string.hpp>

namespace mods
{
	class component final : public component_interface
	{
	public:

		void post_unpack() override
		{
			if (!utils::io::directory_exists("s1x/mods"))
			{
				utils::io::create_directory("s1x/mods");
			}

			command::add("loadmod", [](const command::params& params)
				{
					if (!::game::VirtualLobby_Loaded())
					{
						game_console::print_internal("Cannot load mod while in-game!\n");
						game::CG_GameMessage(0, "^1Cannot load mod while in-game!");
						return;
					}

					const auto path = params.get(1);
					game_console::print_internal("Loading mod %s\n", path);

					if (!utils::io::directory_exists(path))
					{
						game_console::print_internal("Mod %s not found!\n", path);
						return;
					}

					game::mod_folder = path;

					scheduler::once([]()
						{
							command::execute("lui_restart", true);

							INPUT ip;
							ip.type = INPUT_KEYBOARD;
							ip.ki.wScan = 0; // hardware scan code for key
							ip.ki.time = 0;
							ip.ki.dwExtraInfo = 0;

							ip.ki.wVk = VK_ESCAPE; // virtual-key code for the "a" key
							ip.ki.dwFlags = 0; // 0 for key press
							SendInput(1, &ip, sizeof(INPUT));

						}, scheduler::pipeline::renderer);
				});

			command::add("unloadmod", [](const command::params& params)
				{
					if (game::mod_folder.empty())
					{
						game_console::print(console::con_type_info, "No mod loaded\n");
						return;
					}

					if (!::game::VirtualLobby_Loaded())
					{
						game_console::print_internal("Cannot unload mod while in-game!\n");
						game::CG_GameMessage(0, "^1Cannot unload mod while in-game!");
						return;
					}

					game_console::print_internal("Unloading mod %s\n", game::mod_folder.data());
					game::mod_folder.clear();

					scheduler::once([]()
						{
							command::execute("lui_restart", true);

							INPUT ip;
							ip.type = INPUT_KEYBOARD;
							ip.ki.wScan = 0; // hardware scan code for key
							ip.ki.time = 0;
							ip.ki.dwExtraInfo = 0;

							ip.ki.wVk = VK_ESCAPE; // virtual-key code for the "a" key
							ip.ki.dwFlags = 0; // 0 for key press
							SendInput(1, &ip, sizeof(INPUT));
							SendInput(1, &ip, sizeof(INPUT));

						}, scheduler::pipeline::renderer);
				});
		}
	};
}

REGISTER_COMPONENT(mods::component)