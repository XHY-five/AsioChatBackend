#include "common/protocol.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>

namespace asiochat::protocol {
namespace {

using boost::json::array;
using boost::json::object;
using boost::json::parse;
using boost::json::serialize;
using boost::json::value;

std::string join_strings(const std::vector<std::string>& items) {
    std::ostringstream stream;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i != 0) {
            stream << ", ";
        }
        stream << items[i];
    }
    return stream.str();
}

std::string get_string(const object& source, std::string_view key) {
    if (const value* item = source.if_contains(key); item != nullptr && item->is_string()) {
        return std::string(item->as_string().c_str());
    }
    return {};
}

std::vector<std::string> get_string_array(const object& source, std::string_view key) {
    std::vector<std::string> result;
    if (const value* item = source.if_contains(key); item != nullptr && item->is_array()) {
        for (const value& entry : item->as_array()) {
            if (entry.is_string()) {
                result.emplace_back(entry.as_string().c_str());
            }
        }
    }
    return result;
}

std::optional<Command> parse_json_command(std::string_view line) {
    boost::system::error_code ec;
    value parsed = parse(line, ec);
    if (ec || !parsed.is_object()) {
        return Command{CommandType::invalid};
    }

    const object& obj = parsed.as_object();
    const std::string type = get_string(obj, "type");

    if (type == "login") {
        return Command{CommandType::login, get_string(obj, "user"), {}, {}, {}};
    }
    if (type == "join") {
        return Command{CommandType::join_room, {}, get_string(obj, "room"), {}, {}};
    }
    if (type == "message") {
        return Command{CommandType::message, {}, {}, {}, get_string(obj, "message")};
    }
    if (type == "private") {
        return Command{CommandType::private_message, {}, {}, get_string(obj, "target"), get_string(obj, "message")};
    }
    if (type == "list_users") {
        return Command{CommandType::list_users};
    }
    if (type == "list_rooms") {
        return Command{CommandType::list_rooms};
    }
    if (type == "ping") {
        return Command{CommandType::heartbeat};
    }
    if (type == "quit") {
        return Command{CommandType::quit};
    }

    return Command{CommandType::invalid};
}

std::optional<Command> parse_legacy_command(std::string_view line) {
    const std::string cleaned = trim(line);
    if (cleaned.empty()) {
        return std::nullopt;
    }

    if (cleaned.rfind("/login ", 0) == 0) {
        return Command{CommandType::login, trim(cleaned.substr(7)), {}, {}, {}};
    }
    if (cleaned.rfind("/join ", 0) == 0) {
        return Command{CommandType::join_room, {}, trim(cleaned.substr(6)), {}, {}};
    }
    if (cleaned.rfind("/msg ", 0) == 0) {
        return Command{CommandType::message, {}, {}, {}, trim(cleaned.substr(5))};
    }
    if (cleaned.rfind("/pm ", 0) == 0) {
        const std::string payload = trim(cleaned.substr(4));
        const std::size_t split = payload.find(' ');
        if (split == std::string::npos) {
            return Command{CommandType::invalid};
        }
        return Command{CommandType::private_message, {}, {}, trim(payload.substr(0, split)), trim(payload.substr(split + 1))};
    }
    if (cleaned == "/users") {
        return Command{CommandType::list_users};
    }
    if (cleaned == "/rooms") {
        return Command{CommandType::list_rooms};
    }
    if (cleaned == "/ping") {
        return Command{CommandType::heartbeat};
    }
    if (cleaned == "/quit") {
        return Command{CommandType::quit};
    }

    return Command{CommandType::message, {}, {}, {}, cleaned};
}

array to_json_array(const std::vector<std::string>& items) {
    array result;
    for (const auto& item : items) {
        result.emplace_back(item);
    }
    return result;
}

std::string serialize_object(const object& obj) {
    return serialize(obj);
}

}  // namespace

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();

    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }

    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return std::string(text.substr(begin, end - begin));
}

std::optional<Command> parse_client_line(std::string_view line) {
    const std::string cleaned = trim(line);
    if (cleaned.empty()) {
        return std::nullopt;
    }

    if (!cleaned.empty() && cleaned.front() == '{') {
        return parse_json_command(cleaned);
    }

    return parse_legacy_command(cleaned);
}

std::string serialize_command(const Command& command) {
    object obj;
    switch (command.type) {
    case CommandType::login:
        obj["type"] = "login";
        obj["user"] = command.user;
        break;
    case CommandType::join_room:
        obj["type"] = "join";
        obj["room"] = command.room;
        break;
    case CommandType::message:
        obj["type"] = "message";
        obj["message"] = command.message;
        break;
    case CommandType::private_message:
        obj["type"] = "private";
        obj["target"] = command.target;
        obj["message"] = command.message;
        break;
    case CommandType::list_users:
        obj["type"] = "list_users";
        break;
    case CommandType::list_rooms:
        obj["type"] = "list_rooms";
        break;
    case CommandType::heartbeat:
        obj["type"] = "ping";
        break;
    case CommandType::quit:
        obj["type"] = "quit";
        break;
    case CommandType::invalid:
        obj["type"] = "invalid";
        break;
    }
    return serialize_object(obj);
}

std::string normalize_client_input(std::string_view line) {
    const auto command = parse_client_line(line);
    if (!command.has_value()) {
        return {};
    }
    return serialize_command(*command);
}

std::string format_server_line(std::string_view line) {
    boost::system::error_code ec;
    value parsed = parse(line, ec);
    if (ec || !parsed.is_object()) {
        return std::string(line);
    }

    const object& obj = parsed.as_object();
    const std::string type = get_string(obj, "type");

    if (type == "system") {
        return "[system] " + get_string(obj, "message");
    }
    if (type == "chat") {
        return "[room:" + get_string(obj, "room") + "] [" + get_string(obj, "from") + "] " + get_string(obj, "message");
    }
    if (type == "private") {
        return "[private] [" + get_string(obj, "from") + " -> " + get_string(obj, "to") + "] " + get_string(obj, "message");
    }
    if (type == "users") {
        return "[users@" + get_string(obj, "room") + "] " + join_strings(get_string_array(obj, "users"));
    }
    if (type == "rooms") {
        return "[rooms] " + join_strings(get_string_array(obj, "rooms"));
    }
    if (type == "presence") {
        return "[presence] " + get_string(obj, "user") + " " + get_string(obj, "action") + " room " + get_string(obj, "room") +
               " (online: " + get_string(obj, "online") + ")";
    }
    if (type == "pong") {
        return "[pong] heartbeat ack";
    }
    if (type == "error") {
        return "[error] " + get_string(obj, "message");
    }

    return std::string(line);
}

std::string make_frame(std::string_view payload) {
    const auto length = static_cast<std::uint32_t>(payload.size());
    std::string frame(kHeaderLength, '\0');
    frame[0] = static_cast<char>((length >> 24) & 0xFF);
    frame[1] = static_cast<char>((length >> 16) & 0xFF);
    frame[2] = static_cast<char>((length >> 8) & 0xFF);
    frame[3] = static_cast<char>(length & 0xFF);
    frame.append(payload.data(), payload.size());
    return frame;
}

bool decode_header(const std::array<unsigned char, kHeaderLength>& header, std::uint32_t& body_length) {
    body_length = (static_cast<std::uint32_t>(header[0]) << 24) |
                  (static_cast<std::uint32_t>(header[1]) << 16) |
                  (static_cast<std::uint32_t>(header[2]) << 8) |
                  static_cast<std::uint32_t>(header[3]);
    return body_length <= kMaxMessageLength;
}

std::string make_system_event(std::string_view message) {
    object obj;
    obj["type"] = "system";
    obj["message"] = std::string(message);
    return serialize_object(obj);
}

std::string make_chat_event(std::string_view from, std::string_view room, std::string_view message) {
    object obj;
    obj["type"] = "chat";
    obj["from"] = std::string(from);
    obj["room"] = std::string(room);
    obj["message"] = std::string(message);
    return serialize_object(obj);
}

std::string make_private_event(std::string_view from, std::string_view to, std::string_view message) {
    object obj;
    obj["type"] = "private";
    obj["from"] = std::string(from);
    obj["to"] = std::string(to);
    obj["message"] = std::string(message);
    return serialize_object(obj);
}

std::string make_users_event(std::string_view room, const std::vector<std::string>& users) {
    object obj;
    obj["type"] = "users";
    obj["room"] = std::string(room);
    obj["users"] = to_json_array(users);
    return serialize_object(obj);
}

std::string make_rooms_event(const std::vector<std::string>& rooms) {
    object obj;
    obj["type"] = "rooms";
    obj["rooms"] = to_json_array(rooms);
    return serialize_object(obj);
}

std::string make_presence_event(std::string_view user, std::string_view room, std::string_view action, std::size_t online) {
    object obj;
    obj["type"] = "presence";
    obj["user"] = std::string(user);
    obj["room"] = std::string(room);
    obj["action"] = std::string(action);
    obj["online"] = std::to_string(online);
    return serialize_object(obj);
}

std::string make_pong_event() {
    object obj;
    obj["type"] = "pong";
    return serialize_object(obj);
}

}  // namespace asiochat::protocol
