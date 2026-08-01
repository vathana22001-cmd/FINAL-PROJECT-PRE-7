#include "security.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#endif

namespace security {
namespace {

constexpr int kIterations = 200000;
constexpr char kPrefix[] = "$pbkdf2-sha256$";

std::uint32_t rotateRight(std::uint32_t value, int bits) {
    return (value >> bits) | (value << (32 - bits));
}

std::array<std::uint8_t, 32> sha256(const std::vector<std::uint8_t>& input) {
    static constexpr std::array<std::uint32_t, 64> constants = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
        0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
        0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
        0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
        0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
        0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
        0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
        0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};
    std::array<std::uint32_t, 8> state = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U,
                                          0xa54ff53aU, 0x510e527fU, 0x9b05688cU,
                                          0x1f83d9abU, 0x5be0cd19U};
    std::vector<std::uint8_t> data = input;
    const std::uint64_t bit_length = static_cast<std::uint64_t>(data.size()) * 8U;
    data.push_back(0x80U);
    while (data.size() % 64 != 56) data.push_back(0);
    for (int shift = 56; shift >= 0; shift -= 8) {
        data.push_back(static_cast<std::uint8_t>((bit_length >> shift) & 0xffU));
    }

    for (std::size_t offset = 0; offset < data.size(); offset += 64) {
        std::array<std::uint32_t, 64> words{};
        for (int i = 0; i < 16; ++i) {
            const std::size_t at = offset + static_cast<std::size_t>(i) * 4;
            words[i] = (static_cast<std::uint32_t>(data[at]) << 24) |
                       (static_cast<std::uint32_t>(data[at + 1]) << 16) |
                       (static_cast<std::uint32_t>(data[at + 2]) << 8) |
                       static_cast<std::uint32_t>(data[at + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotateRight(words[i - 15], 7) ^
                                     rotateRight(words[i - 15], 18) ^ (words[i - 15] >> 3);
            const std::uint32_t s1 = rotateRight(words[i - 2], 17) ^
                                     rotateRight(words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }

        std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        std::uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t sum1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
            const std::uint32_t choice = (e & f) ^ (~e & g);
            const std::uint32_t temp1 = h + sum1 + choice + constants[i] + words[i];
            const std::uint32_t sum0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = sum0 + majority;
            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }

    std::array<std::uint8_t, 32> result{};
    for (std::size_t i = 0; i < state.size(); ++i) {
        result[i * 4] = static_cast<std::uint8_t>(state[i] >> 24);
        result[i * 4 + 1] = static_cast<std::uint8_t>(state[i] >> 16);
        result[i * 4 + 2] = static_cast<std::uint8_t>(state[i] >> 8);
        result[i * 4 + 3] = static_cast<std::uint8_t>(state[i]);
    }
    return result;
}

std::array<std::uint8_t, 32> hmacSha256(const std::vector<std::uint8_t>& key,
                                        const std::vector<std::uint8_t>& message) {
    std::array<std::uint8_t, 64> normalized{};
    if (key.size() > normalized.size()) {
        const auto digest = sha256(key);
        std::copy(digest.begin(), digest.end(), normalized.begin());
    } else {
        std::copy(key.begin(), key.end(), normalized.begin());
    }
    std::vector<std::uint8_t> inner(64);
    std::vector<std::uint8_t> outer(64);
    for (std::size_t i = 0; i < normalized.size(); ++i) {
        inner[i] = normalized[i] ^ 0x36U;
        outer[i] = normalized[i] ^ 0x5cU;
    }
    inner.insert(inner.end(), message.begin(), message.end());
    const auto inner_hash = sha256(inner);
    outer.insert(outer.end(), inner_hash.begin(), inner_hash.end());
    return sha256(outer);
}

std::array<std::uint8_t, 32> pbkdf2Portable(const std::string& password,
                                            const std::vector<std::uint8_t>& salt,
                                            int iterations) {
    const std::vector<std::uint8_t> key(password.begin(), password.end());
    std::vector<std::uint8_t> first = salt;
    first.insert(first.end(), {0, 0, 0, 1});
    auto current = hmacSha256(key, first);
    auto result = current;
    for (int round = 1; round < iterations; ++round) {
        const std::vector<std::uint8_t> current_bytes(current.begin(), current.end());
        current = hmacSha256(key, current_bytes);
        for (std::size_t i = 0; i < result.size(); ++i) result[i] ^= current[i];
    }
    return result;
}

std::array<std::uint8_t, 32> pbkdf2(const std::string& password,
                                    const std::vector<std::uint8_t>& salt, int iterations) {
#if defined(_WIN32)
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    std::array<std::uint8_t, 32> result{};
    const NTSTATUS opened = BCryptOpenAlgorithmProvider(
        &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (BCRYPT_SUCCESS(opened)) {
        const NTSTATUS derived = BCryptDeriveKeyPBKDF2(
            algorithm,
            reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
            static_cast<ULONG>(password.size()),
            const_cast<PUCHAR>(salt.data()),
            static_cast<ULONG>(salt.size()),
            static_cast<ULONGLONG>(iterations),
            result.data(),
            static_cast<ULONG>(result.size()),
            0);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (BCRYPT_SUCCESS(derived)) {
            return result;
        }
    }
#endif
    return pbkdf2Portable(password, salt, iterations);
}

template <typename Container>
std::string toHex(const Container& bytes) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::uint8_t byte : bytes) output << std::setw(2) << static_cast<int>(byte);
    return output.str();
}

bool fromHex(const std::string& text, std::vector<std::uint8_t>& bytes) {
    if (text.empty() || text.size() % 2 != 0) return false;
    bytes.clear();
    for (std::size_t i = 0; i < text.size(); i += 2) {
        try {
            std::size_t used = 0;
            const int value = std::stoi(text.substr(i, 2), &used, 16);
            if (used != 2) return false;
            bytes.push_back(static_cast<std::uint8_t>(value));
        } catch (...) {
            return false;
        }
    }
    return true;
}

std::string legacyHash(const std::string& password) {
    std::uint32_t h1 = 5381U;
    std::uint32_t h2 = 52711U;
    for (unsigned char c : password) {
        h1 = ((h1 << 5U) + h1) + c;
        h2 = ((h2 << 5U) + h2) ^ c;
    }
    std::ostringstream output;
    output << std::hex << h1 << h2;
    return output.str();
}

bool constantTimeEqual(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) return false;
    unsigned char difference = 0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        difference |= static_cast<unsigned char>(left[i] ^ right[i]);
    }
    return difference == 0;
}

}  // namespace

std::string hashPassword(const std::string& password) {
    std::array<std::uint8_t, 16> salt{};
    std::random_device random;
    for (auto& byte : salt) byte = static_cast<std::uint8_t>(random());
    const std::vector<std::uint8_t> salt_vector(salt.begin(), salt.end());
    const auto derived = pbkdf2(password, salt_vector, kIterations);
    return std::string(kPrefix) + std::to_string(kIterations) + "$" + toHex(salt) + "$" +
           toHex(derived);
}

bool verifyPassword(const std::string& password, const std::string& stored) {
    if (stored.rfind(kPrefix, 0) != 0) return constantTimeEqual(legacyHash(password), stored);
    const std::size_t iteration_start = sizeof(kPrefix) - 1;
    const std::size_t salt_separator = stored.find('$', iteration_start);
    const std::size_t hash_separator = stored.find('$', salt_separator + 1);
    if (salt_separator == std::string::npos || hash_separator == std::string::npos) return false;
    int iterations = 0;
    try {
        iterations = std::stoi(stored.substr(iteration_start, salt_separator - iteration_start));
    } catch (...) {
        return false;
    }
    if (iterations < 10000 || iterations > 2000000) return false;
    std::vector<std::uint8_t> salt;
    if (!fromHex(stored.substr(salt_separator + 1, hash_separator - salt_separator - 1), salt)) {
        return false;
    }
    const auto derived = pbkdf2(password, salt, iterations);
    return constantTimeEqual(toHex(derived), stored.substr(hash_separator + 1));
}

bool needsPasswordUpgrade(const std::string& stored) {
    if (stored.rfind(kPrefix, 0) != 0) return true;
    const std::size_t start = sizeof(kPrefix) - 1;
    const std::size_t separator = stored.find('$', start);
    if (separator == std::string::npos) return true;
    try {
        return std::stoi(stored.substr(start, separator - start)) < kIterations;
    } catch (...) {
        return true;
    }
}

}  // namespace security
