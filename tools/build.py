import shutil
import os
Import("env")

def post_build(source, target, env):
    if os.path.exists(os.path.join(env["PROJECT_DIR"], "build")) == False:
        return

    files = {
        env.subst("$BUILD_DIR/${PROGNAME}.bin"): "firmware_%s_%s.bin" % (env["PIOENV"], env.GetProjectOption("version")),
        env.subst("$BUILD_DIR/${PROGNAME}.factory.bin"): "firmware_%s_%s.factory.bin" % (env["PIOENV"], env.GetProjectOption("version")),
    }

    for src in files:
        if os.path.exists(src):
            dest = os.path.join(env["PROJECT_DIR"], "build", files[src])
            
            print("Copying '%s' to '%s'" % (src, dest))
            shutil.copy(src, dest)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", post_build)