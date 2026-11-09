"""Photo2Scene WARP: check actual three-view numerical fit, not architectural truth."""
from __future__ import annotations
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from three_view_front_plane import solve, export_obj

ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/"data/pripyat/energetik-three-view-front-plane-annotations.json"

class FrontPlaneTest(unittest.TestCase):
    def test_three_camera_solution_with_holdouts(self):
        result=solve(json.loads(DATA.read_text(encoding="utf-8")))
        self.assertTrue(result["solver"]["converged"])
        self.assertEqual(len(result["photos"]),3)
        self.assertAlmostEqual(result["height_in_assumed_bay_units"],2.2533,places=2)
        self.assertEqual(result["status"],"EXPERIMENTAL_UNVERIFIED_LANDMARKS")
        for photo in result["photos"]:
            self.assertLess(photo["train_rms_px"],3.0)
            self.assertLess(photo["holdout_rms_px"],7.0)
            self.assertGreater(photo["fov_horizontal_deg"],10.)
            self.assertLess(photo["fov_horizontal_deg"],130.)
        self.assertIn("side return depth",result["unknown"])

    def test_obj_is_only_a_plane(self):
        result=solve(json.loads(DATA.read_text(encoding="utf-8")))
        obj=export_obj(result["height_in_assumed_bay_units"])
        vertices=[s for s in obj.splitlines() if s.startswith("v ")]
        self.assertEqual(len(vertices),28)
        self.assertEqual({float(s.split()[3]) for s in vertices},{0.0})
        self.assertIn("UNVERIFIED bay",obj)

if __name__=="__main__":
    unittest.main()
