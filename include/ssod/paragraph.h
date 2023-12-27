#pragma once
#include <string>
#include <vector>
#include <dpp/dpp.h>
#include <ssod/game_player.h>
#include <dpp/unicode_emoji.h>

inline const char* directions[] = {
	dpp::unicode_emoji::zero,
	dpp::unicode_emoji::one,
	dpp::unicode_emoji::two,
	dpp::unicode_emoji::three,
	dpp::unicode_emoji::four,
	dpp::unicode_emoji::five,
	dpp::unicode_emoji::six,
	dpp::unicode_emoji::seven,
	dpp::unicode_emoji::eight,
	dpp::unicode_emoji::nine,
	dpp::unicode_emoji::regional_indicator_a,
	dpp::unicode_emoji::regional_indicator_b,
	dpp::unicode_emoji::regional_indicator_c,
	dpp::unicode_emoji::regional_indicator_d,
	dpp::unicode_emoji::regional_indicator_e,
	dpp::unicode_emoji::regional_indicator_f,
	dpp::unicode_emoji::regional_indicator_g,
	dpp::unicode_emoji::regional_indicator_h,
	dpp::unicode_emoji::regional_indicator_i,
	dpp::unicode_emoji::regional_indicator_j,
	dpp::unicode_emoji::regional_indicator_k,
	dpp::unicode_emoji::regional_indicator_l,
	dpp::unicode_emoji::regional_indicator_m,
	dpp::unicode_emoji::regional_indicator_n,
	dpp::unicode_emoji::regional_indicator_o,
	dpp::unicode_emoji::regional_indicator_p,
	dpp::unicode_emoji::regional_indicator_q,
	dpp::unicode_emoji::regional_indicator_r,
	dpp::unicode_emoji::regional_indicator_s,
	dpp::unicode_emoji::regional_indicator_t,
	dpp::unicode_emoji::regional_indicator_u,
	dpp::unicode_emoji::regional_indicator_v,
	dpp::unicode_emoji::regional_indicator_w,
	dpp::unicode_emoji::regional_indicator_x,
	dpp::unicode_emoji::regional_indicator_y,
	dpp::unicode_emoji::regional_indicator_z,
};

enum nav_link_type {
	nav_type_disabled_link,
	nav_type_link,
	nav_type_paylink,
	nav_type_autolink,
	nav_type_modal,
	nav_type_shop,
	nav_type_combat,
	nav_type_bank,
	nav_type_respawn,
	nav_type_pick_one,
};

struct nav_link {
	long paragraph;
	nav_link_type type;
	long cost;
	enemy monster;
	item buyable;
};

struct paragraph {
	uint32_t id{};
	std::string text;
	std::string secure_id;
	std::vector<nav_link> navigation_links;
	std::vector<stacked_item> dropped_items;
	bool combat_disabled{};
	bool magic_disabled{};
	bool theft_disabled{};
	bool chat_disabled{};

	size_t links{0}, words{0};
	std::string tag;
	bool last_was_link{false};
	bool display{true};
	long current_fragment{0};	
	bool auto_test{false}, didntmove{false};
	int g_dice{0};

	paragraph() = default;
	~paragraph() = default;
	paragraph(uint32_t paragraph_id, player& current, dpp::snowflake user_id);

	static bool valid_next(long Current, long Next);

private:
	void parse(player& current_player, dpp::snowflake user_id);
};

std::string extract_value(const std::string& p_text);

long extract_value_number(const std::string& p_text);

bool global_set(const std::string& flag);

void extract_to_quote(std::string& p_text, std::stringstream& content, char end = '>');

std::string remove_last_char(const std::string& s);
