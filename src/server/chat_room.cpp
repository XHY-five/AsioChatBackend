#include "server/chat_room.hpp"

#include "server/chat_session.hpp"

#include <algorithm>

namespace asiochat::server {
namespace {

constexpr const char* kLobbyRoom = "lobby";

}  // namespace

bool ChatRoom::login(const std::shared_ptr<ChatSession>& session,
                     const std::string& requested_name,
                     std::string& assigned_name,
                     std::string& assigned_room) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string candidate = requested_name.empty() ? "guest" : requested_name;
    const std::string base = candidate;
    int suffix = 1;

    while (name_index_.find(candidate) != name_index_.end()) {
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
    rooms.reserve(room_members_.size());
    for (const auto& [room, members] : room_members_) {
        if (!members.empty()) {
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
    if (room_it == room_members_.end()) {
        return users;
    }

    users.reserve(room_it->second.size());
    for (const ChatSession* member : room_it->second) {
        if (const auto name_it = names_.find(member); name_it != names_.end()) {
            users.push_back(name_it->second);
        }
    }
    std::sort(users.begin(), users.end());
    return users;
}

std::string ChatRoom::room_of(const ChatSession* session) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto room_it = session_rooms_.find(session);
    return room_it == session_rooms_.end() ? std::string{} : room_it->second;
}

}  // namespace asiochat::server
