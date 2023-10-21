import shutil
import os
Import("env")

def post_build(source, target, env):
    src = target[0].get_abspath()
    dest = os.path.join(env["PROJECT_DIR"], "build", "firmware_%s_%s.bin" % (env.GetProjectOption("board"), env.GetProjectOption("version")))

    #print("dest:"+dest)
    #print("source:"+src)

    shutil.copy(src, dest)

env.AddPostAction("$BUILD_DIR/firmware.bin", post_build)