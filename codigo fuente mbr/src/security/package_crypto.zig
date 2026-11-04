const std = @import("std");
const Ed25519 = std.crypto.sign.Ed25519;

const TrustedKey = struct {
    id: [16]u8,
    public_key: [Ed25519.PublicKey.encoded_length]u8,
};

// Intentionally empty until VOS has an official release key. Never add a
// development or publicly known test key to this table.
const trusted_keys = [_]TrustedKey{};

fn verify_ed25519(public_key_bytes: [32]u8, message: []const u8,
    signature_bytes: [64]u8) bool {
    const public_key = Ed25519.PublicKey.fromBytes(public_key_bytes) catch {
        return false;
    };
    const signature = Ed25519.Signature.fromBytes(signature_bytes);
    signature.verifyStrict(message, public_key) catch return false;
    return true;
}

export fn package_crypto_verify_trusted(
    key_id_pointer: ?[*]const u8,
    message_pointer: ?[*]const u8,
    message_size: u32,
    signature_pointer: ?[*]const u8,
) callconv(.c) c_int {
    const key_id_bytes = key_id_pointer orelse return -1;
    const message_bytes = message_pointer orelse return -1;
    const signature_bytes = signature_pointer orelse return -1;

    var requested_key_id: [16]u8 = undefined;
    @memcpy(&requested_key_id, key_id_bytes[0..16]);

    for (trusted_keys) |trusted_key| {
        if (!std.mem.eql(u8, &trusted_key.id, &requested_key_id)) continue;

        var encoded_signature: [Ed25519.Signature.encoded_length]u8 = undefined;
        @memcpy(&encoded_signature, signature_bytes[0..encoded_signature.len]);
        const message_length: usize = @intCast(message_size);
        return if (verify_ed25519(trusted_key.public_key,
            message_bytes[0..message_length], encoded_signature)) 1 else -1;
    }

    return 0;
}

test "Ed25519 RFC 8032 test vector 1" {
    const public_key = [_]u8{
        0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7,
        0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
        0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25,
        0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a,
    };
    const signature = [_]u8{
        0xe5, 0x56, 0x43, 0x00, 0xc3, 0x60, 0xac, 0x72,
        0x90, 0x86, 0xe2, 0xcc, 0x80, 0x6e, 0x82, 0x8a,
        0x84, 0x87, 0x7f, 0x1e, 0xb8, 0xe5, 0xd9, 0x74,
        0xd8, 0x73, 0xe0, 0x65, 0x22, 0x49, 0x01, 0x55,
        0x5f, 0xb8, 0x82, 0x15, 0x90, 0xa3, 0x3b, 0xac,
        0xc6, 0x1e, 0x39, 0x70, 0x1c, 0xf9, 0xb4, 0x6b,
        0xd2, 0x5b, 0xf5, 0xf0, 0x59, 0x5b, 0xbe, 0x24,
        0x65, 0x51, 0x41, 0x43, 0x8e, 0x7a, 0x10, 0x0b,
    };
    try std.testing.expect(verify_ed25519(public_key, "", signature));

    var altered = signature;
    altered[17] ^= 1;
    try std.testing.expect(!verify_ed25519(public_key, "", altered));
    try std.testing.expect(!verify_ed25519(public_key, "x", signature));
}
