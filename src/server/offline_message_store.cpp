#include "server/offline_message_store.hpp"

#include <boost/json.hpp>

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace asiochat::server {
namespace {

using boost::json::array;
using boost::json::object;
using boost::json::parse;
using boost::json::serialize;
using boost::json::value;

OfflineMessage from_json(const object& item) {
    OfflineMessage message;
    if (const value* field = item.if_contains("from"); field != nullptr && field->is_string()) {
        message.from = field->as_string().c_str();
    }
    if (const value* field = item.if_contains("to"); field != nullptr && field->is_string()) {
        message.to = field->as_string().c_str();
    }
    if (const value* field = item.if_contains("message"); field != nullptr && field->is_string()) {
        message.message = field->as_string().c_str();
    }
    return message;
}

object to_json(const OfflineMessage& message) {
    object item;
    item["from"] = message.from;
    item["to"] = message.to;
    item["message"] = message.message;
    return item;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::vector<std::string> split_tabs(const std::string& text) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : text) {
        if (ch == '\t') {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

}  // namespace

void NullOfflineMessageStore::save_private_message(const OfflineMessage& /*message*/) {}

std::vector<OfflineMessage> NullOfflineMessageStore::take_private_messages_for(std::string_view /*user*/) {
    return {};
}

FileOfflineMessageStore::FileOfflineMessageStore(std::filesystem::path storage_path)
    : storage_path_(std::move(storage_path)) {
    if (const auto parent = storage_path_.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    if (!std::filesystem::exists(storage_path_)) {
        save_all_messages({});
    }
}

void FileOfflineMessageStore::save_private_message(const OfflineMessage& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto messages = load_all_messages();
    messages.push_back(message);
    save_all_messages(messages);
}

std::vector<OfflineMessage> FileOfflineMessageStore::take_private_messages_for(std::string_view user) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto messages = load_all_messages();
    std::vector<OfflineMessage> delivered;
    std::vector<OfflineMessage> remaining;
    delivered.reserve(messages.size());
    remaining.reserve(messages.size());

    for (const auto& message : messages) {
        if (message.to == user) {
            delivered.push_back(message);
        } else {
            remaining.push_back(message);
        }
    }

    save_all_messages(remaining);
    return delivered;
}

std::vector<OfflineMessage> FileOfflineMessageStore::load_all_messages() const {
    std::ifstream input(storage_path_, std::ios::binary);
    if (!input) {
        return {};
    }

    const std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (content.empty()) {
        return {};
    }

    boost::system::error_code ec;
    value parsed = parse(content, ec);
    if (ec || !parsed.is_array()) {
        return {};
    }

    std::vector<OfflineMessage> messages;
    for (const value& item : parsed.as_array()) {
        if (item.is_object()) {
            messages.push_back(from_json(item.as_object()));
        }
    }
    return messages;
}

void FileOfflineMessageStore::save_all_messages(const std::vector<OfflineMessage>& messages) const {
    array items;
    for (const auto& message : messages) {
        items.push_back(to_json(message));
    }

    std::ofstream output(storage_path_, std::ios::binary | std::ios::trunc);
    output << serialize(items);
}

MySqlOfflineMessageStore::MySqlOfflineMessageStore(MySqlConfig config)
    : config_(std::move(config)) {
    ensure_schema();
}

void MySqlOfflineMessageStore::save_private_message(const OfflineMessage& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string sql =
        "INSERT INTO offline_messages(sender, recipient, content) VALUES ("
        "UNHEX('" + to_hex(message.from) + "'),"
        "UNHEX('" + to_hex(message.to) + "'),"
        "UNHEX('" + to_hex(message.message) + "'))";
    run_mysql(sql, true);
}

std::vector<OfflineMessage> MySqlOfflineMessageStore::take_private_messages_for(std::string_view user) {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string user_hex = to_hex(user);
    const std::string query =
        "SELECT HEX(sender), HEX(recipient), HEX(content) FROM offline_messages "
        "WHERE recipient = UNHEX('" + user_hex + "') ORDER BY id ASC";
    const std::string output = run_mysql(query, true);

    std::vector<OfflineMessage> messages;
    for (const auto& line : split_lines(output)) {
        const auto fields = split_tabs(line);
        if (fields.size() < 3) {
            continue;
        }
        messages.push_back(OfflineMessage{
            from_hex(fields[0]),
            from_hex(fields[1]),
            from_hex(fields[2])});
    }

    const std::string remove =
        "DELETE FROM offline_messages WHERE recipient = UNHEX('" + user_hex + "')";
    run_mysql(remove, true);
    return messages;
}

std::string MySqlOfflineMessageStore::run_mysql(std::string_view sql, bool select_database) const {
    const auto temp_file = std::filesystem::temp_directory_path() / "asiochat_mysql_tmp.sql";
    {
        std::ofstream sql_file(temp_file, std::ios::binary | std::ios::trunc);
        sql_file << sql << ';';
    }

    std::ostringstream command;
    command << "cmd /c \"\"" << config_.mysql_executable << "\""
            << " -N -B"
            << " -h " << config_.host
            << " -P " << config_.port
            << " -u " << config_.user
            << " -p" << config_.password;

    if (select_database) {
        command << ' ' << config_.database;
    }

    command << " < \"" << temp_file.string() << "\" 2>&1\"";

    FILE* pipe = _popen(command.str().c_str(), "r");
    if (pipe == nullptr) {
        std::filesystem::remove(temp_file);
        throw std::runtime_error("Failed to start mysql client process.");
    }

    std::string output;
    char buffer[256];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    std::filesystem::remove(temp_file);

    const int exit_code = _pclose(pipe);
    if (exit_code != 0) {
        throw std::runtime_error("mysql client command failed: " + output);
    }
    return output;
}
std::string MySqlOfflineMessageStore::to_hex(std::string_view input) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (unsigned char ch : input) {
        stream << std::setw(2) << static_cast<int>(ch);
    }
    return stream.str();
}

std::string MySqlOfflineMessageStore::from_hex(std::string_view input) {
    std::string result;
    result.reserve(input.size() / 2);
    for (std::size_t i = 0; i + 1 < input.size(); i += 2) {
        const std::string byte_text(input.substr(i, 2));
        const char value = static_cast<char>(std::stoi(byte_text, nullptr, 16));
        result.push_back(value);
    }
    return result;
}

void MySqlOfflineMessageStore::ensure_schema() {
    std::lock_guard<std::mutex> lock(mutex_);
    run_mysql(
        "CREATE DATABASE IF NOT EXISTS " + config_.database +
        " CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        false);
    run_mysql(
        "CREATE TABLE IF NOT EXISTS offline_messages ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "sender VARCHAR(64) NOT NULL,"
        "recipient VARCHAR(64) NOT NULL,"
        "content TEXT NOT NULL,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ") CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        true);
}

}  // namespace asiochat::server







