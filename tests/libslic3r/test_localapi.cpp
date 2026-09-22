#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "libslic3r/LocalAPI/SliceLease.hpp"

#include "libslic3r/LocalAPI/Dispatcher.hpp"

#include "libslic3r/LocalAPI/Auth.hpp"

#include <nlohmann/json.hpp>

using Slic3r::LocalAPI::Dispatcher;
using Slic3r::LocalAPI::MethodError;
using json = nlohmann::json;

namespace {

// A table with one echo method; everything else exercises the envelope.
Dispatcher make_table()
{
    using Slic3r::LocalAPI::json;
    Dispatcher d;
    d.add_method("server.echo",
        [](const json &params) { return params; });
    d.add_method("server.info",
        [](const json &) { return json({{"impl", "test"}}); },
        false /* requires_auth */);
    d.add_method("job.fail",
        [](const json &) -> json {
            throw MethodError{2100, "no such job", ""};
        });
    d.add_method("slice.fail",
        [](const json &) -> json {
            throw MethodError{3000, "support generation failed", "slicing"};
        });
    d.add_method("job.crash",
        [](const json &) -> json {
            throw std::runtime_error("boom");
        });
    return d;
}

json dispatch(const Dispatcher &d, const std::string &req, bool authed = true)
{
    const std::string out = d.dispatch(req, authed);
    CHECK_FALSE(out.empty());
    return json::parse(out);
}

} // namespace

TEST_CASE("registered method returns result envelope echoing numeric id", "[LocalAPI]")
{
    const json r = dispatch(make_table(), R"({"jsonrpc":"2.0","method":"server.echo","params":{"a":1},"id":7})");
    CHECK(r.at("jsonrpc") == "2.0");
    CHECK(r.at("id") == 7);
    CHECK(r.at("result") == json({{"a", 1}}));
}

TEST_CASE("string id is echoed verbatim", "[LocalAPI]")
{
    const json r = dispatch(make_table(), R"({"jsonrpc":"2.0","method":"server.echo","params":{},"id":"req-1"})");
    CHECK(r.at("id") == "req-1");
    CHECK(r.at("result") == json::object());
}

TEST_CASE("unknown method returns MethodNotFound with id", "[LocalAPI]")
{
    const json r = dispatch(make_table(), R"({"jsonrpc":"2.0","method":"nope.nope","id":3})");
    CHECK(r.at("id") == 3);
    CHECK(r.at("error").at("code") == -32601);
}

TEST_CASE("malformed json returns ParseError with null id", "[LocalAPI]")
{
    const json r = dispatch(make_table(), "{not json");
    CHECK(r.at("id").is_null());
    CHECK(r.at("error").at("code") == -32700);
}

TEST_CASE("batch array is rejected as InvalidRequest", "[LocalAPI]")
{
    const json r = dispatch(make_table(), R"([{"jsonrpc":"2.0","method":"server.echo","id":1}])");
    CHECK(r.at("error").at("code") == -32600);
}

TEST_CASE("missing jsonrpc member is rejected as InvalidRequest", "[LocalAPI]")
{
    const json r = dispatch(make_table(), R"({"method":"server.echo","id":1})");
    CHECK(r.at("error").at("code") == -32600);
}

TEST_CASE("notification returns no response", "[LocalAPI]")
{
    const Dispatcher d = make_table();
    CHECK(d.dispatch(R"({"jsonrpc":"2.0","method":"server.echo","params":{}})", true).empty());
}

TEST_CASE("unknown method notification returns no response", "[LocalAPI]")
{
    const Dispatcher d = make_table();
    CHECK(d.dispatch(R"({"jsonrpc":"2.0","method":"nope.nope"})", true).empty());
}

TEST_CASE("extra top-level members are ignored", "[LocalAPI]")
{
    const json r = dispatch(make_table(), R"({"jsonrpc":"2.0","method":"server.echo","params":{"a":1},"id":9,"foo":"bar"})");
    CHECK(r.at("id") == 9);
    CHECK(r.at("result") == json({{"a", 1}}));
}

TEST_CASE("handler app error maps to code and message envelope", "[LocalAPI]")
{
    const json r = dispatch(make_table(), R"({"jsonrpc":"2.0","method":"job.fail","id":4})");
    CHECK(r.at("id") == 4);
    CHECK(r.at("error").at("code") == 2100);
    CHECK(r.at("error").at("message") == "no such job");
}

TEST_CASE("handler app error carries stage when set", "[LocalAPI]")
{
    const json r = dispatch(make_table(), R"({"jsonrpc":"2.0","method":"slice.fail","id":5})");
    CHECK(r.at("error").at("code") == 3000);
    CHECK(r.at("error").at("stage") == "slicing");
}

TEST_CASE("unexpected handler exception maps to InternalError", "[LocalAPI]")
{
    const json r = dispatch(make_table(), R"({"jsonrpc":"2.0","method":"job.crash","id":6})");
    CHECK(r.at("error").at("code") == -32603);
}

TEST_CASE("unauthenticated call to guarded method returns Unauthorized", "[LocalAPI]")
{
    const json r = dispatch(make_table(), R"({"jsonrpc":"2.0","method":"server.echo","params":{},"id":1})", false);
    CHECK(r.at("error").at("code") == 1200);
}

TEST_CASE("exempt method works without authentication", "[LocalAPI]")
{
    const json r = dispatch(make_table(), R"({"jsonrpc":"2.0","method":"server.info","id":2})", false);
    CHECK(r.at("result") == json({{"impl", "test"}}));
}

TEST_CASE("correct token verifies", "[LocalAPI]")
{
    Slic3r::LocalAPI::Auth auth;
    auth.set_token("s3cret-token");
    CHECK(auth.verify("s3cret-token"));
}

TEST_CASE("wrong token is rejected", "[LocalAPI]")
{
    Slic3r::LocalAPI::Auth auth;
    auth.set_token("s3cret-token");
    CHECK_FALSE(auth.verify("wrong-token"));
    CHECK_FALSE(auth.verify("s3cret-toke"));
    CHECK_FALSE(auth.verify("s3cret-token!"));
}

TEST_CASE("empty presented token is rejected", "[LocalAPI]")
{
    Slic3r::LocalAPI::Auth auth;
    auth.set_token("s3cret-token");
    CHECK_FALSE(auth.verify(""));
}

TEST_CASE("verification fails closed with no token set", "[LocalAPI]")
{
    Slic3r::LocalAPI::Auth auth;
    CHECK_FALSE(auth.has_token());
    CHECK_FALSE(auth.verify("anything"));
    CHECK_FALSE(auth.verify(""));
}

TEST_CASE("token rotation replaces the secret", "[LocalAPI]")
{
    Slic3r::LocalAPI::Auth auth;
    auth.set_token("old-token");
    auth.set_token("new-token");
    CHECK(auth.verify("new-token"));
    CHECK_FALSE(auth.verify("old-token"));
}

TEST_CASE("idle lease grants API acquire", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease;
    const auto got = lease.acquire_api(1);
    CHECK(got.result == Slic3r::LocalAPI::AcquireResult::Acquired);
    CHECK(lease.holder() == Slic3r::LocalAPI::LeaseHolder::Api);
    CHECK(lease.active_job() == 1);
}

TEST_CASE("idle lease grants GUI acquire", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease;
    CHECK(lease.acquire_gui() == Slic3r::LocalAPI::AcquireResult::Acquired);
    CHECK(lease.holder() == Slic3r::LocalAPI::LeaseHolder::Gui);
}

TEST_CASE("second API acquire queues with visible position", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease;
    lease.acquire_api(1);
    const auto second = lease.acquire_api(2);
    const auto third = lease.acquire_api(3);
    CHECK(second.result == Slic3r::LocalAPI::AcquireResult::Queued);
    CHECK(second.position == 0);
    CHECK(third.position == 1);
    CHECK(lease.queue_length() == 2);
}

TEST_CASE("release promotes the head of the queue", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease;
    lease.acquire_api(1);
    lease.acquire_api(2);
    lease.acquire_api(3);
    lease.release_api();
    CHECK(lease.holder() == Slic3r::LocalAPI::LeaseHolder::Api);
    CHECK(lease.active_job() == 2);
    lease.release_api();
    CHECK(lease.active_job() == 3);
    lease.release_api();
    CHECK(lease.holder() == Slic3r::LocalAPI::LeaseHolder::None);
}

TEST_CASE("API acquire preempts a GUI-held lease", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease;
    lease.acquire_gui();
    const auto got = lease.acquire_api(9);
    CHECK(got.result == Slic3r::LocalAPI::AcquireResult::PreemptGui);
    CHECK(lease.holder() == Slic3r::LocalAPI::LeaseHolder::Api);
    CHECK(lease.active_job() == 9);
}

TEST_CASE("GUI acquire waits on an API-held lease", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease;
    lease.acquire_api(1);
    CHECK(lease.acquire_gui() == Slic3r::LocalAPI::AcquireResult::Busy);
    CHECK(lease.holder() == Slic3r::LocalAPI::LeaseHolder::Api);
}

TEST_CASE("GUI reacquire while holding is idempotent", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease;
    lease.acquire_gui();
    CHECK(lease.acquire_gui() == Slic3r::LocalAPI::AcquireResult::Acquired);
    CHECK(lease.holder() == Slic3r::LocalAPI::LeaseHolder::Gui);
}

TEST_CASE("cancel removes a queued job and collapses positions", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease;
    lease.acquire_api(1);
    lease.acquire_api(2);
    lease.acquire_api(3);
    CHECK(lease.cancel_queued(2));
    CHECK(lease.queue_position(3) == 0);
    CHECK(lease.queue_length() == 1);
}

TEST_CASE("cancel of active or absent job fails", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease;
    lease.acquire_api(1);
    lease.acquire_api(2);
    CHECK_FALSE(lease.cancel_queued(1));
    CHECK_FALSE(lease.cancel_queued(99));
}

TEST_CASE("release by the non-holder is a no-op", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease;
    lease.acquire_api(1);
    lease.release_gui();
    CHECK(lease.holder() == Slic3r::LocalAPI::LeaseHolder::Api);
    lease.release_api();
    lease.release_api();
    CHECK(lease.holder() == Slic3r::LocalAPI::LeaseHolder::None);
}

TEST_CASE("preempted GUI resumes when the API queue drains", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease;
    lease.acquire_gui();
    lease.acquire_api(9);
    CHECK(lease.gui_waiting());
    lease.release_api();
    CHECK(lease.holder() == Slic3r::LocalAPI::LeaseHolder::Gui);
    CHECK_FALSE(lease.gui_waiting());
}

TEST_CASE("waiting GUI takes the lease after drain", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease;
    lease.acquire_api(1);
    CHECK(lease.acquire_gui() == Slic3r::LocalAPI::AcquireResult::Busy);
    lease.release_api();
    CHECK(lease.holder() == Slic3r::LocalAPI::LeaseHolder::Gui);
}

TEST_CASE("idle drain with no waiting GUI releases the lease", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease;
    lease.acquire_api(1);
    lease.release_api();
    CHECK(lease.holder() == Slic3r::LocalAPI::LeaseHolder::None);
}

TEST_CASE("GUI teardown clears waiting intent", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease;
    lease.acquire_api(1);
    lease.acquire_gui();
    lease.release_gui();
    lease.release_api();
    CHECK(lease.holder() == Slic3r::LocalAPI::LeaseHolder::None);
}

TEST_CASE("full queue rejects with Full", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease(2);
    lease.acquire_api(1);
    lease.acquire_api(2);
    lease.acquire_api(3);
    const auto got = lease.acquire_api(4);
    CHECK(got.result == Slic3r::LocalAPI::AcquireResult::Full);
    CHECK(lease.queue_length() == 2);
}

TEST_CASE("cancel frees a slot in a full queue", "[LocalAPI]")
{
    Slic3r::LocalAPI::SliceLease lease(1);
    lease.acquire_api(1);
    lease.acquire_api(2);
    CHECK(lease.acquire_api(3).result == Slic3r::LocalAPI::AcquireResult::Full);
    CHECK(lease.cancel_queued(2));
    const auto got = lease.acquire_api(3);
    CHECK(got.result == Slic3r::LocalAPI::AcquireResult::Queued);
    CHECK(got.position == 0);
}
