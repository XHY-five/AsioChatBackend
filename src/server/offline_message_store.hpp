#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace asiochat::server {

struct OfflineMessage {
    std::string from;
    std::string to;
    std::string message;
};

struct MySqlConfig {
    std::string host{"127.0.0.1"};
    unsigned int port{3306};
    std::string user{"root"};
    std::string password;
    std::string database{"asiochat"};
    std::string mysql_executable{"mysql"};
};

class OfflineMessageStore {
public:
    virtual ~OfflineMessageStore() = default;

    virtual void save_private_message(const OfflineMessage& message) = 0;
    virtual std::vector<OfflineMessage> take_private_messages_for(std::string_view user) = 0;
};

class NullOfflineMessageStore : public OfflineMessageStore {
public:
    void save_private_message(const OfflineMessage& message) override;
    std::vector<OfflineMessage> take_private_messages_for(std::string_view user) override;
};

class FileOfflineMessageStore : public OfflineMessageStore {
public:
    explicit FileOfflineMessageStore(std::filesystem::path storage_path);

    void save_private_message(const OfflineMessage& message) override;
    std::vector<OfflineMessage> take_private_messages_for(std::string_view user) override;

private:
    std::vector<OfflineMessage> load_all_messages() const;
    void save_all_messages(const std::vector<OfflineMessage>& messages) const;

    std::filesystem::path storage_path_;
    mutable std::mutex mutex_;
};

class MySqlOfflineMessageStore : public OfflineMessageStore {
public:
    explicit MySqlOfflineMessageStore(MySqlConfig config);

    void save_private_message(const OfflineMessage& message) override;
    std::vector<OfflineMessage> take_private_messages_for(std::string_view user) override;

private:
    std::string run_mysql(std::string_view sql, bool select_database) const;
    static std::string to_hex(std::string_view input);
    static std::string from_hex(std::string_view input);
    void ensure_schema();

    MySqlConfig config_;
    mutable std::mutex mutex_;
};

}  // namespace asiochat::server
