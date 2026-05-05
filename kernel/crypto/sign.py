import argparse
from pathlib import Path

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey


PRIVATE_KEY_SIZE = 32
SIGNATURE_SIZE = 64


def load_private_key(path: Path) -> Ed25519PrivateKey:
    key_bytes = path.read_bytes()

    if len(key_bytes) != PRIVATE_KEY_SIZE:
        raise ValueError(
            f"Invalid private key size: {len(key_bytes)} bytes "
            f"(expected {PRIVATE_KEY_SIZE} bytes)"
        )

    return Ed25519PrivateKey.from_private_bytes(key_bytes)


def sign_file(input_path: Path, private_key: Ed25519PrivateKey) -> bytes:
    data = input_path.read_bytes()
    return private_key.sign(data)


def make_check_file_path(input_path: Path) -> Path:
    return input_path.with_suffix(".chk")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Sign a file with Ed25519 private_key.bin"
    )
    parser.add_argument("input_file", help="File to sign")
    parser.add_argument(
        "-k",
        "--key",
        default="private_key.bin",
        help="Path to Ed25519 private key binary file",
    )

    args = parser.parse_args()

    input_path = Path(args.input_file)
    key_path = Path(args.key)
    output_path = make_check_file_path(input_path)

    private_key = load_private_key(key_path)
    signature = sign_file(input_path, private_key)

    if len(signature) != SIGNATURE_SIZE:
        raise ValueError(
            f"Invalid signature size: {len(signature)} bytes "
            f"(expected {SIGNATURE_SIZE} bytes)"
        )

    output_path.write_bytes(signature)

    print(f"Input : {input_path}")
    print(f"Output: {output_path}")
    print(f"Size  : {len(signature)} bytes")


if __name__ == "__main__":
    main()