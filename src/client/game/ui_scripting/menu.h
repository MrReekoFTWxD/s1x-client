#pragma once
#include "game/game.hpp"

#include "script_value.hpp"

namespace ui_scripting
{
	enum alignment
	{
		start,
		middle,
		end,
	};

	class element final
	{
	public:
		element();

		void set_horzalign(const std::string& value);
		void set_vertalign(const std::string& value);

		void set_text(const std::string& text);
		void set_font(const std::string& _font);
		void set_font(const std::string& _font, const int _fontsize);
		void set_color(float r, float g, float b, float a);
		void set_second_color(float r, float g, float b, float a);
		void set_glow_color(float r, float g, float b, float a);
		void set_text_offset(float x, float y);

		void set_scale(float x, float y);
		void set_rotation(float rotation);
		void set_style(int style);

		void set_background_color(float r, float g, float b, float a);
		void set_material(const std::string& material);

		void set_border_material(const std::string& material);
		void set_border_color(float r, float g, float b, float a);
		void set_border_width(float top);
		void set_border_width(float top, float right);
		void set_border_width(float top, float right, float bottom);
		void set_border_width(float top, float right, float bottom, float left);

		void set_slice(float left_percent, float top_percent, float right_percent, float bottom_percent);

		void set_rect(const float _x, const float _y, const float _w, const float _h);

		uint64_t id;

		void render() const;

		bool hidden = false;
		bool use_gradient = false;

		float x = 0.f;
		float y = 0.f;
		float w = 0.f;
		float h = 0.f;

		float rotation = 0;
		float x_scale = 1.f;
		float y_scale = 1.f;

		int style = 0;
		int fontsize = 20;

		float text_offset[2] = { 0.f, 0.f };
		float color[4] = { 1.f, 1.f, 1.f, 1.f };
		float second_color[4] = { 1.f, 1.f, 1.f, 1.f };
		float glow_color[4] = { 0.f, 0.f, 0.f, 0.f };
		float background_color[4] = { 0.f, 0.f, 0.f, 0.f };
		float border_color[4] = { 0.f, 0.f, 0.f, 0.f };
		float border_width[4] = { 0.f, 0.f, 0.f, 0.f };
		float slice[4] = { 0.f, 0.f, 1.f, 1.f };

		alignment horzalign = alignment::start;
		alignment vertalign = alignment::start;

		std::unordered_map<std::string, script_value> attributes = {};
		std::string font = "default";
		std::string material = "white";
		std::string border_material = "white";
		std::string text{};
	};
}

namespace ui_scripting
{
	enum menu_type
	{
		normal,
		overlay
	};

	class menu final
	{
	public:
		menu();

		bool visible = false;
		bool hidden = false;
		bool cursor = false;
		bool ignoreevents = false;
		bool cursor_was_enabled = false;

		void open();
		void close();

		void add_child(element* el);
		void render();

		menu_type type = normal;

		std::string overlay_menu;
		std::vector<element*> children{};
	};
}