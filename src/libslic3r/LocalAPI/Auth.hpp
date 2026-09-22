// Local control API: bearer-token store and check (spec §5).
//
// GUI-independent: no keychain, no AppConfig here — the owner loads and
// persists the token, this class only holds and compares it.

#pragma once

#include <string>

namespace Slic3r {
namespace LocalAPI {

class Auth {
public:
    // Installs (or rotates) the token. An empty token means none is set:
    // verification then fails closed.
    void set_token(std::string token) { m_token = std::move(token); }
    bool has_token() const { return !m_token.empty(); }

    // Constant-time comparison against the stored token. Never throws.
    bool verify(const std::string &presented) const noexcept;

private:
    std::string m_token;
};

} // namespace LocalAPI
} // namespace Slic3r

