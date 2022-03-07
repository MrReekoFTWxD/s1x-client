#pragma once

#include "../event.hpp"

#pragma warning(push)
#pragma warning(disable: 4702)

#define SOL_ALL_SAFETIES_ON 1
#define SOL_PRINT_ERRORS 0
#include <sol/sol.hpp>

#include "scheduler.hpp"



namespace ui_scripting::lua
{
	using event_arguments = std::vector<sol::lua_value>;
	using event_callback = sol::protected_function;

	class event_listener_handle
	{
	public:
		uint64_t id = 0;
	};

	class event_listener final : public event_listener_handle
	{
	public:
		std::string event = {};
		event_callback callback = {};
		bool is_volatile = false;
		bool is_deleted = false;
		std::vector<std::string> endon_conditions{};
	};

	class event_handler final
	{
	public:
		event_handler(sol::state& state);

		event_handler(event_handler&&) noexcept = delete;
		event_handler& operator=(event_handler&&) noexcept = delete;

		event_handler(const scheduler&) = delete;
		event_handler& operator=(const event_handler&) = delete;

		void dispatch(const event& event);

		event_listener_handle add_event_listener(event_listener&& listener);

		void clear();

	private:
		sol::state& state_;
		std::atomic_int64_t current_listener_id_ = 0;

		using task_list = std::vector<event_listener>;
		utils::concurrency::container<task_list> new_callbacks_;
		utils::concurrency::container<task_list, std::recursive_mutex> callbacks_;

		void remove(const event_listener_handle& handle);
		void merge_callbacks();
		void handle_endon_conditions(const event& event);

		void add_endon_condition(const event_listener_handle& handle, const std::string& event);

		event_arguments build_arguments(const event& event) const;
	};
}


namespace ui_scripting::lua
{
	class context
	{
	public:
		context(std::string folder);
		~context();

		context(context&&) noexcept = delete;
		context& operator=(context&&) noexcept = delete;

		context(const context&) = delete;
		context& operator=(const context&) = delete;

		void run_frame();
		void notify(const event& e);

	private:
		sol::state state_{};
		std::string folder_;
		std::unordered_set<std::string> loaded_scripts_;

		scheduler scheduler_;
		event_handler event_handler_;

		void load_script(const std::string& script);
	};
}
