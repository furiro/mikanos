#include <stdint.h>
#include <vector>

#include "error.hpp"
#include "monocypher-4.0.2/src/monocypher-ed25519.h"

static constexpr uint8_t kPublicKey[] = {
    #include "public_key.txt"
};


WithError<uint64_t> VerifyModuleSignature(const std::vector<uint8_t> module_data, const std::vector<uint8_t> module_signature) {
    // [ToDo]サイズのチェックとか
    const int result = crypto_ed25519_check(module_signature.data(), kPublicKey, module_data.data(), module_data.size());
    if (result != 0) {
        return { 0, MAKE_ERROR(Error::kTransferFailed) };
    }


    return { 0, MAKE_ERROR(Error::kSuccess) };
}


