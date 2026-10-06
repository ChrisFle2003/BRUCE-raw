import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('bruce_benchmark', ROOT / 'scripts/benchmark.py')
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)


class BenchmarkTests(unittest.TestCase):
    def test_exact_reference_rejects_wrong_answer(self):
        self.assertTrue(bench.check_result({'value': '1/2'}, {'value': '1/2'}))
        self.assertFalse(bench.check_result({'value': '0.5'}, {'value': '1/2'}))
        self.assertFalse(bench.check_result({}, {'value': '1/2'}))

    def test_summary_excludes_failed_measurements(self):
        rows = [{'passed': True, 'wall_seconds': 2, 'cpu_seconds': 1, 'peak_rss_kib': 10},
                {'passed': False, 'wall_seconds': .01, 'cpu_seconds': 0, 'peak_rss_kib': 1}]
        result = bench.summarize(rows)
        self.assertEqual(result['passed'], 1)
        self.assertEqual(result['wall_median'], 2)

    def test_timeout_and_per_process_measurement(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run = bench.measure([sys.executable, '-c', 'import time; time.sleep(5)'], root, 'slow', .05, 512)
            self.assertTrue(run['timed_out'])
            good = bench.measure([sys.executable, '-c', 'print(123)'], root, 'fast', 5, 512)
            self.assertFalse(good['timed_out'])
            self.assertEqual(good['exit_code'], 0)
            self.assertGreater(good['peak_rss_kib'], 0)
            self.assertGreaterEqual(good['cpu_seconds'], 0)

    def test_output_directory_is_exclusive(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(FileExistsError):
                bench.prepare_output(Path(directory))


if __name__ == '__main__':
    unittest.main()
