"""Provide Arduino TinyUSB for the ESP32-S2 HybridCompile SDK."""

from io import BytesIO
from pathlib import Path
import shutil
from urllib.request import Request, urlopen
from zipfile import ZipFile

from SCons.Script import Return

Import("env")

if env.IsCleanTarget():
  Return()


def download_sources(repository, revision, cache, paths):
  directory = cache / (repository.rsplit("/", 1)[-1] + "-" + revision)
  marker = directory / ".ready"
  if marker.is_file() and all((directory / path).exists() for path in paths):
    return directory
  marker.unlink(missing_ok=True)

  request = Request("https://codeload.github.com/" + repository + "/zip/" + revision,
                    headers={"User-Agent": "OTGateway"})
  with urlopen(request, timeout=60) as response, ZipFile(BytesIO(response.read())) as archive:
    if len(archive.comment) != 40 or not archive.comment.decode("ascii").startswith(revision):
      raise RuntimeError("Source archive does not match SDK revision " + revision)
    prefix = directory.name + "/"
    members = [name for name in archive.namelist()
               if any(name == prefix + path or name.startswith(prefix + path + "/") for path in paths)]
    archive.extractall(cache, members=members)

  if not all((directory / path).exists() for path in paths):
    raise RuntimeError("Source archive is missing required files")
  marker.touch()
  return directory


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
sources = cache / "sources"
sources.mkdir(exist_ok=True)
builder = download_sources("espressif/esp32-arduino-lib-builder", builder_revision, sources,
                           ("components/arduino_tinyusb/CMakeLists.txt",
                            "components/arduino_tinyusb/Kconfig.projbuild", "components/arduino_tinyusb/include"))
tinyusb = download_sources("hathach/tinyusb", tinyusb_revision, sources, ("src", "hw/bsp"))

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
