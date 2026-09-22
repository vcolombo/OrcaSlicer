// Local control API: JSON-RPC 2.0 dispatcher (spec §2).
//
// GUI-independent: depends only on the standard library and the vendored
// nlohmann/json. The Beast transport adapter lives outside this seam.

#pragma once

#include <functional>
#include <map>
#include <string>

#include "nlohmann/json.hpp"

namespace Slic3r {
namespace LocalAPI {

using json = nlohmann::json;

// Reserved JSON-RPC 2.0 errors verbatim; application codes are positive,
// clear of the reserved -32768..-32000 range.
enum JsonRpcError : int {
    ErrorParse          = -32700,
    ErrorInvalidRequest = -32600,
    ErrorMethodNotFound = -32601,
    ErrorInvalidParams  = -32602,
    ErrorInternal       = -32603,
    ErrorUnauthorized   = 1200,
    ErrorLeaseHeld      = 2000,
    ErrorNotFound       = 2100,
    ErrorSliceFailed    = 3000,
    ErrorPrintFaulted   = 3100,
};

// Thrown by handlers for application-level failures.
struct MethodError {
    int code;
    std::string message;
    std::string stage; // empty when the error has no stage (non-slice failures)
    MethodError(int code_, std::string message_, std::string stage_ = {})
        : code(code_), message(std::move(message_)), stage(std::move(stage_)) {}
};

// Handler contract: params object (or null when omitted) in, result value
// out. Unknown params members are the handler's business (tolerant readers).
using Handler = std::function<json(const json &params)>;

struct Method {
    Handler handler;
    bool requires_auth = true;
};

class Dispatcher {
public:
    void add_method(std::string name, Handler handler, bool requires_auth = true);

    // Dispatches one JSON-RPC request. Returns "" for notifications
    // (requests without an "id" member), which never get a response.
    // Never throws.
    std::string dispatch(const std::string &request_json, bool authed) const;

private:
    std::map<std::string, Method> m_methods;
};

} // namespace LocalAPI
} // namespace Slic3r

