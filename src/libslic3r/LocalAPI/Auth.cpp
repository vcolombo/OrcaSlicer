#include "Auth.hpp"

#include <openssl/crypto.h>

namespace Slic3r {
namespace LocalAPI {

bool Auth::verify(const std::string &presented) const noexcept
{
    if (m_token.empty() || presented.size() != m_token.size())
        return false;
    return CRYPTO_memcmp(m_token.data(), presented.data(), m_token.size()) == 0;
}

} // namespace LocalAPI
} // namespace Slic3r
