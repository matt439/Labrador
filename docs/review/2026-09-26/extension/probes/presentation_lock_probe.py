"""Offline diagnostic; creates synthetic evidence in a temporary directory."""
import json
from pathlib import Path
import sys
import tempfile

ROOT = next(parent for parent in Path(__file__).resolve().parents
            if (parent / "tools/cloud_performance/analysis.py").is_file())
sys.path.insert(0, str(ROOT))

from tools.cloud_performance import analysis
from tools.tests.test_cloud_performance import AnalysisTests

with tempfile.TemporaryDirectory(prefix="labrador-review-cadence-") as temporary:
    fixture = Path(temporary)
    helper = AnalysisTests()
    helper.evidence(fixture)
    helper.record_policies(fixture)
    period = 16_666_667
    samples = [{
        "ordinal": index,
        "update_ns": 16_750_000,
        "begin_ns": 30_000,
        "record_submit_ns": 100_000,
        "present_ns": 20_000,
        "whole_frame_ns": 16_900_000,
        "scheduled_interval_ns": 16_900_100,
        "pacing_wait_ns": 100,
        "start_lateness_ns": (index + 1) * (16_900_100 - period),
    } for index in range(100)]
    for repetition in (1, 2):
        helper.replace_samples(fixture, repetition, samples)
    report = analysis.analyze(fixture)
    print("complete=", report["complete"])
    backend = report["backends"]["d3d11"]
    print("presentation_locked_repetitions=", backend["presentation_locked_repetitions"])
    first = backend["repetitions"][0]
    print("work_ns=", samples[0]["update_ns"] + samples[0]["record_submit_ns"])
    print("long_begin_or_present_count=", first["long_begin_or_present"]["either_count"])
    print(json.dumps(first["presentation_lock"], sort_keys=True))
    print(report["interpretation"]["presentation_lock"])
