Import("env")


# see https://docs.platformio.org/en/latest/scripting/examples/platformio_ini_custom_options.html
config = env.GetProjectConfig()
port = config.get("http_server", "port")
mdns = config.get("http_server", "mdns")
root = config.get("http_server", "root")
trace = config.get("http_server", "trace")
encoding = config.get("http_server", "encoding")
try:
    python_modules = config.get("http_server", "python_modules").split()
except Exception:
    python_modules = []

project_dir = env['PROJECT_DIR']

# print(f" --- httpserver options: {port=} {mdns=} {root=} {encoding=} {trace=} {python_modules=} {python_modules=} {project_dir=}")

pp = "PYTHONPATH="
for pm in python_modules:
    pp += f"{project_dir}/{pm}:"

if python_modules:
    pp = pp[:-1]

# print(f"pypath:  {pp=}")

# Multiple actions
env.AddCustomTarget(
    name="http_server",
    dependencies=None,
    actions=[
        f"{pp} "
        f"python scripts/rangeserver.py --web {root} "
        f"--port {port} "
        f"--encoding {encoding} "
        f"--mdns '{mdns}'"
    ],
    title="HTTP Server",
    description="Run local HTTP Server",
)
