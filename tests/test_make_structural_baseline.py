import unittest

from tools.make_structural_baseline import prune_config


class StructuralBaselineTests(unittest.TestCase):
    def test_removes_named_objects_data_and_routes_and_bypasses_msiir(self):
        source = """SectionWidget {
\t'stream0.eq0' {
\t\tdata 'stream0.eq0:tuple0'
\t}
\t'stream0.msiir0' {
\t\tdata 'stream0.msiir0:tuple0'
\t}
\t'stream6.sp_vi0' {
\t\tdata 'stream6.sp_vi0:data0'
\t}
}
SectionData {
\t'stream0.msiir0:tuple0'.tuples 'stream0.msiir0:tuple0'
\t'stream6.sp_vi0:data0'.bytes
\t\t'01:02:
\t\t 03:04'
}
SectionVendorTuples {
\t'stream0.eq0:tuple0' {
\t\ttokens 'stream0.eq0'
\t\ttoken209 24608
\t}
\t'stream0.msiir0:tuple0' {
\t\ttokens 'stream0.msiir0'
\t\ttuples { 0_word { token1 1 } }
\t}
}
SectionGraph {
\tset0 {
\t\tlines [
\t\t\t'stream0.msiir0, , stream0.eq0'
\t\t\t'stream0.vol_ctrl0, , stream0.msiir0'
\t\t\t'stream6.sp_vi0, , stream0.vol_ctrl0'
\t\t]
\t}
}
"""
        result = prune_config(source)
        self.assertNotIn("stream0.msiir0", result)
        self.assertNotIn("stream6.", result)
        self.assertEqual(result.count("'stream0.vol_ctrl0, , stream0.eq0'"), 1)
        self.assertIn("'stream0.eq0'", result)
        self.assertIn("token209 24580", result)

    def test_refuses_source_without_expected_msiir_route(self):
        with self.assertRaisesRegex(ValueError, "expected exactly one"):
            prune_config("SectionGraph {\n}\n")
