#include "Auth.hpp"

namespace Slic3r {
namespace LocalAPI {

bool Auth::verify(const std::string &presented) const noexcept
{
    if (m_token.empty() || presented.size() != m_token.size())
        return false;
    unsigned diff = 0;
    for (size_t i = 0; i < m_token.size(); ++i)
        diff |= static_cast<unsigned>(m_token[i] ^ presented[i]);
    return diff == 0;
}

} // namespace LocalAPI
} // namespace Slic3r
