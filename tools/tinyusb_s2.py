"""Provide Arduino TinyUSB for the ESP32-S2 HybridCompile SDK."""

from pathlib import Path
import shutil
import subprocess

from SCons.Script import Return

Import("env")

if env.IsCleanTarget():
  Return()


def checkout(repository, revision, directory):
  if not (directory / ".git").is_dir():
    subprocess.run(["git", "clone", "--filter=blob:none", "--no-checkout",
                    repository, str(directory)], check=True)
  current = subprocess.run(["git", "-C", str(directory), "rev-parse", "HEAD"],
                           check=True, capture_output=True, text=True).stdout.strip()
  if not current.startswith(revision):
    subprocess.run(["git", "-C", str(directory), "checkout", "--detach", revision], check=True)


platform = env.PioPlatform()
project = Path(env.subst("$PROJECT_DIR"))
framework = Path(platform.get_package_dir("framework-arduinoespressif32"))
dummy = project / ".dummy"
sdk = Path(platform.get_package_dir("framework-arduinoespressif32-libs"))
versions = {}
for line in (sdk / "versions.txt").read_text(encoding="utf-8").splitlines():
  if ": " in line:
    name, value = line.split(": ", 1)
    versions[name] = value.split()[-1]

builder_revision = versions["lib-builder"]
tinyusb_revision = versions["tinyusb"]
cache = project / ".pio/tinyusb-s2"
cache.mkdir(parents=True, exist_ok=True)
builder = cache / ("lib-builder-" + builder_revision)
tinyusb = cache / ("tinyusb-" + tinyusb_revision)
checkout("https://github.com/espressif/esp32-arduino-lib-builder.git", builder_revision, builder)
checkout("https://github.com/hathach/tinyusb.git", tinyusb_revision, tinyusb)

original = builder / "components/arduino_tinyusb"
component = cache / ("component-" + builder_revision + "-" + tinyusb_revision) / "arduino_tinyusb"
cmake_file = component / "CMakeLists.txt"
if not cmake_file.is_file():
  component.mkdir(parents=True, exist_ok=True)
  shutil.copytree(original / "include", component / "include", dirs_exist_ok=True)
  shutil.copytree(tinyusb / "src", component / "tinyusb/src", dirs_exist_ok=True)
  shutil.copytree(tinyusb / "hw/bsp", component / "tinyusb/hw/bsp", dirs_exist_ok=True)
  shutil.copyfile(original / "Kconfig.projbuild", component / "Kconfig.projbuild")

# Adapt only the generated build description to HybridCompile component names.
cmake = (original / "CMakeLists.txt").read_text(encoding="utf-8")
requires = "set(priv_requires arduino main)"
if requires not in cmake:
  raise RuntimeError("Unsupported Arduino TinyUSB component dependencies")
cmake = cmake.replace(requires, f"set(priv_requires {framework.name} {dummy.name})")
if not cmake_file.is_file() or cmake_file.read_text(encoding="utf-8") != cmake:
  cmake_file.write_text(cmake, encoding="utf-8")

# Keep HybridCompile's two default components when extending the CMake list.
dummy.mkdir(exist_ok=True)
directories = [dummy, framework, component]
arguments = env.BoardConfig().get("build.cmake_extra_args", "") or ""
arguments += ' "-DEXTRA_COMPONENT_DIRS:PATH=' + ";".join(path.as_posix() for path in directories) + '"'
env.BoardConfig().update("build.cmake_extra_args", arguments.strip())
