#include "Dispatcher.hpp"

namespace Slic3r {
namespace LocalAPI {

void Dispatcher::add_method(std::string name, Handler handler, bool requires_auth)
{
    m_methods.emplace(std::move(name), Method{std::move(handler), requires_auth});
}

namespace {

// Every response goes through here; notifications never get one.
std::string respond(json body, const json &id, bool notification)
{
    if (notification)
        return "";
    body["jsonrpc"] = "2.0";
    body["id"] = id;
    return body.dump(-1, ' ', false, json::error_handler_t::replace);
}

std::string fail(int code, std::string message, const json &id, bool notification)
{
    return respond({{"error", {{"code", code}, {"message", std::move(message)}}}}, id, notification);
}

// The request id echoed verbatim when usable, null otherwise.
json usable_id(const json &request)
{
    auto it = request.find("id");
    if (it == request.end() || !(it->is_string() || it->is_number() || it->is_null()))
        return json(nullptr);
    return *it;
}

} // namespace

std::string Dispatcher::dispatch(const std::string &request_json, bool authed) const
{
    json request;
    try {
        request = json::parse(request_json);
    } catch (const json::exception &) {
        return fail(ErrorParse, "parse error", json(nullptr), false);
    }
    if (!request.is_object())
        // v1 serves single calls only; batch arrays are invalid requests.
        return fail(ErrorInvalidRequest, "invalid request", json(nullptr), false);
    const auto version_it = request.find("jsonrpc");
    if (version_it == request.end() || !version_it->is_string() || *version_it != "2.0")
        return fail(ErrorInvalidRequest, "invalid request", usable_id(request), false);

    const auto id_it = request.find("id");
    const bool notification = id_it == request.end();
    if (!notification && !(id_it->is_string() || id_it->is_number() || id_it->is_null()))
        return fail(ErrorInvalidRequest, "invalid request", json(nullptr), false);
    auto method_it = request.find("method");
    if (method_it == request.end() || !method_it->is_string())
        return fail(ErrorInvalidRequest, "invalid request", usable_id(request), notification);

    auto table_it = m_methods.find(method_it->get<std::string>());
    if (table_it == m_methods.end())
        return fail(ErrorMethodNotFound, "method not found", usable_id(request), notification);
    if (table_it->second.requires_auth && !authed)
        return fail(ErrorUnauthorized, "unauthorized", usable_id(request), notification);

    const auto params_it = request.find("params");
    if (params_it != request.end() && !params_it->is_object())
        return fail(ErrorInvalidParams, "invalid params", usable_id(request), notification);
    const json omitted_params = nullptr;
    const json &params = params_it == request.end() ? omitted_params : *params_it;
    json result;
    try {
        result = table_it->second.handler(params);
    } catch (const MethodError &e) {
        json error = {{"code", e.code}, {"message", e.message}};
        if (!e.stage.empty())
            error["stage"] = e.stage;
        return respond({{"error", std::move(error)}}, usable_id(request), notification);
    } catch (const std::exception &e) {
        return fail(ErrorInternal, std::string("internal error: ") + e.what(), usable_id(request), notification);
    } catch (...) {
        return fail(ErrorInternal, "internal error", usable_id(request), notification);
    }
    return respond({{"result", std::move(result)}}, usable_id(request), notification);
}

} // namespace LocalAPI
} // namespace Slic3r
