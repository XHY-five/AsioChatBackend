#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace asiochat::protocol {

inline constexpr std::uint32_t kHeaderLength = 4;
inline constexpr std::uint32_t kMaxMessageLength = 64 * 1024;

enum class CommandType {
    login,
    join_room,
    message,
    private_message,
    list_users,
    list_rooms,
    heartbeat,
    quit,
    invalid,
};

struct Command {
    CommandType type{CommandType::invalid};
    std::string user;
    std::string room;
    std::string target;
    std::string message;
};

std::string trim(std::string_view text);
std::optional<Command> parse_client_line(std::string_view line);
std::string serialize_command(const Command& command);
std::string normalize_client_input(std::string_view line);
std::string format_server_line(std::string_view line);

std::string make_frame(std::string_view payload);
bool decode_header(const std::array<unsigned char, kHeaderLength>& header, std::uint32_t& body_length);

std::string make_system_event(std::string_view message);
std::string make_chat_event(std::string_view from, std::string_view room, std::string_view message);
std::string make_private_event(std::string_view from, std::string_view to, std::string_view message);
std::string make_users_event(std::string_view room, const std::vector<std::string>& users);
std::string make_rooms_event(const std::vector<std::string>& rooms);
std::string make_presence_event(std::string_view user, std::string_view room, std::string_view action, std::size_t online);
std::string make_pong_event();

}  // namespace asiochat::protocol
