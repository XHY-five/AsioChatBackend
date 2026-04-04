#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>

namespace asiochat::protocol
{
    constexpr std::size_t kHeaderLength = 4;
    constexpr std::uint32_t kMaxBodyLength = 8 * 1024;

    struct Message
    {
        std::string type;
        std::string user;
        std::string password;
        std::string room;
        std::string target;
        std::string message;
    };

    std::array<unsigned char, kHeaderLength> encode_header(std::uint32_t body_length);
    bool decode_header(const std::array<unsigned char, kHeaderLength> &header, std::uint32_t &body_length);

    std::optional<Message> parse_message(const std::string &json_text);

    std::string serialize_message(const Message &message);

    std::string make_frame(const std::string &payload);

}