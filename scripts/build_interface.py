import subprocess
import os
import hashlib

Import("env")

interface_dir = os.path.join(env.get("PROJECT_DIR"), "interface")
data_dir = os.path.join(env.get("PROJECT_DIR"), "data")
hash_file = os.path.join(interface_dir, ".build_hash")

def get_source_hash():
    hasher = hashlib.md5()
    for root, _, files in os.walk(os.path.join(interface_dir, "src")):
        for f in sorted(files):
            path = os.path.join(root, f)
            with open(path, "rb") as fp:
                hasher.update(fp.read())
    for f in ["package.json", "vite.config.ts", "tailwind.config.js", "index.html"]:
        path = os.path.join(interface_dir, f)
        if os.path.exists(path):
            with open(path, "rb") as fp:
                hasher.update(fp.read())
    return hasher.hexdigest()

def get_cached_hash():
    if os.path.exists(hash_file):
        with open(hash_file, "r") as f:
            return f.read().strip()
    return None

def save_hash(h):
    with open(hash_file, "w") as f:
        f.write(h)

def build_interface(source, target, env):
    current_hash = get_source_hash()
    cached_hash = get_cached_hash()
    
    if current_hash == cached_hash and os.path.exists(os.path.join(data_dir, "index.html.gz")):
        print("Interface unchanged, skipping build")
        return
    
    print("Building interface...")
    
    if not os.path.exists(os.path.join(interface_dir, "node_modules")):
        print("Installing npm dependencies...")
        subprocess.run(["npm", "install"], cwd=interface_dir, check=True)
    
    subprocess.run(["npm", "run", "build"], cwd=interface_dir, check=True)
    save_hash(current_hash)
    print("Interface build complete")

env.AddPreAction("buildfs", build_interface)
env.AddPreAction("$BUILD_DIR/littlefs.bin", build_interface)
