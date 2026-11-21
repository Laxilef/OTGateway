"""Apply environment flags after SDK compile environments are prepared."""

from pathlib import Path
import re

Import("env")

build_frameworks = env.BuildFrameworks


def no_exception_link_flags(sdk_directory):
  cmake = (Path(sdk_directory) / "components/cxx/CMakeLists.txt").read_text()
  wrappers = re.findall(r"set\(WRAP_FUNCTIONS\s+([^)]*)\)", cmake, re.S)
  forced = re.findall(
    r'if\(NOT CONFIG_COMPILER_CXX_EXCEPTIONS\)\s+'
    r'target_link_libraries\(\$\{COMPONENT_LIB\} INTERFACE "(-u [A-Za-z_][A-Za-z0-9_]*)"\)\s+endif\(\)',
    cmake
  )
  if len(wrappers) != 1 or len(forced) != 1:
    raise RuntimeError("ESP-IDF C++ linker metadata changed; review tools/after_build_sdk.py")

  symbols = wrappers[0].split()
  if (not symbols or len(symbols) != len(set(symbols))
      or not all(re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", symbol) for symbol in symbols)):
    raise RuntimeError("Unsupported ESP-IDF C++ wrapper list")

  return ["-Wl,--wrap=" + symbol for symbol in symbols] + forced[0].split()


def configure_after_build_sdk_flags(build_env, frameworks):
  result = build_frameworks(frameworks)

  # SDK environments are already cloned; project libraries are configured later.
  flags = build_env.GetProjectOption("custom_app_flags", "")
  build_env.ProcessFlags(flags)
  link_flags = build_env.GetProjectOption("custom_link_flags", "").split()

  sdkconfig = build_env.GetProjectOption("custom_sdkconfig", "")
  if any(line.strip().strip("'\"") == "# CONFIG_COMPILER_CXX_EXCEPTIONS is not set"
         for line in sdkconfig.splitlines()):
    # HybridCompile does not transfer this component's interface linker flags.
    sdk_directory = build_env.PioPlatform().get_package_dir("framework-espidf")
    if not sdk_directory:
      raise RuntimeError("ESP-IDF package is required for the C++ linker bridge")
    link_flags += no_exception_link_flags(sdk_directory)

  build_env.Append(LINKFLAGS=link_flags)
  return result


env.AddMethod(configure_after_build_sdk_flags, "BuildFrameworks")
