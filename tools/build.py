import shutil
import os
Import("env")


def post_build(source, target, env):
  copy_to_build_dir({
    source[0].get_abspath(): "firmware_%s_%s.bin" % (env["PIOENV"], env.GetProjectOption("version")),
    env.subst("$BUILD_DIR/${PROGNAME}.factory.bin"): "firmware_%s_%s.factory.bin" % (env["PIOENV"], env.GetProjectOption("version")),
  }, os.path.join(env["PROJECT_DIR"], "build"));
      
  env.Execute("pio run --target buildfs --environment %s" % env["PIOENV"]);


def after_buildfs(source, target, env):
  copy_to_build_dir({
    source[0].get_abspath(): "filesystem_%s_%s.bin" % (env["PIOENV"], env.GetProjectOption("version")),
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
env.AddPostAction("buildfs", after_buildfs)