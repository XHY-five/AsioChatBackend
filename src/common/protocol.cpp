#include "asiochat/common/protocol.hpp"

#include <sstream>
#include <vector>
#include <boost/json.hpp>

namespace asiochat::protocol
{

    std::array<unsigned char, kHeaderLength> encode_header(std::uint32_t body_length)
    {
        std::array<unsigned char, kHeaderLength> header{};

        header[0] = static_cast<unsigned char>((body_length >> 24) & 0xff);
        header[1] = static_cast<unsigned char>((body_length >> 16) & 0xff);
        header[2] = static_cast<unsigned char>((body_length >> 8) & 0xff);
        header[3] = static_cast<unsigned char>((body_length) & 0xff);

        return header;
    }

    bool decode_header(const std::array<unsigned char, kHeaderLength> &header, std::uint32_t &body_length)
    {
        body_length =
            (static_cast<std::uint32_t>(header[0] << 24)) |
            (static_cast<std::uint32_t>(header[1] << 16)) |
            (static_cast<std::uint32_t>(header[2] << 8)) |
            (static_cast<std::uint32_t>(header[3]));

        return body_length <= kMaxBodyLength;
    }

    std::optional<Message> parse_message(const std::string &json_text)
    {
        try
        {
            /* code */
            const auto value = boost::json::parse(json_text);
            if (!value.is_object())
            {
                return std::nullopt;
            }

            const auto &obj = value.as_object();

            Message msg;

            if (auto *type = obj.if_contains("type"); type && type->is_string())
            {
                msg.type = type->as_string().c_str();
            }
            else
            {
                return std::nullopt;
            }

            if (auto *user = obj.if_contains("user"); user && user->is_string())
            {
                msg.user = user->as_string().c_str();
            }
            if (auto *password = obj.if_contains("password"); password && password->is_string())
            {
                msg.password = password->as_string().c_str();
            }
            if (auto *room = obj.if_contains("room"); room && room->is_string())
            {
                msg.room = room->as_string().c_str();
            }
            if (auto *target = obj.if_contains("target"); target && target->is_string())
            {
                msg.target = target->as_string().c_str();
            }
            if (auto *message = obj.if_contains("message"); message && message->is_string())
            {
                msg.message = message->as_string().c_str();
            }

            return msg;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    std::string serialize_message(const Message &message)
    {
        boost::json::object obj;
        obj["type"] = message.type;

        if (!message.user.empty())
        {
            obj["user"] = message.user;
        }
        if (!message.password.empty())
        {
            obj["password"] = message.password;
        }
        if (!message.room.empty())
        {
            obj["room"] = message.room;
        }
        if (!message.target.empty())
        {
            obj["target"] = message.target;
        }
        if (!message.message.empty())
        {
            obj["message"] = message.message;
        }

        return boost::json::serialize(obj);
    }

    std::string make_frame(const std::string &payload)
    {
        if (payload.size() > kMaxBodyLength)
        {
            return {};
        }

        const auto header = encode_header(static_cast<std::uint32_t>(payload.size()));

        std::string frame;
        frame.reserve(kHeaderLength + payload.size());

        for (unsigned char byte : header)
        {
            frame.push_back(static_cast<char>(byte));
        }

        frame += payload;
        return frame;
    }

} // namespace asiochat::protocol
