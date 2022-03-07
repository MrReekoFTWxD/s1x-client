#include <std_include.hpp>
#include "context.hpp"
#include "error.hpp"
#include "value_conversion.hpp"
#include "../script_value.hpp"
#include "../execution.hpp"

#include "../../../component/ui_scripting.hpp"
#include "../../../component/scripting.hpp"
#include "../../../game/scripting/vector.hpp"
#include "../../../component/command.hpp"

#include "component/game_console.hpp"
#include "component/scheduler.hpp"

#include <utils/string.hpp>
#include <utils/nt.hpp>
#include <utils/io.hpp>

#include "game/symbols.hpp"
#include "game/game.hpp"


namespace ui_scripting::lua
{
	event_handler::event_handler(sol::state& state)
		: state_(state)
	{
		auto event_listener_handle_type = state.new_usertype<event_listener_handle>("event_listener_handle");

		event_listener_handle_type["clear"] = [this](const event_listener_handle& handle)
		{
			this->remove(handle);
		};

		event_listener_handle_type["endon"] = [this](const event_listener_handle& handle, const std::string& event)
		{
			this->add_endon_condition(handle, event);
		};
	}

	void event_handler::dispatch(const event& event)
	{
		bool has_built_arguments = false;
		event_arguments arguments{};

		callbacks_.access([&](task_list& tasks)
			{
				this->merge_callbacks();
				this->handle_endon_conditions(event);

				for (auto i = tasks.begin(); i != tasks.end();)
				{
					if (i->event != event.name)
					{
						++i;
						continue;
					}

					if (!i->is_deleted)
					{
						if (!has_built_arguments)
						{
							has_built_arguments = true;
							arguments = this->build_arguments(event);
						}

						handle_error(i->callback(sol::as_args(arguments)));
					}

					if (i->is_volatile || i->is_deleted)
					{
						i = tasks.erase(i);
					}
					else
					{
						++i;
					}
				}
			});
	}

	event_listener_handle event_handler::add_event_listener(event_listener&& listener)
	{
		const uint64_t id = ++this->current_listener_id_;
		listener.id = id;
		listener.is_deleted = false;

		new_callbacks_.access([&listener](task_list& tasks)
			{
				tasks.emplace_back(std::move(listener));
			});

		return { id };
	}

	void event_handler::add_endon_condition(const event_listener_handle& handle, const std::string& event)
	{
		auto merger = [&](task_list& tasks)
		{
			for (auto& task : tasks)
			{
				if (task.id == handle.id)
				{
					task.endon_conditions.emplace_back(event);
				}
			}
		};

		callbacks_.access([&](task_list& tasks)
			{
				merger(tasks);
				new_callbacks_.access(merger);
			});
	}

	void event_handler::clear()
	{
		callbacks_.access([&](task_list& tasks)
			{
				new_callbacks_.access([&](task_list& new_tasks)
					{
						new_tasks.clear();
						tasks.clear();
					});
			});
	}

	void event_handler::remove(const event_listener_handle& handle)
	{
		auto mask_as_deleted = [&](task_list& tasks)
		{
			for (auto& task : tasks)
			{
				if (task.id == handle.id)
				{
					task.is_deleted = true;
					break;
				}
			}
		};

		callbacks_.access(mask_as_deleted);
		new_callbacks_.access(mask_as_deleted);
	}

	void event_handler::merge_callbacks()
	{
		callbacks_.access([&](task_list& tasks)
			{
				new_callbacks_.access([&](task_list& new_tasks)
					{
						tasks.insert(tasks.end(), std::move_iterator<task_list::iterator>(new_tasks.begin()),
							std::move_iterator<task_list::iterator>(new_tasks.end()));
						new_tasks = {};
					});
			});
	}

	void event_handler::handle_endon_conditions(const event& event)
	{
		auto deleter = [&](task_list& tasks)
		{
			for (auto& task : tasks)
			{
				for (auto& condition : task.endon_conditions)
				{
					if (condition == event.name)
					{
						task.is_deleted = true;
						break;
					}
				}
			}
		};

		callbacks_.access(deleter);
	}

	event_arguments event_handler::build_arguments(const event& event) const
	{
		event_arguments arguments;

		for (const auto& argument : event.arguments)
		{
			arguments.emplace_back(convert(this->state_, argument));
		}

		return arguments;
	}
}

namespace ui_scripting::lua
{
	namespace
	{

		void setup_io(sol::state& state)
		{
			state["io"]["fileexists"] = utils::io::file_exists;
			state["io"]["writefile"] = utils::io::write_file;
			state["io"]["movefile"] = utils::io::move_file;
			state["io"]["filesize"] = utils::io::file_size;
			state["io"]["createdirectory"] = utils::io::create_directory;
			state["io"]["directoryexists"] = utils::io::directory_exists;
			state["io"]["directoryisempty"] = utils::io::directory_is_empty;
			state["io"]["listfiles"] = utils::io::list_files;
			state["io"]["copyfolder"] = utils::io::copy_folder;
			state["io"]["removefile"] = utils::io::remove_file;
			state["io"]["readfile"] = static_cast<std::string(*)(const std::string&)>(utils::io::read_file);
		}

		bool valid_dvar_name(const std::string& name)
		{
			for (const auto c : name)
			{
				if (!isalnum(c) && c != '_')
				{
					return false;
				}
			}

			return true;
		}

		void setup_types(sol::state& state, scheduler& scheduler, event_handler& handler)
		{
			struct game
			{
			};
			auto game_type = state.new_usertype<game>("game_");
			state["game"] = game();


			game_type["getloadedmod"] = [](const game&)
			{
				return ::game::mod_folder;
			};

			game_type["executecommand"] = [](const game&, const std::string& command)
			{
				using namespace game;
				game_console::print_internal("Mod Folder: %s", mod_folder.c_str());
				command::execute(command, false);
			};

			game_type["ontimeout"] = [&scheduler](const game&, const sol::protected_function& callback,
			                                      const long long milliseconds)
			{
				return scheduler.add(callback, milliseconds, true);
			};

			game_type["oninterval"] = [&scheduler](const game&, const sol::protected_function& callback,
			                                       const long long milliseconds)
			{
				return scheduler.add(callback, milliseconds, false);
			};

			game_type["onnotify"] = [&handler](const game&, const std::string& event,
				const event_callback& callback)
			{
				event_listener listener{};
				listener.callback = callback;
				listener.event = event;
				listener.is_volatile = false;

				return handler.add_event_listener(std::move(listener));
			};

			game_type["onnotifyonce"] = [&handler](const game&, const std::string& event,
				const event_callback& callback)
			{
				event_listener listener{};
				listener.callback = callback;
				listener.event = event;
				listener.is_volatile = true;

				return handler.add_event_listener(std::move(listener));
			};

			game_type["isingame"] = []()
			{
				return ::game::CL_IsCgameInitialized() && ::game::mp::g_entities[0].client;
			};

			game_type["getdvar"] = [](const game&, const sol::this_state s, const std::string& name)
			{
				const auto dvar = ::game::Dvar_FindVar(name.data());
				if (!dvar)
				{
					return sol::lua_value{ s, sol::lua_nil };
				}

				const std::string value = ::game::Dvar_ValueToString(dvar, dvar->current);
				return sol::lua_value{ s, value };
			};

			game_type["getdvarint"] = [](const game&, const sol::this_state s, const std::string& name)
			{
				const auto dvar = ::game::Dvar_FindVar(name.data());
				if (!dvar)
				{
					return sol::lua_value{ s, sol::lua_nil };
				}

				const auto value = atoi(::game::Dvar_ValueToString(dvar, dvar->current));
				return sol::lua_value{ s, value };
			};

			game_type["getdvarfloat"] = [](const game&, const sol::this_state s, const std::string& name)
			{
				const auto dvar = ::game::Dvar_FindVar(name.data());
				if (!dvar)
				{
					return sol::lua_value{ s, sol::lua_nil };
				}

				const auto value = atof(::game::Dvar_ValueToString(dvar, dvar->current));
				return sol::lua_value{ s, value };
			};

			game_type["setdvar"] = [](const game&, const std::string& name, const sol::lua_value& value)
			{
				if (!valid_dvar_name(name))
				{
					throw std::runtime_error("Invalid DVAR name, must be alphanumeric");
				}

				std::string string_value;

				if (value.is<bool>())
				{
					string_value = utils::string::va("%i", value.as<bool>());
				}
				else if (value.is<int>())
				{
					string_value = utils::string::va("%i", value.as<int>());
				}
				else if (value.is<float>())
				{
					string_value = utils::string::va("%f", value.as<float>());
				}
				else if (value.is<scripting::vector>())
				{
					const auto v = value.as<scripting::vector>();
					string_value = utils::string::va("%f %f %f",
						v.get_x(),
						v.get_y(),
						v.get_z()
					);
				}

				if (value.is<std::string>())
				{
					string_value = value.as<std::string>();
				}

				::game::Dvar_SetCommand(name.data(), string_value.data());
			};

			game_type["getwindowsize"] = [](const game&, const sol::this_state s)
			{
				const auto size = ::game::ScrPlace_GetViewPlacement()->realViewportSize;

				auto screen = sol::table::create(s.lua_state());
				screen["x"] = size[0];
				screen["y"] = size[1];

				return screen;
			};

			game_type["registermaterial"] = [](const game&, const std::string& command)
			{
				return ::game::Material_RegisterHandle(command.c_str());
			};

			auto userdata_type = state.new_usertype<userdata>("userdata_");

			userdata_type["new"] = sol::property(
				[](const userdata& userdata, const sol::this_state s)
				{
					return convert(s, userdata.get("new"));
				},
				[](const userdata& userdata, const sol::this_state s, const sol::lua_value& value)
				{
					userdata.set("new", convert({s, value}));
				}
			);

			
			userdata_type["get"] = [](const userdata& userdata, const sol::this_state s,
				const sol::lua_value& key)
			{
				return convert(s, userdata.get(convert({s, key})));
			};

			userdata_type["set"] = [](const userdata& userdata, const sol::this_state s,
				const sol::lua_value& key, const sol::lua_value& value)
			{
				userdata.set(convert({s, key}), convert({s, value}));
			};

			userdata_type[sol::meta_function::index] = [](const userdata& userdata, const sol::this_state s, 
				const sol::lua_value& key)
			{
				return convert(s, userdata.get(convert({s, key})));
			};

			userdata_type[sol::meta_function::new_index] = [](const userdata& userdata, const sol::this_state s, 
				const sol::lua_value& key, const sol::lua_value& value)
			{
				userdata.set(convert({s, key }), convert({s, value}));
			};

			auto table_type = state.new_usertype<table>("table_");

			table_type["new"] = sol::property(
				[](const table& table, const sol::this_state s)
				{
					return convert(s, table.get("new"));
				},
				[](const table& table, const sol::this_state s, const sol::lua_value& value)
				{
					table.set("new", convert({s, value}));
				}
			);

			table_type["get"] = [](const table& table, const sol::this_state s,
				const sol::lua_value& key)
			{
				return convert(s, table.get(convert({s, key})));
			};

			table_type["set"] = [](const table& table, const sol::this_state s,
				const sol::lua_value& key, const sol::lua_value& value)
			{
				table.set(convert({s, key}), convert({s, value}));
			};

			table_type[sol::meta_function::index] = [](const table& table, const sol::this_state s,
				const sol::lua_value& key)
			{
				return convert(s, table.get(convert({s, key})));
			};

			table_type[sol::meta_function::new_index] = [](const table& table, const sol::this_state s,
				const sol::lua_value& key, const sol::lua_value& value)
			{
				table.set(convert({s, key}), convert({s, value}));
			};

			auto function_type = state.new_usertype<function>("function_");

			function_type[sol::meta_function::call] = [](const function& function, const sol::this_state s, sol::variadic_args va)
			{
				arguments arguments{};

				for (auto arg : va)
				{
					arguments.push_back(convert({s, arg}));
				}

				const auto values = function.call(arguments);
				std::vector<sol::lua_value> returns;

				for (const auto& value : values)
				{
					returns.push_back(convert(s, value));
				}

				return sol::as_returns(returns);
			};

			state["luiglobals"] = table((*::game::hks::lua_state)->globals.v.table);
			state["CoD"] = state["luiglobals"]["CoD"];
			state["LUI"] = state["luiglobals"]["LUI"];
			state["Engine"] = state["luiglobals"]["Engine"];
			state["Game"] = state["luiglobals"]["Game"];
		}
	}

	context::context(std::string folder)
		: folder_(std::move(folder))
		  , scheduler_(state_)
		, event_handler_(state_)
	{
		this->state_.open_libraries(sol::lib::base,
		                            sol::lib::package,
		                            sol::lib::io,
		                            sol::lib::string,
		                            sol::lib::os,
		                            sol::lib::math,
		                            sol::lib::table);

		this->state_["include"] = [this](const std::string& file)
		{
			this->load_script(file);
		};

		sol::function old_require = this->state_["require"];
		auto base_path = utils::string::replace(this->folder_, "/", ".") + ".";
		this->state_["require"] = [base_path, old_require](const std::string& path)
		{
			return old_require(base_path + path);
		};

		this->state_["scriptdir"] = [this]()
		{
			return this->folder_;
		};

		setup_io(this->state_);
		setup_types(this->state_, this->scheduler_, this->event_handler_);

		printf("Loading ui script '%s'\n", this->folder_.data());
		this->load_script("__init__");
	}

	context::~context()
	{
		this->state_.collect_garbage();
		this->scheduler_.clear();
		this->state_ = {};
	}

	void context::run_frame()
	{
		this->scheduler_.run_frame();
		this->state_.collect_garbage();
	}

	void context::notify(const event& e)
	{
		this->scheduler_.dispatch(e);
		this->event_handler_.dispatch(e);
	}

	void context::load_script(const std::string& script)
	{
		if (!this->loaded_scripts_.emplace(script).second)
		{
			return;
		}

		const auto file = (std::filesystem::path{this->folder_} / (script + ".lua")).generic_string();
		handle_error(this->state_.safe_script_file(file, &sol::script_pass_on_error));
	}
}
