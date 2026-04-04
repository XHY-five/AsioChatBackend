#pragma once

#include "asiochat/server/mysql_config.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace asiochat::server {

class MySqlOfflineMessageDao;

struct OfflineMessage {
    std::string from;
    std::string to;
    std::string message;
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
    std::vector<OfflineMessage> load_all_message() const;
    void save_all_message(const std::vector<OfflineMessage>& messages) const;

    std::filesystem::path storage_path_;
};

class MySqlOfflineMessageStore : public OfflineMessageStore {
public:
    explicit MySqlOfflineMessageStore(const MySqlConfig& config);
    ~MySqlOfflineMessageStore() override;

    void save_private_message(const OfflineMessage& message) override;
    std::vector<OfflineMessage> take_private_messages_for(std::string_view user) override;

private:
    std::unique_ptr<MySqlOfflineMessageDao> dao_;
};

}  // namespace asiochat::server
