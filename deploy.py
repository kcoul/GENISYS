"""
deploy.py
Deploys all three GENISYS binaries to an RPi5 target via SFTP.

    python deploy.py --target-ip 192.168.1.100
    python deploy.py --target-ip 192.168.1.100 --deploy-path /opt/genisys

NOTE: stop any running GENISYS processes on the target before deploying —
SFTP cannot overwrite a binary that is currently executing.

After deploy, on the Pi:
    # Terminal 1 — headless backend (always; --diag sends transcripts to your workstation)
    ~/genisys/GenisysBackend [--diag <workstation-ip>]

    # Terminal 2 (or Pi touchscreen) — studio config frontend
    ~/genisys/GenisysFrontend [--backend 127.0.0.1]

    # Terminal 3 (or workstation) — WER / transcript debug console
    ~/genisys/GenisysDebugConsole
"""

import os
import getpass
import argparse

try:
    import paramiko
except ImportError:
    raise SystemExit("pip install paramiko")

_DIST     = os.path.join(os.path.dirname(__file__), "build", "dist")
_BINARIES = ["GenisysBackend", "GenisysFrontend", "GenisysDebugConsole"]

def main():
    parser = argparse.ArgumentParser(description="Deploy GENISYS binaries to RPi5 via SFTP")
    parser.add_argument("--target-ip",   required=True,
                        help="Target IP or hostname (e.g. 192.168.1.100)")
    parser.add_argument("--deploy-path", default="~/genisys",
                        help="Destination directory on target (default: ~/genisys)")
    args = parser.parse_args()

    missing = [b for b in _BINARIES if not os.path.exists(os.path.join(_DIST, b))]
    if missing:
        raise SystemExit(
            f"Binaries not found in {_DIST}: {missing}\n"
            "Run  ./build.sh  first."
        )

    user, _, host = args.target_ip.rpartition('@')
    password = getpass.getpass(f"Password for {args.target_ip}: ")

    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(host, username=user or None, password=password)

    deploy_dir = args.deploy_path
    if '~' in deploy_dir:
        _, stdout, _ = client.exec_command('echo $HOME')
        home = stdout.read().decode().strip()
        deploy_dir = deploy_dir.replace('~', home)

    client.exec_command(f'mkdir -p "{deploy_dir}"')

    sftp = client.open_sftp()
    for binary in _BINARIES:
        local_path  = os.path.join(_DIST, binary)
        remote_path = deploy_dir.rstrip('/') + '/' + binary
        print(f"  → {binary}")
        sftp.put(local_path, remote_path)
        sftp.chmod(remote_path, 0o755)

    # Deploy model files (Hailo HEFs, etc.) preserving the dist/ subdirectory layout.
    models_src = os.path.join(_DIST, "models")
    if os.path.isdir(models_src):
        for dirpath, _, filenames in os.walk(models_src):
            rel_dir = os.path.relpath(dirpath, _DIST).replace(os.sep, '/')
            remote_dir = deploy_dir.rstrip('/') + '/' + rel_dir
            client.exec_command(f'mkdir -p "{remote_dir}"')
            for fname in sorted(filenames):
                local  = os.path.join(dirpath, fname)
                remote = remote_dir + '/' + fname
                print(f"  → {rel_dir}/{fname}")
                sftp.put(local, remote)

    # Write a convenience launch script for the backend
    run_sh = (
        "#!/bin/sh\n"
        f'exec "{deploy_dir}/GenisysBackend" "$@"\n'
    )
    run_path = deploy_dir.rstrip('/') + '/run_backend.sh'
    with sftp.open(run_path, 'w') as f:
        f.write(run_sh)
    sftp.chmod(run_path, 0o755)

    sftp.close()
    client.close()

    print(f"\nDeployed to {args.target_ip}:{deploy_dir}")
    print("\nTo run on the Pi:")
    print(f"  {deploy_dir}/GenisysBackend [--diag <your-ip>]")
    print(f"  {deploy_dir}/GenisysFrontend [--backend 127.0.0.1]")
    print(f"  {deploy_dir}/GenisysDebugConsole")
    print()
    print("To simulate a voice transcript without Hailo (trips the DebugConsole signal watchdog):")
    print("  pip install python-osc")
    print(f"  python -c \"from pythonosc import udp_client; "
          f"udp_client.SimpleUDPClient('{host}', 54282)"
          f".send_message('/genisys/diag/transcript', ['hello genisys', 0])\"")

if __name__ == "__main__":
    main()
