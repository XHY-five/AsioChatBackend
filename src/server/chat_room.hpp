#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace asiochat::server {

class ChatSession;

class ChatRoom {
public:
    bool login(const std::shared_ptr<ChatSession>& session,
               const std::string& requested_name,
               std::string& assigned_name,
               std::string& assigned_room);
    bool change_room(const ChatSession* session,
                     const std::string& requested_room,
                     std::string& previous_room,
                     std::string& assigned_room);
    void leave(const ChatSession* session, std::string& departed_name, std::string& departed_room);
    void broadcast_to_room(std::string_view room, std::string_view message, const ChatSession* exclude = nullptr);
    bool deliver_private(std::string_view target_name, std::string_view message);
    std::size_t online_count(std::string_view room) const;
    std::vector<std::string> room_names() const;
    std::vector<std::string> room_users(std::string_view room) const;
    std::string room_of(const ChatSession* session) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<const ChatSession*, std::shared_ptr<ChatSession>> sessions_;
    std::unordered_map<const ChatSession*, std::string> names_;
    std::unordered_map<const ChatSession*, std::string> session_rooms_;
    std::unordered_map<std::string, const ChatSession*> name_index_;
    std::unordered_map<std::string, std::unordered_set<const ChatSession*>> room_members_;
};

}  // namespace asiochat::server
