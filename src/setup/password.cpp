#include "setup/password.h"

#include <fstream>
#include <stdexcept>

namespace purr::setup {

std::string newPassword()
{
    std::ifstream random("/dev/urandom", std::ios::binary);
    if (!random)
        throw std::runtime_error("cannot read /dev/urandom");
    // Bytes past the last whole multiple of the alphabet are skipped, so each letter is equally likely.
    const unsigned limit = 256 - 256 % PASSWORD_ALPHABET.size();
    std::string out;
    while (out.size() < PASSWORD_LENGTH) {
        unsigned char b = 0;
        random.read(reinterpret_cast<char*>(&b), 1);
        if (!random)
            throw std::runtime_error("cannot read /dev/urandom");
        if (b < limit)
            out += PASSWORD_ALPHABET[b % PASSWORD_ALPHABET.size()];
    }
    return out;
}

} // namespace purr::setup
