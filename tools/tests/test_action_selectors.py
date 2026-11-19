"""Literal Action pins and migration guards; no hosted execution or account access."""

from __future__ import annotations

import copy
from pathlib import Path
import re
import tempfile
import unittest

from tools.tests.tool_fixtures import load_tool

checker = load_tool("tools/check_action_selectors.py")
repository = Path(__file__).resolve().parents[2]
commit = "a" * 40


def inventory(name="actions/checkout", guards=None, subpaths=None):
    return {"schema_version": 1, "minimum_runner": "2.327.1", "actions": {
        name: {"version": "v7.0.1", "commit": commit, "runtime": "node24", "module_format": "esm",
               "subpaths": [""] if subpaths is None else subpaths,
               "guard_inputs": {"allow-unsafe-pr-checkout": "false"} if guards is None else guards,
               "release_url": f"https://github.com/{name}/releases/tag/v7.0.1",
               "metadata_url": f"https://raw.githubusercontent.com/{name}/{commit}/action.yml",
               "metadata_sha256": "b" * 64}}}


def workflow(ref=commit, guard="false"):
    return ("name: fixture\non: push\njobs:\n  build:\n    runs-on: windows-latest\n    steps:\n"
            f"      - uses: actions/checkout@{ref} # v7.0.1\n"
            f"        with:\n          fetch-depth: 0\n          allow-unsafe-pr-checkout: {guard}\n")


class ActionSelectorTests(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory(prefix="labrador-action-pins-")
        self.addCleanup(folder.cleanup)
        self.root = Path(folder.name)
        self.path = self.root / ".github/workflows/ci.yml"
        self.path.parent.mkdir(parents=True)
        self.path.write_text(workflow(), encoding="utf-8")

    def audit(self, record=None):
        return checker.audit(self.root, inventory() if record is None else record)

    def test_reviewed_commit_and_explicit_guard_pass(self):
        findings, uses = self.audit()
        self.assertEqual(findings, [])
        self.assertEqual(uses[0].inputs["fetch-depth"], "0")

    def test_major_tags_and_different_commits_are_not_the_reviewed_input(self):
        for ref in ("v7", "v7.0.1", "c" * 40):
            with self.subTest(ref=ref):
                self.path.write_text(workflow(ref), encoding="utf-8")
                self.assertTrue(any("must use reviewed" in item for item in self.audit()[0]))

    def test_guard_cannot_be_true_dynamic_or_missing(self):
        for value in ("true", "${{ inputs.unsafe }}", ""):
            with self.subTest(value=value):
                self.path.write_text(workflow(guard=value), encoding="utf-8")
                self.assertTrue(any("requires explicit" in item for item in self.audit()[0]))

    def test_false_in_env_does_not_satisfy_the_action_input(self):
        source = workflow().replace("          allow-unsafe-pr-checkout: false", "        env:\n          allow-unsafe-pr-checkout: false")
        self.path.write_text(source, encoding="utf-8")
        self.assertTrue(any("requires explicit" in item for item in self.audit()[0]))

    def test_named_step_if_and_quoted_literals_are_read(self):
        source = workflow().replace("      - uses:", "      - name: Checkout\n        if: always()\n        uses:")
        source = source.replace(f"actions/checkout@{commit} # v7.0.1", f"'actions/checkout@{commit}' # release")
        source = source.replace("allow-unsafe-pr-checkout: false", "allow-unsafe-pr-checkout: 'false'")
        self.path.write_text(source, encoding="utf-8")
        self.assertEqual(self.audit()[0], [])

    def test_shell_block_text_is_not_an_action_reference(self):
        source = workflow() + "      - run: |\n          uses: evil/action@main\n          with:\n            allow-unsafe-pr-checkout: true\n"
        self.path.write_text(source, encoding="utf-8")
        findings, uses = self.audit()
        self.assertEqual((findings, len(uses)), ([], 1))

    def test_multiline_input_cannot_inject_a_guard_or_fake_action(self):
        source = workflow().replace("          allow-unsafe-pr-checkout: false", "          path: |\n            allow-unsafe-pr-checkout: false\n            uses: evil/action@main")
        self.path.write_text(source, encoding="utf-8")
        findings, uses = self.audit()
        self.assertEqual(len(uses), 1)
        self.assertTrue(any("requires explicit" in item for item in findings))

    def test_duplicate_guard_or_with_mapping_is_a_failure(self):
        for extra in ("          allow-unsafe-pr-checkout: false\n", "        with:\n          fetch-depth: 0\n"):
            with self.subTest(extra=extra):
                self.path.write_text(workflow() + extra, encoding="utf-8")
                self.assertTrue(any("duplicate" in item for item in self.audit()[0]))

    def test_unreviewed_action_or_subaction_is_a_failure(self):
        for selector in (f"evil/checkout@{commit}", f"actions/checkout/sub@{commit}"):
            with self.subTest(selector=selector):
                self.path.write_text(workflow().replace(f"actions/checkout@{commit}", selector), encoding="utf-8")
                self.assertTrue(any("unreviewed" in item for item in self.audit()[0]))

    def test_cache_restore_keeps_its_reviewed_subaction_and_commit(self):
        source = workflow().replace("actions/checkout", "actions/cache/restore")
        self.path.write_text(source, encoding="utf-8")
        record = inventory("actions/cache", guards={}, subpaths=["", "restore", "save"])
        self.assertEqual(self.audit(record)[0], [])
        self.path.write_text(source.replace("/restore@", "/unreviewed@"), encoding="utf-8")
        self.assertTrue(any("unreviewed" in item for item in self.audit(record)[0]))

    def test_local_actions_resolve_only_within_the_owned_action_directory(self):
        local = self.root / ".github/actions/owned"
        local.mkdir(parents=True)
        (local / "action.yml").write_text("runs:\n  using: composite\n  steps: []\n", encoding="utf-8")
        self.path.write_text(workflow() + "      - uses: ./.github/actions/owned\n", encoding="utf-8")
        self.assertEqual(self.audit()[0], [])
        self.path.write_text(workflow() + "      - uses: ./outside\n", encoding="utf-8")
        self.assertTrue(any("local Action" in item for item in self.audit()[0]))

    def test_optional_starter_recipes_are_checked_together_with_workflows(self):
        path = self.root / "website/src/walkthrough/browser-ci.yml"
        path.parent.mkdir(parents=True)
        path.write_text(workflow("v4"), encoding="utf-8")
        self.assertTrue(any("website/src/walkthrough/browser-ci.yml" in item for item in self.audit()[0]))

    def test_node_and_artifact_guards_are_enforced_as_values_not_labels(self):
        for name, guards in (("actions/setup-node", {"package-manager-cache": "false"}),
                             ("actions/upload-artifact", {"archive": "true"}),
                             ("actions/download-artifact", {"digest-mismatch": "error", "skip-decompress": "false"})):
            with self.subTest(name=name):
                source = f"jobs:\n  build:\n    steps:\n      - uses: {name}@{commit}\n        with:\n"
                source += "".join(f"          {key}: {value}\n" for key, value in guards.items())
                self.path.write_text(source, encoding="utf-8")
                record = inventory(name, guards=guards)
                self.assertEqual(self.audit(record)[0], [])
                self.path.write_text(source.replace(next(iter(guards.values())), "wrong"), encoding="utf-8")
                self.assertTrue(any("requires explicit" in item for item in self.audit(record)[0]))

    def test_missing_workflows_and_unused_inventory_entries_cannot_false_pass(self):
        self.path.write_text("name: no actions\non: push\n", encoding="utf-8")
        self.assertTrue(any("has no source use" in item for item in self.audit()[0]))
        with self.assertRaisesRegex(ValueError, "no repository workflows"):
            checker.audit(self.root / "engine", inventory())

    def test_inventory_rejects_nonimmutable_or_incoherent_review_facts(self):
        for key, value in (("commit", "v7"), ("runtime", "node20"), ("module_format", "cjs"),
                           ("metadata_url", "https://other.invalid/action.yml"), ("metadata_sha256", "bad")):
            with self.subTest(key=key):
                record = copy.deepcopy(inventory())
                record["actions"]["actions/checkout"][key] = value
                with self.assertRaises(ValueError):
                    checker.checked_inventory(record)

    def test_actual_repository_selectors_match_the_reviewed_inventory(self):
        findings, uses = checker.audit(repository)
        self.assertEqual(findings, [])
        self.assertTrue(uses)

    def test_privileged_watch_stays_on_master_without_unsafe_override(self):
        text = (repository / ".github/workflows/master-watch.yml").read_text(encoding="utf-8")
        self.assertIn("branches: [master]", text)
        use, = checker.action_uses(text, "master-watch.yml")
        self.assertEqual(use.inputs["ref"], "master")
        self.assertEqual(use.inputs["allow-unsafe-pr-checkout"], "false")
        self.assertIn("contents: read", text)
        self.assertIn("actions: write", text)
        self.assertIn("issues: write", text)
        for path in checker.source_paths(repository):
            self.assertNotIn("pull_request_target:", path.read_text(encoding="utf-8"), path)

    def test_consumer_binary_cache_steps_remain_restore_only(self):
        text = (repository / ".github/workflows/ci.yml").read_text(encoding="utf-8").split("\njobs:\n", 1)[1]
        for name in ("wasm-null", "wasm-consumer", "consumer"):
            with self.subTest(job=name):
                body = re.search(rf"^  {name}:\n(.*?)(?=^  [\w-]+:\n|\Z)", text, re.MULTILINE | re.DOTALL)
                self.assertIsNotNone(body)
                caches = [use.selector.split("@", 1)[0] for use in checker.action_uses(body[1], name)
                          if use.selector.startswith("actions/cache")]
                self.assertEqual(caches, ["actions/cache/restore"])


if __name__ == "__main__":
    unittest.main()
