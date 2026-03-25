#include "server/chat_room.hpp"

#include "server/chat_session.hpp"

#include <algorithm>

namespace asiochat::server {
namespace {

constexpr const char* kLobbyRoom = "lobby";

}  // namespace

ChatRoom::ChatRoom() = default;

ChatRoom::ChatRoom(const AppConfig& config) {
    configure_ai_agents(config);
}

void ChatRoom::configure_ai_agents(const AppConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_room_ai_config_ = config.default_room_ai_agent;
    room_ai_bot_names_.clear();
    room_ai_welcome_messages_.clear();

    for (const auto& [room_name, room_config] : config.room_ai_agents) {
        if (!room_config.enabled) {
            continue;
        }
        room_ai_bot_names_[room_name] = room_config.bot_name;
        if (!room_config.welcome_message.empty()) {
            room_ai_welcome_messages_[room_name] = room_config.welcome_message;
        }
    }
}

bool ChatRoom::login(const std::shared_ptr<ChatSession>& session,
                     const std::string& requested_name,
                     std::string& assigned_name,
                     std::string& assigned_room) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string candidate = requested_name.empty() ? "guest" : requested_name;
    const std::string base = candidate;
    int suffix = 1;
    const auto reserved_bot_names = configured_ai_bot_names_();

    while (name_index_.find(candidate) != name_index_.end() || reserved_bot_names.find(candidate) != reserved_bot_names.end()) {
        candidate = base + std::to_string(++suffix);
    }

    assigned_name = candidate;
    assigned_room = kLobbyRoom;

    sessions_[session.get()] = session;
    names_[session.get()] = assigned_name;
    session_rooms_[session.get()] = assigned_room;
    name_index_[assigned_name] = session.get();
    room_members_[assigned_room].insert(session.get());
    return assigned_name == requested_name;
}

bool ChatRoom::change_room(const ChatSession* session,
                           const std::string& requested_room,
                           std::string& previous_room,
                           std::string& assigned_room) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto current = session_rooms_.find(session);
    if (current == session_rooms_.end()) {
        return false;
    }

    assigned_room = requested_room.empty() ? kLobbyRoom : requested_room;
    previous_room = current->second;
    if (previous_room == assigned_room) {
        return true;
    }

    if (auto room_it = room_members_.find(previous_room); room_it != room_members_.end()) {
        room_it->second.erase(session);
        if (room_it->second.empty()) {
            room_members_.erase(room_it);
        }
    }

    current->second = assigned_room;
    room_members_[assigned_room].insert(session);
    return true;
}

void ChatRoom::leave(const ChatSession* session, std::string& departed_name, std::string& departed_room) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (const auto name_it = names_.find(session); name_it != names_.end()) {
        departed_name = name_it->second;
        name_index_.erase(name_it->second);
        names_.erase(name_it);
    }

    if (const auto room_it = session_rooms_.find(session); room_it != session_rooms_.end()) {
        departed_room = room_it->second;
        if (auto members = room_members_.find(departed_room); members != room_members_.end()) {
            members->second.erase(session);
            if (members->second.empty()) {
                room_members_.erase(members);
            }
        }
        session_rooms_.erase(room_it);
    }

    sessions_.erase(session);
}

void ChatRoom::broadcast_to_room(std::string_view room, std::string_view message, const ChatSession* exclude) {
    std::vector<std::shared_ptr<ChatSession>> recipients;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto room_it = room_members_.find(std::string(room));
        if (room_it == room_members_.end()) {
            return;
        }

        recipients.reserve(room_it->second.size());
        for (const ChatSession* member : room_it->second) {
            if (member == exclude) {
                continue;
            }
            if (const auto session_it = sessions_.find(member); session_it != sessions_.end()) {
                recipients.push_back(session_it->second);
            }
        }
    }

    for (const auto& session : recipients) {
        session->deliver(std::string(message));
    }
}

bool ChatRoom::deliver_private(std::string_view target_name, std::string_view message) {
    std::shared_ptr<ChatSession> recipient;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto target_it = name_index_.find(std::string(target_name));
        if (target_it == name_index_.end()) {
            return false;
        }

        const auto session_it = sessions_.find(target_it->second);
        if (session_it == sessions_.end()) {
            return false;
        }
        recipient = session_it->second;
    }

    recipient->deliver(std::string(message));
    return true;
}

std::size_t ChatRoom::online_count(std::string_view room) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto room_it = room_members_.find(std::string(room));
    return room_it == room_members_.end() ? 0 : room_it->second.size();
}

std::vector<std::string> ChatRoom::room_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> rooms;
    rooms.reserve(room_members_.size() + room_ai_bot_names_.size());
    for (const auto& [room, members] : room_members_) {
        if (!members.empty()) {
            rooms.push_back(room);
        }
    }
    for (const auto& [room, bot_name] : room_ai_bot_names_) {
        (void)bot_name;
        if (std::find(rooms.begin(), rooms.end(), room) == rooms.end()) {
            rooms.push_back(room);
        }
    }
    std::sort(rooms.begin(), rooms.end());
    return rooms;
}

std::vector<std::string> ChatRoom::room_users(std::string_view room) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> users;
    const auto room_it = room_members_.find(std::string(room));
    if (room_it != room_members_.end()) {
        users.reserve(room_it->second.size() + 1);
        for (const ChatSession* member : room_it->second) {
            if (const auto name_it = names_.find(member); name_it != names_.end()) {
                users.push_back(name_it->second);
            }
        }
    }

    if (const auto bot_name = ai_agent_name_for_room_(room); bot_name.has_value()) {
        users.push_back(*bot_name);
    }

    std::sort(users.begin(), users.end());
    return users;
}

std::string ChatRoom::room_of(const ChatSession* session) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto room_it = session_rooms_.find(session);
    return room_it == session_rooms_.end() ? std::string{} : room_it->second;
}

bool ChatRoom::is_ai_agent_room_enabled(std::string_view room) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return is_ai_enabled_for_room_(room);
}

std::optional<std::string> ChatRoom::ai_agent_name_for_room(std::string_view room) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ai_agent_name_for_room_(room);
}

std::optional<std::string> ChatRoom::ai_agent_welcome_for_room(std::string_view room) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ai_agent_welcome_for_room_(room);
}

bool ChatRoom::is_ai_enabled_for_room_(std::string_view room) const {
    if (room_ai_bot_names_.find(std::string(room)) != room_ai_bot_names_.end()) {
        return true;
    }
    return default_room_ai_config_.has_value() && default_room_ai_config_->enabled;
}

std::optional<std::string> ChatRoom::ai_agent_name_for_room_(std::string_view room) const {
    if (const auto it = room_ai_bot_names_.find(std::string(room)); it != room_ai_bot_names_.end()) {
        return it->second;
    }
    if (default_room_ai_config_.has_value() && default_room_ai_config_->enabled && !default_room_ai_config_->bot_name.empty()) {
        return default_room_ai_config_->bot_name;
    }
    return std::nullopt;
}

std::optional<std::string> ChatRoom::ai_agent_welcome_for_room_(std::string_view room) const {
    if (const auto it = room_ai_welcome_messages_.find(std::string(room)); it != room_ai_welcome_messages_.end()) {
        return it->second;
    }
    if (default_room_ai_config_.has_value() && default_room_ai_config_->enabled && !default_room_ai_config_->welcome_message.empty()) {
        return default_room_ai_config_->welcome_message;
    }
    return std::nullopt;
}

std::unordered_set<std::string> ChatRoom::configured_ai_bot_names_() const {
    std::unordered_set<std::string> names;
    names.reserve(room_ai_bot_names_.size() + (default_room_ai_config_.has_value() ? 1u : 0u));
    for (const auto& [room_name, bot_name] : room_ai_bot_names_) {
        (void)room_name;
        names.insert(bot_name);
    }
    if (default_room_ai_config_.has_value() && default_room_ai_config_->enabled && !default_room_ai_config_->bot_name.empty()) {
        names.insert(default_room_ai_config_->bot_name);
    }
    return names;
}

}  // namespace asiochat::server
