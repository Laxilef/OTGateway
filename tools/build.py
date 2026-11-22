import shutil
import os
Import("env")


def post_build(source, target, env):
  copy_to_build_dir({
    source[0].get_abspath():                          "firmware_%s_%s.bin" % (env["PIOENV"], env.GetProjectOption("custom_version")),
    env.subst("$BUILD_DIR/${PROGNAME}.factory.bin"):  "firmware_%s_%s.factory.bin" % (env["PIOENV"], env.GetProjectOption("custom_version")),
    env.subst("$BUILD_DIR/${PROGNAME}.elf"):          "firmware_%s_%s.elf" % (env["PIOENV"], env.GetProjectOption("custom_version"))
  }, os.path.join(env["PROJECT_DIR"], "build"));
  
  return env.Execute('"$PYTHONEXE" -m platformio run --target buildfs --environment "$PIOENV"')


def before_buildfs(source, target, env):
  result = env.Execute("npm install --silent")
  if result:
    return result
  return env.Execute("npx gulp build_all --no-deprecation")

def after_buildfs(source, target, env):
  copy_to_build_dir({
    source[0].get_abspath(): "filesystem_%s_%s.bin" % (env["PIOENV"], env.GetProjectOption("custom_version")),
  }, os.path.join(env["PROJECT_DIR"], "build"));


def copy_to_build_dir(files, build_dir):
  if os.path.exists(build_dir) == False:
    return
  
  for src in files:
    if os.path.exists(src):
      dst = os.path.join(build_dir, files[src])
      
      print("Copying '%s' to '%s'" % (src, dst))
      shutil.copy(src, dst)


env.AddPostAction("buildprog", post_build)
env.AddPreAction("$BUILD_DIR/spiffs.bin", before_buildfs)
env.AddPreAction("$BUILD_DIR/littlefs.bin", before_buildfs)
env.AddPostAction("buildfs", after_buildfs)
