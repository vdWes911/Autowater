Import("env")
import subprocess
import sys
import os

def run_minify(source, target, env):
    """Run npm minify before building"""
    print("=" * 60)
    print("Running web asset minification...")
    print("=" * 60)

    project_dir = env.get("PROJECT_DIR")

    try:
        # Check if node_modules exists, if not run npm install
        node_modules = os.path.join(project_dir, 'node_modules')
        if not os.path.exists(node_modules):
            print("Installing npm dependencies...")
            subprocess.run(
                ["npm", "install"],
                cwd=project_dir,
                check=True
            )

        # Run npm minify
        # We don't capture output anymore to avoid encoding issues with emojis
        # in some terminal environments (e.g. CP1252 vs UTF-8)
        subprocess.run(
            ["npm", "run", "minify"],
            cwd=project_dir,
            shell=True,
            check=True
        )

        print("=" * 60)
        print("✅ Minification complete!")
        print("=" * 60)

    except subprocess.CalledProcessError as e:
        print("=" * 60)
        print(f"❌ Error running minification:")
        if e.stdout:
            print(e.stdout)
        if e.stderr:
            print(e.stderr)
        print("=" * 60)
        sys.exit(1)

    except FileNotFoundError:
        print("=" * 60)
        print("❌ npm not found. Please install Node.js from https://nodejs.org/")
        print("=" * 60)
        sys.exit(1)

# Register the pre-build hook
print("Registering pre-build minification hook...")
env.AddPreAction("buildprog", run_minify)
env.AddPreAction("size", run_minify)
env.AddPreAction("upload", run_minify)
# Also run it immediately when the script is loaded by PlatformIO
run_minify(None, None, env)
