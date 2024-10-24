import shutil
import gzip
import os
Import("env")


def post_build(source, target, env):
  copy_to_build_dir({
    source[0].get_abspath():                          "firmware_%s_%s.bin" % (env["PIOENV"], env.GetProjectOption("version")),
    env.subst("$BUILD_DIR/${PROGNAME}.factory.bin"):  "firmware_%s_%s.factory.bin" % (env["PIOENV"], env.GetProjectOption("version")),
    env.subst("$BUILD_DIR/${PROGNAME}.elf"):          "firmware_%s_%s.elf" % (env["PIOENV"], env.GetProjectOption("version"))
  }, os.path.join(env["PROJECT_DIR"], "build"));
  
  env.Execute("pio run --target buildfs --environment %s" % env["PIOENV"]);


def before_buildfs(source, target, env):
  env.Execute("npm install --silent")
  env.Execute("npx gulp build_all --no-deprecation")
"""
  src = os.path.join(env["PROJECT_DIR"], "src_data")
  dst = os.path.join(env["PROJECT_DIR"], "data")

  for root, dirs, files in os.walk(src, topdown=False):
    for name in files:
      src_path = os.path.join(root, name)

      with open(src_path, 'rb') as f_in:
          dst_name = name + ".gz"
          dst_path = os.path.join(dst, os.path.relpath(root, src), dst_name)

          if os.path.exists(os.path.join(dst, os.path.relpath(root, src))) == False:
            os.mkdir(os.path.join(dst, os.path.relpath(root, src)))

          with gzip.open(dst_path, 'wb', 9) as f_out:
            shutil.copyfileobj(f_in, f_out)
          
          print("Compressed '%s' to '%s'" % (src_path, dst_path))
"""

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
env.AddPreAction("$BUILD_DIR/spiffs.bin", before_buildfs)
env.AddPreAction("$BUILD_DIR/littlefs.bin", before_buildfs)
env.AddPostAction("buildfs", after_buildfs)