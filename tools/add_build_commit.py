import subprocess
Import("env")

try:
  commit_hash = "undefined"
  result = subprocess.check_output(
    ["git", "rev-parse", "--short", "HEAD"]
  )
  commit_hash = result.decode("utf-8").strip()
  env.Append(
    CPPDEFINES=[
      ("BUILD_COMMIT", '\\"{}\\"'.format(commit_hash))
    ]
  )
except Exception as error:
  print("Failed to get commit hash: {}".format(error))