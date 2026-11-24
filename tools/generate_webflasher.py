import argparse
import json
from pathlib import Path


def generate_manifest(artifacts, environments):
  firmware_files = {}
  # Match specific environments before their prefixes, e.g. esp32_c6_ethernet.
  prefixes = sorted(environments, key=len, reverse=True)
  for firmware in sorted(artifacts.glob("firmware_*.factory.bin")):
    environment = next((
      name for name in prefixes if firmware.name.startswith(f"firmware_{name}_")
    ), None)
    if environment is None:
      raise ValueError(f"Unexpected factory image: {firmware.name}")
    if environment in firmware_files:
      raise ValueError(f"Multiple factory images for {environment}")
    firmware_files[environment] = firmware

  manifest = []
  for environment in environments:
    firmware = firmware_files.get(environment)
    if firmware is None:
      raise ValueError(f"Factory image missing for {environment}")
    version = firmware.name.removeprefix(f"firmware_{environment}_").removesuffix(".factory.bin")
    filesystem = artifacts / f"filesystem_{environment}_{version}.bin"
    if not filesystem.is_file():
      raise ValueError(f"Filesystem image missing: {filesystem.name}")

    manifest.append({
      "name": environment,
      "files": [
        {"address": "0x0", "name": firmware.name},
        {"address": "0x370000", "name": filesystem.name},
      ],
    })
  return manifest


def main():
  parser = argparse.ArgumentParser(description="Generate the release Web Flasher manifest")
  parser.add_argument("--envs", required=True, type=json.loads)
  parser.add_argument("--artifacts", type=Path, default=Path("artifacts"))
  args = parser.parse_args()
  if not isinstance(args.envs, list) or not args.envs:
    parser.error("--envs must be a nonempty JSON array")

  try:
    manifest = generate_manifest(args.artifacts, args.envs)
  except ValueError as error:
    parser.error(str(error))
  output = args.artifacts / "webflasher.json"
  output.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
  main()
