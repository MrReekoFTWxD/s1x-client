#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "scheduler.hpp"

#include "game/scripting/entity.hpp"
#include "game/scripting/execution.hpp"
#include "game/scripting/lua/value_conversion.hpp"
#include "game/scripting/lua/error.hpp"

#include <utils/hook.hpp>

#include "logfile.hpp"

namespace logfile
{
	std::unordered_map<const char*, sol::protected_function> vm_execute_hooks;

	namespace
	{
		utils::hook::detour scr_player_killed_hook;
		utils::hook::detour scr_player_damage_hook;

		std::vector<sol::protected_function> player_killed_callbacks;
		std::vector<sol::protected_function> player_damage_callbacks;

		utils::hook::detour vm_execute_hook;
		char empty_function[2] = {0x32, 0x34}; // CHECK_CLEAR_PARAMS, END
		bool hook_enabled = true;

		sol::lua_value convert_entity(lua_State* state, const game::mp::gentity_s* ent)
		{
			if (!ent)
			{
				return {};
			}

			const auto player = scripting::call("getEntByNum", {ent->s.entityNum});

			return scripting::lua::convert(state, player);
		}

		std::string get_weapon_name(unsigned int weapon, bool isAlternate)
		{
			char output[1024] = { 0 };
			game::BG_GetWeaponNameComplete(weapon, isAlternate, output, 1024);

			return output;
		}

		sol::lua_value convert_vector(lua_State* state, const float* vec)
		{
			if (!vec)
			{
				return {};
			}

			const auto _vec = scripting::vector(vec);

			return scripting::lua::convert(state, _vec);
		}

		std::string convert_mod(const int meansOfDeath)
		{
			const auto value = reinterpret_cast<game::scr_string_t**>(0x1409B5360)[meansOfDeath];
			const auto string = game::SL_ConvertToString(*value);
			
			return string;
		}

		void scr_player_killed_stub(game::mp::gentity_s* self, const game::mp::gentity_s* inflictor, game::mp::gentity_s* attacker, int damage,
		                            const int meansOfDeath, const unsigned int weapon, const bool isAlternate, const float* vDir, const unsigned int hitLoc, int psTimeOffset, int deathAnimDuration)
		{
			{
				const std::string _hitLoc = reinterpret_cast<const char**>(0x1409B5400)[hitLoc];
				const auto _mod = convert_mod(meansOfDeath);

				const auto _weapon = get_weapon_name(weapon, isAlternate);

				for (const auto& callback : player_killed_callbacks)
				{
					const auto state = callback.lua_state();

					const auto _self = convert_entity(state, self);
					const auto _inflictor = convert_entity(state, inflictor);
					const auto _attacker = convert_entity(state, attacker);

					const auto _vDir = convert_vector(state, vDir);

					const auto result = callback(_self, _inflictor, _attacker, damage, _mod, _weapon, _vDir, _hitLoc, psTimeOffset, deathAnimDuration);

					scripting::lua::handle_error(result);

					if (result.valid() && result.get_type() == sol::type::number)
					{
						damage = result.get<int>();
					}
				}

				if (damage == 0)
				{
					return;
				}
			}

			scr_player_killed_hook.invoke<void>(self, inflictor, attacker, damage, meansOfDeath, weapon, isAlternate, vDir, hitLoc, psTimeOffset, deathAnimDuration);
		}

		void scr_player_damage_stub(game::mp::gentity_s* self, const game::mp::gentity_s* inflictor, game::mp::gentity_s* attacker, int damage, int dflags,
		                            const int meansOfDeath, const unsigned int weapon, const bool isAlternate, const float* vPoint, const float* vDir, const unsigned int hitLoc, const int timeOffset)
		{
			{
				const std::string _hitLoc = reinterpret_cast<const char**>(0x1409B5400)[hitLoc];
				const auto _mod = convert_mod(meansOfDeath);

				const auto _weapon = get_weapon_name(weapon, isAlternate);

				for (const auto& callback : player_damage_callbacks)
				{
					const auto state = callback.lua_state();

					const auto _self = convert_entity(state, self);
					const auto _inflictor = convert_entity(state, inflictor);
					const auto _attacker = convert_entity(state, attacker);

					const auto _vPoint = convert_vector(state, vPoint);
					const auto _vDir = convert_vector(state, vDir);

					const auto result = callback(_self, _inflictor, _attacker, damage, dflags, _mod, _weapon, _vPoint, _vDir, _hitLoc);

					scripting::lua::handle_error(result);

					if (result.valid() && result.get_type() == sol::type::number)
					{
						damage = result.get<int>();
					}
				}

				if (damage == 0)
				{
					return;
				}
			}

			scr_player_damage_hook.invoke<void>(self, inflictor, attacker, damage, dflags, meansOfDeath, weapon, isAlternate, vPoint, vDir, hitLoc, timeOffset);
		}

		void client_command_stub(const int clientNum)
		{
			auto self = &game::mp::g_entities[clientNum];
			char cmd[1024]{};

			game::SV_Cmd_ArgvBuffer(0, cmd, 1024);

			if (cmd == "say"s || cmd == "say_team"s)
			{
				auto hidden = false;
				std::string message(game::ConcatArgs(1));

				hidden = message[1] == '/';
				message.erase(0, hidden ? 2 : 1);

				scheduler::once([cmd, message, self]()
				{
					const scripting::entity level{*game::levelEntityId};
					const auto player = scripting::call("getEntByNum", {self->s.entityNum}).as<scripting::entity>();

					scripting::notify(level, cmd, {player, message});
					scripting::notify(player, cmd, {message});
				}, scheduler::pipeline::server);

				if (hidden)
				{
					return;
				}
			}

			// ClientCommand
			return reinterpret_cast<void(*)(int)>(0x1402E98F0)(clientNum);
		}

		void g_shutdown_game_stub(const int freeScripts)
		{
			{
				const scripting::entity level{*game::levelEntityId};
				scripting::notify(level, "shutdownGame_called", {1});
			}
			
			// G_ShutdownGame
			return reinterpret_cast<void(*)(int)>(0x1402F8C10)(freeScripts);
		}

		unsigned int local_id_to_entity(unsigned int local_id)
		{
			const auto variable = game::scr_VarGlob->objectVariableValue[local_id];
			return variable.u.f.next;
		}

		bool execute_vm_hook(const char* pos)
		{
			if (vm_execute_hooks.find(pos) == vm_execute_hooks.end())
			{
				hook_enabled = true;
				return false;
			}

			if (!hook_enabled && pos > reinterpret_cast<char*>(vm_execute_hooks.size()))
			{
				hook_enabled = true;
				return false;
			}

			const auto hook = vm_execute_hooks[pos];
			const auto state = hook.lua_state();

			const scripting::entity self = local_id_to_entity(game::scr_VmPub->function_frame->fs.localId);

			std::vector<sol::lua_value> args;

			const auto top = game::scr_function_stack->top;

			for (auto* value = top; value->type != game::SCRIPT_END; --value)
			{
				args.push_back(scripting::lua::convert(state, *value));
			}

			const auto result = hook(self, sol::as_args(args));
			scripting::lua::handle_error(result);

			return true;
		}

		void vm_execute_stub(utils::hook::assembler& a)
		{
			const auto replace = a.newLabel();
			const auto end = a.newLabel();

			a.pushad64();

			a.mov(rcx, r14);
			a.call_aligned(execute_vm_hook);

			a.cmp(al, 0);
			a.jne(replace);

			a.popad64();
			a.jmp(end);

			a.bind(end);

			a.movzx(r15d, byte_ptr(r14));
			a.inc(r14);
			a.lea(eax, dword_ptr(r15, -0x17));
			a.mov(dword_ptr(rbp, 0x68), r15d);

			a.jmp(0x1403FA143);

			a.bind(replace);

			a.popad64();
			a.mov(r14, reinterpret_cast<char*>(empty_function));
			a.jmp(end);
		}
	}

	void add_player_damage_callback(const sol::protected_function& callback)
	{
		player_damage_callbacks.push_back(callback);
	}

	void add_player_killed_callback(const sol::protected_function& callback)
	{
		player_killed_callbacks.push_back(callback);
	}

	void clear_callbacks()
	{
		player_damage_callbacks.clear();
		player_killed_callbacks.clear();
		vm_execute_hooks.clear();
	}

	void enable_vm_execute_hook()
	{
		hook_enabled = true;
	}

	void disable_vm_execute_hook()
	{
		hook_enabled = false;
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_sp())
			{
				return;
			}

			utils::hook::call(0x14043A9AD, client_command_stub);

			scr_player_damage_hook.create(0x140332150, scr_player_damage_stub);
			scr_player_killed_hook.create(0x1403323D0, scr_player_killed_stub);

			utils::hook::call(0x14043E550, g_shutdown_game_stub);
			utils::hook::call(0x14043EA11, g_shutdown_game_stub);

			utils::hook::jump(0x1403FA134, utils::hook::assemble(vm_execute_stub), true);
		}
	};
}

REGISTER_COMPONENT(logfile::component)
