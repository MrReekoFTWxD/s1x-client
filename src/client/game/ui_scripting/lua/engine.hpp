#pragma once

#include "../event.hpp"

namespace ui_scripting::lua::engine
{
	void start();
	void stop();
	void run_frame();

	void ui_event(const std::string& type, const std::vector<int>& arguments);

	void notify(const event& e);
}
