"""Photo2Scene WARP reproducible multi-view facade regression tests.

Uses labelled candidates from *three* original images, not generated images.
The error metrics describe internal consistency only, not historic accuracy.
"""
import json
import math
import tempfile
from pathlib import Path
import unittest

from multiview_facade import solve, export_obj, split_anchors


DATA=Path(__file__).resolve().parents[1]/"data/pripyat/energetik-20261109-threeview-facade-anchors.json"


class MultiviewFacadeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=json.loads(DATA.read_text(encoding="utf-8"))
        cls.report,cls.parameters=solve(cls.data)

    def test_shared_three_camera_bundle(self):
        self.assertEqual(len(self.report["cameras"]),3)
        self.assertTrue(self.report["optimization"]["success"])
        h=self.report["gauge"]["height_visible_colonnade_in_bays"]
        self.assertLess(abs(h-2.2533),.2)
        self.assertIsNone(self.report["gauge"]["meters_per_bay"])
        self.assertIn("rear/right facade depth",self.report["not_determined"])

    def test_no_holdout_leak(self):
        for original,solved in zip(self.data["photos"],self.report["cameras"]):
            fitting,hold=split_anchors(original)
            self.assertEqual(solved["fit"]["count"],len(fitting))
            self.assertEqual(solved["holdout"]["count"],len(hold))
            self.assertLess(solved["fit"]["rms_px"],5.)
        tolerances=[6.,9.,5.]
        for camera,lim in zip(self.report["cameras"],tolerances):
            self.assertLess(camera["holdout"]["rms_px"],lim)

    def test_cameras_not_same_location(self):
        cameras=self.report["cameras"]
        for camera in cameras:
            self.assertTrue(math.isfinite(camera["focal_px"]))
            self.assertGreater(camera["focal_px"],80.)
            self.assertLess(camera["fov_horizontal_deg"],140.)
        positions=[camera["position_in_bay_units"] for camera in cameras]
        self.assertNotEqual(positions[0],positions[1])
        self.assertNotEqual(positions[0],positions[2])

    def test_wireframe_restrains_to_observed_front_plane(self):
        with tempfile.TemporaryDirectory() as temp:
            file=Path(temp)/"front.obj"
            export_obj(file,self.report["gauge"]["height_visible_colonnade_in_bays"],13)
            lines=file.read_text().splitlines()
            vertices=[line for line in lines if line.startswith("v ")]
            self.assertEqual(len(vertices),28)
            self.assertTrue(all(float(x.split()[3])==0 for x in vertices))
            self.assertEqual(sum(line.startswith("l ") for line in lines),16)

    def test_annotations_are_not_oracle(self):
        self.assertIn("manually placed candidates",self.data["annotation_status"])
        self.assertEqual(self.report["status"],"EXPERIMENTAL_PARTIAL_FRONT_FACADE")


if __name__=="__main__":
    unittest.main(verbosity=2)
