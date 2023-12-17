#include <dpp/dpp.h>
#include <ssod/ssod.h>
#include <ssod/paragraph.h>
#include <ssod/database.h>
#include <ssod/game_player.h>
#include <ssod/game_dice.h>
#include <ssod/game_util.h>
#include <ssod/parser.h>
#include <fmt/format.h>

paragraph::paragraph(uint32_t paragraph_id, player& current, dpp::snowflake user_id) {
	auto location = db::query("SELECT * FROM game_locations WHERE id = ?", {paragraph_id});
	if (location.empty()) {
		throw dpp::logic_exception("Invalid location, internal error");
	}
	id = paragraph_id;
	text = location[0].at("data");
	combat_disabled = location[0].at("combat_disabled") == "1";
	magic_disabled = location[0].at("magic_disabled") == "1";
	theft_disabled = location[0].at("theft_disabled") == "1";
	chat_disabled = location[0].at("chat_disabled") == "1";
	auto dropped = db::query("SELECT * FROM game_dropped_items WHERE location_id = ?", {paragraph_id});
	for (const auto& dropped_item : dropped) {
		dropped_items.push_back(item{ .name = dropped_item.at("item_desc"), .flags = dropped_item.at("item_flags") });
	}
	parse(current, user_id);
	// TODO: Nav links
}

// extracts from any NAME=Value pair
std::string extract_without_quotes(const std::string& p_text) {
	std::string item_name;
	bool copying{false};
	for (const char c : p_text) {
		if (c == '=' || c == ' ' || c == '>') {
			copying = !copying;
			continue;
		}
		if (copying) {
			item_name += c;
		}
	}
	return item_name;
}

// extracts a value from any NAME="Value" pair
std::string extract_value(const std::string& p_text) {
	if (p_text.find("\"") == std::string::npos) {
		return extract_without_quotes(p_text);
	}
	std::string item_name;
	bool copying{false};
	for (const char c : p_text) {
		if (c == '"') {
			copying = !copying;
			continue;
		}
		if (copying) {
			item_name += c;
		}
	}
	return item_name;
}

long extract_value_number(const std::string& p_text)
{
	return atol(extract_value(p_text));
}

bool global_set(const std::string& flag) {
	return db::query("SELECT flag FROM game_global_flags WHERE flag = ?", {flag}).size() > 0;
}

// returns true if the user hasnt found an item before
bool not_got_yet(uint32_t paragraph, const std::string& item, const std::string& gotfrom) {
	std::string f{" [" + item + std::to_string(paragraph) + "]"};
	return gotfrom.find(f) == std::string::npos;
}

void extract_to_quote(std::string& p_text, std::stringstream& content, char end) {
	while (!content.eof() && p_text.length() && *p_text.rbegin() != end) {
		std::string extra;
		content >> extra;
		p_text += " " + extra;
	}							
}

std::string remove_last_char(const std::string& s) {
	return s.substr(0, s.length() - 1);	
}

void paragraph::parse(player& current_player, dpp::snowflake user_id) {
	std::stringstream paragraph_content(text);
	std::stringstream output;
	std::string p_text, LastLink;

	while (!paragraph_content.eof()) {
		paragraph_content >> p_text;
		std::string neat_version{p_text};

		if (paragraph_content.eof()) {
			break;
		}	
		if (dpp::lowercase(p_text) == "<combat") {
			// combat tag
			links++;
			paragraph_content >> p_text;
			extract_to_quote(p_text, paragraph_content, '"');
			std::string monster_name = extract_value(p_text);
			paragraph_content >> p_text;
			long monster_skill = extract_value_number(p_text);
			paragraph_content >> p_text;
			long monster_stamina = extract_value_number(p_text);
			paragraph_content >> p_text;
			long monster_armour = extract_value_number(p_text);
			paragraph_content >> p_text;
			long monster_weapon = extract_value_number(p_text);
			if (current_fragment == after_fragment) {
				// when combat link is finished it goes back to the
				// paragraph it came from, but the next fragment of it.
				// fragments can only be requested on a paragraph
				// that contains at least one combat.
				
				output << fmt::format(
					"\n```\n ⚔ {0:26s} STM:{1:2d} SKL:{2:2d} ARM:{3:2d} WPN:{4:2d}\n```\n",
					monster_name.substr(0, 26),
					monster_stamina,
					monster_skill,
					monster_armour,
					monster_weapon
					);
				words++;
				navigation_links.push_back(nav_link{
					.paragraph = this->id,
					.type = nav_type_combat,
					.cost = 0,
					.monster = { 
						.name = monster_name,
						.stamina = monster_stamina,
						.skill = monster_skill,
						.armour = monster_armour,
						.weapon = monster_weapon,
					},
					.buyable = {}
				});
				break;
			}
			current_fragment++;
			continue;
		}
			
		if (current_fragment < after_fragment) {
			// nothing should be displayed that comes before the desired fragment!
			continue;
		}

		if (route_tag(*this, p_text, paragraph_content, output, current_player, display)) {
			continue;
		}
		
		if (display) {
			std::string tag = dpp::lowercase(p_text.substr(0, 20));
			if (tag.find("<paylink=") != std::string::npos && !last_was_link) {
				int i{0};
				std::string pnum, cost;
				while (p_text[i++] != '=');
				while (p_text[i] != ',') {
					cost += p_text[i++];
				}
				i++;
				while (p_text[i] != '>') {
					pnum += p_text[i++];
				}
				links++;
				output << directions[links];
				if (current_player.gold < atol(cost)) {
					//Image("link_bad.jpg","You dont have enough gold to do this!");
					navigation_links.push_back(nav_link{ .paragraph = atol(pnum), .type = nav_type_disabled_link, .cost = 0, .monster = {}, .buyable = {} });
				} else {
					navigation_links.push_back(nav_link{ .paragraph = atol(pnum), .type = nav_type_paylink, .cost = atol(cost), .monster = {}, .buyable = {} });
				}
				output << " ";
			} else if (tag.find("<link=") != std::string::npos && !last_was_link) {
				int i{0};
				std::string pnum;
				while (p_text[i++] != '=');
				while (p_text[i] != '>') {
					pnum += p_text[i++];
				}
				if (current_player.stamina < 1 || dpp::lowercase(pnum) == "the") {
					output << " (unable to follow this path)";
				} else {
					links++;
					LastLink = pnum;
					output << directions[links];
					navigation_links.push_back(nav_link{ .paragraph = atol(pnum), .type = nav_type_link, .cost = 0, .monster = {}, .buyable = {} });
				}
				output << " ";
			} else if (tag.find("<autolink=") != std::string::npos && !last_was_link) {
				int i{0};
				std::string pnum;
				while (p_text[i++] != '=');
				while (p_text[i] != '>') {
					pnum += p_text[i++];
				}
				/* TODO: What is this 'the' madness? Find out! Ancient fix? */
				if (current_player.stamina < 1 || dpp::lowercase(pnum) == "the") {
					output << " (unable to follow this path)";
				} else {
					links++;
					LastLink = pnum;
					output << directions[links];
					if (auto_test) {
						navigation_links.push_back(nav_link{ .paragraph = atol(pnum), .type = nav_type_autolink, .cost = 0, .monster = {}, .buyable = {} });
					} else {
						navigation_links.push_back(nav_link{ .paragraph = atol(pnum), .type = nav_type_disabled_link, .cost = 0, .monster = {}, .buyable = {} });
					}
				}
				auto_test = !auto_test; // invert the next autolink...
				output << " ";
			}
			else {
				if (neat_version.find(">") == std::string::npos && !neat_version.empty() && neat_version[0] != '<') {
					output << neat_version << " ";
					words++;
				}
			}
		}

	}
	text = output.str();
}