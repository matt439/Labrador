"""CONTRIBUTING's required-check list against the check names the workflows
report; reads ci.yml, website.yml and CONTRIBUTING.md and runs nothing.

GitHub's branch protection holds the required contexts by name. A job or
matrix entry renamed in a workflow but not in the protection leaves every pull
request waiting on a check that never reports, so CONTRIBUTING's list is the
record both sides are compared against. The live settings are not read here:
`gh api repos/matt439/Labrador/branches/master/protection` stays a manual check.
"""

from __future__ import annotations

from pathlib import Path
import re
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
WORKFLOWS = (".github/workflows/ci.yml", ".github/workflows/website.yml")
LIST_LEAD = "The required contexts are exactly these "
NUMBERS = {
    "one": 1, "two": 2, "three": 3, "four": 4, "five": 5, "six": 6, "seven": 7,
    "eight": 8, "nine": 9, "ten": 10, "eleven": 11, "twelve": 12, "thirteen": 13,
    "fourteen": 14, "fifteen": 15, "sixteen": 16, "seventeen": 17,
    "eighteen": 18, "nineteen": 19, "twenty": 20, "twenty-one": 21,
    "twenty-two": 22, "twenty-three": 23, "twenty-four": 24, "twenty-five": 25,
}
REMINDER = ("a pull request that adds, removes or renames a job or matrix entry "
            "updates CONTRIBUTING's list and the branch protection in the same change")


class WorkflowShapeError(Exception):
    pass


def scalar(text: str) -> str:
    text = text.strip()
    if len(text) >= 2 and text[0] == text[-1] and text[0] in "'\"":
        return text[1:-1]
    return text


def matrix_values(body: str, key: str) -> list[str]:
    """The values `key` takes in a job's `strategy.matrix.include` list, in
    either block (`- key: v`) or flow (`- { key: v, ... }`) form."""
    found = re.search(r"^      matrix:\n((?:(?:        .*)?\n)*)", body, re.MULTILINE)
    if found is None:
        raise WorkflowShapeError(f"the name uses matrix.{key} but the job has no matrix")
    matrix = found.group(1)
    if not re.match(r"        include:\n", matrix) or re.search(r"^        (?!include:)\S", matrix, re.MULTILINE):
        raise WorkflowShapeError("only a matrix made of an include list is read")
    values = []
    for entry in re.finditer(r"^          - (.*)\n((?:            .*\n)*)", matrix, re.MULTILINE):
        first, rest = entry.group(1), entry.group(2)
        if first.startswith("{"):
            pairs = dict(scalar(part).split(":", 1) for part in first.strip("{} ").split(","))
            pairs = {name.strip(): scalar(value) for name, value in pairs.items()}
            value = pairs.get(key)
        else:
            lines = [first] + [line.strip() for line in rest.splitlines()]
            value = next((scalar(line.split(":", 1)[1]) for line in lines
                          if line.split(":", 1)[0].strip() == key), None)
        if value is None:
            raise WorkflowShapeError(f"an include entry has no {key}")
        values.append(value)
    if not values:
        raise WorkflowShapeError("the matrix lists no include entries")
    return values


def workflow_contexts(text: str) -> list[str]:
    """Every check name a workflow's jobs report: each job's `name:`, or its
    id without one, with a `${{ matrix.key }}` expanded over the include list."""
    jobs = re.search(r"^jobs:\n((?:(?:[ #].*)?\n)*)", text, re.MULTILINE)
    if jobs is None:
        raise WorkflowShapeError("no jobs: block")
    for header in re.findall(r"^  \S.*$", jobs.group(1), re.MULTILINE):
        if not header.startswith("  #") and not re.fullmatch(r"  [A-Za-z0-9_-]+:", header):
            raise WorkflowShapeError(f"only a bare `id:` job header is read, not {header.strip()!r}")
    contexts = []
    for job in re.finditer(r"^  ([A-Za-z0-9_-]+):\n((?:(?:(?:   |  #).*)?\n)*)", jobs.group(1), re.MULTILINE):
        identifier, body = job.group(1), job.group(2)
        named = re.search(r"^    name: (.+)$", body, re.MULTILINE)
        if named and named.group(1)[0] not in "'\"" and re.search(r"\s#", named.group(1)):
            raise WorkflowShapeError(f"{identifier}: an unquoted name with a comment is not read")
        name = scalar(named.group(1)) if named else identifier
        expressions = re.findall(r"\$\{\{\s*([^}]*?)\s*\}\}", name)
        if not expressions:
            contexts.append(name)
            continue
        if len(expressions) != 1 or not re.fullmatch(r"matrix\.[A-Za-z0-9_]+", expressions[0]):
            raise WorkflowShapeError(f"{identifier}: only one ${{{{ matrix.key }}}} is expanded, not {name}")
        key = expressions[0].split(".", 1)[1]
        try:
            values = matrix_values(body, key)
        except WorkflowShapeError as error:
            raise WorkflowShapeError(f"{identifier}: {error}") from None
        pattern = re.compile(r"\$\{\{\s*matrix\." + key + r"\s*\}\}")
        contexts.extend(pattern.sub(value, name) for value in values)
    if not contexts:
        raise WorkflowShapeError("no jobs found")
    return contexts


def documented_contexts(text: str) -> tuple[int, list[str], str]:
    """CONTRIBUTING's stated count, its bulleted list, and the paragraph after it."""
    start = text.index(LIST_LEAD)
    stated = text[start + len(LIST_LEAD):].split(":", 1)[0]
    after = text[start:].split("\n\n", 2)
    bullets = re.findall(r"^- `([^`]+)`$", after[1], re.MULTILINE)
    if len(bullets) != len(after[1].splitlines()):
        raise AssertionError("CONTRIBUTING's required-context list has a line that is not a `- `name`` bullet")
    return NUMBERS[stated], bullets, after[2].split("\n\n", 1)[0]


class ParserTests(unittest.TestCase):
    def test_names_ids_and_both_matrix_forms_are_read(self):
        workflow = (
            "on: push\njobs:\n"
            "  plain:\n    runs-on: x\n"
            "  # a comment between jobs\n"
            "  named:\n    name: a name\n    steps:\n      - run: echo\n\n"
            "  block:\n    name: block (${{ matrix.preset }})\n    strategy:\n      matrix:\n"
            "        include:\n          - preset: one\n            other: ''\n"
            "          - preset: 'two'\n            other: x\n\n    steps: []\n"
            "  flow:\n    name: flow (${{matrix.target}})\n    strategy:\n      fail-fast: false\n      matrix:\n"
            "        include:\n          - { target: a, os: x }\n          - { os: y, target: b }\n"
        )
        self.assertEqual(workflow_contexts(workflow),
                         ["plain", "a name", "block (one)", "block (two)", "flow (a)", "flow (b)"])

    def test_unread_shapes_are_refused(self):
        lead = "jobs:\n  job:\n    name: j (${{ matrix.preset }})\n    strategy:\n      matrix:\n"
        for workflow in (lead + "        preset: [a, b]\n",
                         lead + "        include:\n          - preset: a\n        exclude:\n          - preset: a\n",
                         lead + "        include:\n          - other: a\n",
                         "jobs:\n  job:\n    name: j (${{ matrix.preset }})\n",
                         "jobs:\n  job:\n    name: ${{ github.ref }}\n",
                         "jobs:\n  job:\n    runs-on: x\n  other:  # a comment\n    runs-on: x\n",
                         "jobs:\n  'quoted':\n    runs-on: x\n",
                         "jobs:\n  job:\n    name: j  # a comment\n"):
            with self.subTest(workflow=workflow), self.assertRaises(WorkflowShapeError):
                workflow_contexts(workflow)


class RequiredCheckTests(unittest.TestCase):
    def test_contributing_lists_exactly_the_workflows_checks(self):
        per_workflow = []
        for relative in WORKFLOWS:
            text = (REPOSITORY / relative).read_text(encoding="utf-8")
            try:
                per_workflow.append(workflow_contexts(text))
            except WorkflowShapeError as error:
                self.fail(f"{relative}: {error}; teach this test the new shape")
        stated, listed, sentence = documented_contexts(
            (REPOSITORY / "CONTRIBUTING.md").read_text(encoding="utf-8"))
        derived = [context for contexts in per_workflow for context in contexts]

        self.assertEqual(len(derived), len(set(derived)), f"two jobs report one check name: {derived}")
        self.assertEqual(len(listed), len(set(listed)), f"CONTRIBUTING lists a context twice: {listed}")
        missing = [context for context in derived if context not in listed]
        extra = [context for context in listed if context not in derived]
        self.assertEqual(missing, [], f"checks the workflows report that CONTRIBUTING does not list; {REMINDER}")
        self.assertEqual(extra, [], f"CONTRIBUTING lists checks no workflow reports; {REMINDER}")

        self.assertEqual(stated, len(listed), "CONTRIBUTING's stated count differs from its list")
        # The list is grouped by workflow, and the sentence after it says where
        # each group comes from.
        boundary = len(per_workflow[0])
        self.assertEqual(set(listed[:boundary]), set(per_workflow[0]),
                         f"CONTRIBUTING's list does not start with {WORKFLOWS[0]}'s checks")
        counts = re.fullmatch(r"The first (\S+) are jobs in \[\.github/workflows/ci\.yml\]\(\.github/workflows/ci\.yml\)\s+"
                              r"and the last (\S+) are jobs in\s+\[\.github/workflows/website\.yml\]"
                              r"\(\.github/workflows/website\.yml\)\.", sentence)
        self.assertIsNotNone(counts, f"the sentence after the list changed shape: {sentence}")
        self.assertEqual([NUMBERS[counts.group(1)], NUMBERS[counts.group(2)]],
                         [len(contexts) for contexts in per_workflow])


if __name__ == "__main__":
    unittest.main()
