from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization


def format_c_byte_array(data: bytes, bytes_per_line: int = 8) -> str:
    lines = []

    for i in range(0, len(data), bytes_per_line):
        chunk = data[i:i + bytes_per_line]
        line = ", ".join(f"0x{b:02x}" for b in chunk)
        lines.append("    " + line + ",")

    return "\n".join(lines)


def main() -> None:
    private_key = Ed25519PrivateKey.generate()
    public_key = private_key.public_key()

    private_bytes = private_key.private_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PrivateFormat.Raw,
        encryption_algorithm=serialization.NoEncryption(),
    )

    public_bytes = public_key.public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw,
    )

    if len(private_bytes) != 32:
        raise ValueError(f"Invalid private key size: {len(private_bytes)}")

    if len(public_bytes) != 32:
        raise ValueError(f"Invalid public key size: {len(public_bytes)}")

    with open("private_key.bin", "wb") as f:
        f.write(private_bytes)

    with open("public_key.txt", "w", encoding="utf-8") as f:
        f.write(format_c_byte_array(public_bytes))
        f.write("\n")

    print("Generated:")
    print("  private_key.bin")
    print("  public_key.txt")
    print()
    print("private key hex:")
    print(private_bytes.hex())
    print()
    print("public key hex:")
    print(public_bytes.hex())


if __name__ == "__main__":
    main()