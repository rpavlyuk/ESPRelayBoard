from __future__ import annotations

from typing import Any
from pathlib import Path
import subprocess
import click


def action_extensions(base_actions: dict, project_path: str) -> dict:
    """
    Adds: idf.py push-repo

    Uploads build artifacts to S3 using util/upload_build_to_s3.py
    """

    def push_repo(subcommand_name: str,
                  ctx: click.Context,
                  global_args: dict,
                  **action_args: Any) -> None:
        root = Path(project_path).resolve()

        profile = action_args.get("profile")
        build_info = action_args.get("build_info")

        script = root / "util" / "upload_build_to_s3.py"
        if not script.exists():
            raise click.ClickException(f"Upload script not found: {script}")

        cmd = ["python3", str(script), str(root)]

        if build_info:
            cmd += ["--build-info", build_info]
        if profile:
            cmd += ["--profile", profile]

        print(f"Running {subcommand_name}: {' '.join(cmd)}")
        try:
            subprocess.check_call(cmd, cwd=str(root))
        except subprocess.CalledProcessError as e:
            raise click.ClickException(f"Upload failed with exit code {e.returncode}") from e

    return {
        "version": "1",
        "actions": {
            "push-repo": {
                "callback": push_repo,
                "short_help": "Upload built firmware artifacts to S3 repo",
                "help": (
                    "Uploads build/build_info.json, build/ESPRelayBoard.bin, build/storage.bin "
                    "to s3://dist-repo-public/firmware/ESPRelayBoard/{latest,<version>/}."
                ),
                # Ensures build runs before push-repo (so you can just run: idf.py push-repo)
                "dependencies": ["build"],
                "options": [
                    {
                        "names": ["--profile", "-p"],
                        "help": "AWS profile name to use (e.g. ESPRelayBoard-repo). If omitted, boto3 default resolution is used.",
                        "default": os.getenv("AWS_PROFILE", "ESPRelayBoard-repo"),
                    },
                    {
                        "names": ["--build-info"],
                        "help": "Optional path to build_info.json (default handled by upload script).",
                        "default": None,
                        "type": click.Path(exists=True, dir_okay=False, path_type=str),
                    },
                ],
            }
        },
    }
