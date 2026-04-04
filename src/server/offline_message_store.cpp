#include "asiochat/server/offline_message_store.hpp"
#include "asiochat/server/mysql_offline_message_dao.hpp"

#include <fstream>
#include <memory>
#include <sstream>
#include <utility>

namespace asiochat::server
{
    FileOfflineMessageStore::FileOfflineMessageStore(std::filesystem::path sp)
        : storage_path_(sp) {}

    void FileOfflineMessageStore::save_private_message(const OfflineMessage &message)
    {
        auto messages = load_all_message();
        messages.push_back(message);
        save_all_message(messages);
    }

    std::vector<OfflineMessage> FileOfflineMessageStore::take_private_messages_for(std::string_view user)
    {
        auto messages = load_all_message();

        std::vector<OfflineMessage> matched;
        std::vector<OfflineMessage> remaining;

        for (const auto &message : messages)
        {
            if (message.to == user)
            {
                matched.push_back(message);
            }
            else
            {
                remaining.push_back(message);
            }
        }

        save_all_message(remaining);

        return matched;
    }

    void NullOfflineMessageStore::save_private_message(const OfflineMessage & /*message*/)
    {
    }

    std::vector<OfflineMessage> NullOfflineMessageStore::take_private_messages_for(std::string_view /*user*/)
    {
        return {};
    }

    std::vector<OfflineMessage> FileOfflineMessageStore::load_all_message() const
    {
        std::vector<OfflineMessage> messages;
        std::ifstream input(storage_path_);
        if (!input.is_open())
        {
            return messages;
        }

        std::string line;
        while (std::getline(input, line))
        {
            std::istringstream stream(line);

            OfflineMessage message;

            if (std::getline(stream, message.from, '|') &&
                std::getline(stream, message.to, '|') &&
                std::getline(stream, message.message))
            {
                messages.push_back(message);
            }
        }

        return messages;
    }

    void FileOfflineMessageStore::save_all_message(const std::vector<OfflineMessage> &messages) const
    {
        std::ofstream op(storage_path_, std::ios::trunc);

        for (const auto &message : messages)
        {
            op << message.from << '|'
               << message.to << '|'
               << message.message << '\n';
        }
    }

    MySqlOfflineMessageStore::MySqlOfflineMessageStore(const MySqlConfig &config)
        : dao_(std::make_unique<MySqlOfflineMessageDao>(config))
    {
        dao_->ensure_schema();
    }

    MySqlOfflineMessageStore::~MySqlOfflineMessageStore() = default;

    void MySqlOfflineMessageStore::save_private_message(const OfflineMessage &message)
    {
        dao_->save_private_message(message);
    }

    std::vector<OfflineMessage> MySqlOfflineMessageStore::take_private_messages_for(std::string_view user)
    {
        return dao_->take_private_messages_for(std::string(user));
    }

} // namespace asiochat::server
