#include "pch.h"

#include "game/sha256.h"

namespace orion2re::crypto {
    namespace {

        constexpr std::array<std::uint32_t, 64> constants = {
            0x428a2f98,
            0x71374491,
            0xb5c0fbcf,
            0xe9b5dba5,
            0x3956c25b,
            0x59f111f1,
            0x923f82a4,
            0xab1c5ed5,
            0xd807aa98,
            0x12835b01,
            0x243185be,
            0x550c7dc3,
            0x72be5d74,
            0x80deb1fe,
            0x9bdc06a7,
            0xc19bf174,
            0xe49b69c1,
            0xefbe4786,
            0x0fc19dc6,
            0x240ca1cc,
            0x2de92c6f,
            0x4a7484aa,
            0x5cb0a9dc,
            0x76f988da,
            0x983e5152,
            0xa831c66d,
            0xb00327c8,
            0xbf597fc7,
            0xc6e00bf3,
            0xd5a79147,
            0x06ca6351,
            0x14292967,
            0x27b70a85,
            0x2e1b2138,
            0x4d2c6dfc,
            0x53380d13,
            0x650a7354,
            0x766a0abb,
            0x81c2c92e,
            0x92722c85,
            0xa2bfe8a1,
            0xa81a664b,
            0xc24b8b70,
            0xc76c51a3,
            0xd192e819,
            0xd6990624,
            0xf40e3585,
            0x106aa070,
            0x19a4c116,
            0x1e376c08,
            0x2748774c,
            0x34b0bcb5,
            0x391c0cb3,
            0x4ed8aa4a,
            0x5b9cca4f,
            0x682e6ff3,
            0x748f82ee,
            0x78a5636f,
            0x84c87814,
            0x8cc70208,
            0x90befffa,
            0xa4506ceb,
            0xbef9a3f7,
            0xc67178f2,
        };

    } // namespace

    Sha256Digest sha256(std::string_view source) {
        std::vector<std::uint8_t> bytes(source.begin(), source.end());
        const std::uint64_t bit_length = static_cast<std::uint64_t>(bytes.size()) * 8;
        bytes.push_back(0x80);
        while (bytes.size() % 64 != 56) {
            bytes.push_back(0);
        }
        for (int shift = 56; shift >= 0; shift -= 8) {
            bytes.push_back(static_cast<std::uint8_t>(bit_length >> shift));
        }

        std::array<std::uint32_t, 8> state = {
            0x6a09e667,
            0xbb67ae85,
            0x3c6ef372,
            0xa54ff53a,
            0x510e527f,
            0x9b05688c,
            0x1f83d9ab,
            0x5be0cd19,
        };
        for (std::size_t block = 0; block < bytes.size(); block += 64) {
            std::array<std::uint32_t, 64> words{};
            for (std::size_t index = 0; index < 16; ++index) {
                const std::size_t offset = block + index * 4;
                words[index] = (static_cast<std::uint32_t>(bytes[offset]) << 24) |
                    (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
                    (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
                    static_cast<std::uint32_t>(bytes[offset + 3]);
            }
            for (std::size_t index = 16; index < words.size(); ++index) {
                const std::uint32_t first = std::rotr(words[index - 15], 7) ^
                    std::rotr(words[index - 15], 18) ^
                    (words[index - 15] >> 3);
                const std::uint32_t second = std::rotr(words[index - 2], 17) ^
                    std::rotr(words[index - 2], 19) ^
                    (words[index - 2] >> 10);
                words[index] = words[index - 16] + first + words[index - 7] + second;
            }

            std::uint32_t a = state[0];
            std::uint32_t b = state[1];
            std::uint32_t c = state[2];
            std::uint32_t d = state[3];
            std::uint32_t e = state[4];
            std::uint32_t f = state[5];
            std::uint32_t g = state[6];
            std::uint32_t h = state[7];
            for (std::size_t index = 0; index < words.size(); ++index) {
                const std::uint32_t choose = (e & f) ^ (~e & g);
                const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                const std::uint32_t sum_a = std::rotr(a, 2) ^ std::rotr(a, 13) ^
                    std::rotr(a, 22);
                const std::uint32_t sum_e = std::rotr(e, 6) ^ std::rotr(e, 11) ^
                    std::rotr(e, 25);
                const std::uint32_t first = h + sum_e + choose + constants[index] +
                    words[index];
                const std::uint32_t second = sum_a + majority;
                h = g;
                g = f;
                f = e;
                e = d + first;
                d = c;
                c = b;
                b = a;
                a = first + second;
            }
            state[0] += a;
            state[1] += b;
            state[2] += c;
            state[3] += d;
            state[4] += e;
            state[5] += f;
            state[6] += g;
            state[7] += h;
        }

        Sha256Digest digest{};
        for (std::size_t index = 0; index < state.size(); ++index) {
            for (std::size_t byte = 0; byte < 4; ++byte) {
                digest[index * 4 + byte] = static_cast<std::uint8_t>(
                    state[index] >> (24 - static_cast<int>(byte) * 8));
            }
        }
        return digest;
    }

    bool sha256(const void* source,
        std::size_t source_size,
        std::uint8_t* digest,
        std::size_t digest_capacity) {
        if ((source == nullptr && source_size != 0) || digest == nullptr ||
            digest_capacity < Sha256Digest{}.size()) {
            return false;
        }
        const Sha256Digest value = sha256(std::string_view(
            static_cast<const char*>(source), source_size));
        std::copy(value.begin(), value.end(), digest);
        return true;
    }

    std::string sha256_hex(std::string_view source) {
        return sha256_hex(sha256(source));
    }

    std::string sha256_hex(const Sha256Digest& digest) {
        static constexpr char digits[] = "0123456789abcdef";
        std::string result(digest.size() * 2, '0');
        for (std::size_t index = 0; index < digest.size(); ++index) {
            result[index * 2] = digits[digest[index] >> 4];
            result[index * 2 + 1] = digits[digest[index] & 0x0f];
        }
        return result;
    }

} // namespace orion2re::crypto