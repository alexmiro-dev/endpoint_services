
#pragma once

#include <crow.h>
#include <magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <semver.hpp>

#include <format>
#include <optional>
#include <unordered_map>

namespace nt::proto {

//--------------------------------------------------------------------------------
// Type definitions
//--------------------------------------------------------------------------------
enum class MessageType : uint32_t {
    /* All directions */
    Uninitialized,
    NotSupported,
    Accepted,

    /* Request (Client -> Server) */
    Version,
    GetUpdates,
    PushSettings,

    /* Response (Server -> Client) */
    BadRequest,
    VersionUpdatesAvailable,
    Updates,
    Deprecated
};

NLOHMANN_JSON_SERIALIZE_ENUM(MessageType,
                             {
                                 {MessageType::Uninitialized, "Uninitialized"},
                                 {MessageType::BadRequest, "BadRequest"},
                                 {MessageType::NotSupported, "NotSupported"},
                                 {MessageType::Accepted, "Accepted"},
                                 {MessageType::Version, "Version"},
                                 {MessageType::GetUpdates, "GetUpdates"},
                                 {MessageType::VersionUpdatesAvailable, "VersionUpdatesAvailable"},
                                 {MessageType::PushSettings, "PushSettings"},
                                 {MessageType::Updates, "Updates"},
                                 {MessageType::Deprecated, "Deprecated"},
                             })

namespace keys {
static constexpr std::string_view kRequest = "request";
static constexpr std::string_view kType = "type";
static constexpr std::string_view kPayload = "payload";
static constexpr std::string_view kVersion = "version";
static constexpr std::string_view kMetrics = "metrics";
static constexpr std::string kAvailability = "availability";
static constexpr std::string kPerformance = "performance";
} // namespace keys

enum class MetricType : uint8_t { Integer, Double, String };

NLOHMANN_JSON_SERIALIZE_ENUM(MetricType, {{MetricType::Integer, "Integer"},
                                          {MetricType::Double, "Double"},
                                          {MetricType::String, "String"}})

struct Metric {
    std::string name;
    std::string description;
    MetricType type;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Metric, name, description, type)
};

using metrics_umap_t = std::unordered_map<std::string, Metric>;

metrics_umap_t const kMetricsDefault = {
    {keys::kAvailability, {.name = keys::kAvailability, .description = "The uptime", .type = MetricType::Double}},
    {keys::kPerformance, {.name = keys::kPerformance, .description = "The performance", .type = MetricType::Double}}
};

//--------------------------------------------------------------------------------
// Message definitions
//--------------------------------------------------------------------------------

struct Version {
    semver::version value;
};

struct Message {
    MessageType type = MessageType::Uninitialized;
    nlohmann::json payload;
};

inline Message toMessage(const nlohmann::json &json) {
    Message m;
    if (json.contains(keys::kType)) {
        json.at(keys::kType).get_to(m.type);
    }
    if (json.contains(keys::kPayload)) {
        json.at(keys::kPayload).get_to(m.payload);
    }
    return m;
}

inline std::string toString(Message const &m) {
    return std::format(R"({{"type": "{}", "payload": {}}})",
                       std::string{magic_enum::enum_name(m.type)}, m.payload.dump());
}

//--------------------------------------------------------------------------------
//  Handlers
//--------------------------------------------------------------------------------

using handle_func_t = std::function<std::optional<Message>(Message &&)>;

/**
 *
 */
class MessageHandler {
public:
    using self_t = MessageHandler;

    self_t &onBadRequest(handle_func_t f) {
        handlers_.insert(std::make_pair(MessageType::BadRequest, f));
        return *this;
    }

    self_t &onNotSupported(handle_func_t f) {
        handlers_.insert(std::make_pair(MessageType::NotSupported, f));
        return *this;
    }

    self_t &onAccepted(handle_func_t f) {
        handlers_.insert(std::make_pair(MessageType::Accepted, f));
        return *this;
    }

    self_t &onVersion(handle_func_t f) {
        handlers_.insert(std::make_pair(MessageType::Version, f));
        return *this;
    }

    self_t &onGetUpdates(handle_func_t f) {
        handlers_.insert(std::make_pair(MessageType::GetUpdates, f));
        return *this;
    }

    self_t &onVersionUpdatesAvailable(handle_func_t f) {
        handlers_.insert(std::make_pair(MessageType::VersionUpdatesAvailable, f));
        return *this;
    }

    self_t &onUpdates(handle_func_t f) {
        handlers_.insert(std::make_pair(MessageType::Updates, f));
        return *this;
    }

    self_t &onPushSettings(handle_func_t f) {
        handlers_.insert(std::make_pair(MessageType::PushSettings, f));
        return *this;
    }

    [[nodiscard]] std::optional<Message> process(Message &&message) const {
        Message response;

        if (!handlers_.contains(message.type)) {
            response.type = MessageType::NotSupported;
            response.payload = message.payload;
            return response;
        }
        auto runHandler = handlers_.at(message.type);
        return runHandler(std::move(message));
    }

    std::unordered_map<MessageType, handle_func_t> handlers_;
};

} // namespace nt::proto
