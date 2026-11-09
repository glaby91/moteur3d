"""Deterministic synthetic geometry tests, not claims about historical buildings."""
import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from photo2scene import (
    GeometryError, analyse, fit_plane, fov_from_orthogonal_vps, vanishing_point
)


class GeometryTest(unittest.TestCase):
    def test_homography_and_relative_coordinates(self):
        def px(u, v): return [200+50*u+10*v, 100-5*u+40*v]
        points = [(0,0),(2,0),(0,3),(2,3),(1,1),(3,2)]
        anchors = [{"pixel":px(u,v),"scene_uv":[u,v]} for u,v in points]
        out = fit_plane(anchors,1920,1080,
            [{"id":"arch-foot","pixel":px(1.2,1.7)}],
            [{"pixel":px(2.7,.4),"scene_uv":[2.7,.4]}])
        self.assertLess(out["fitting"]["rms_px"],1e-6)
        self.assertLess(out["held_out_validation"]["rms_px"],1e-6)
        u,v = out["derived_plane_points"][0]["scene_uv"]
        self.assertAlmostEqual(u,1.2,places=6)
        self.assertAlmostEqual(v,1.7,places=6)

    def test_single_fov_from_orthogonal_vps(self):
        out = fov_from_orthogonal_vps([2460,540],[0,540],1920,1080)
        self.assertAlmostEqual(out["focal_length_px"],1200,places=6)
        self.assertAlmostEqual(
            out["fov_horizontal_deg"], math.degrees(2*math.atan(.8)),places=6)

    def test_vanishing_intersection(self):
        vp = vanishing_point([
            {"a":[0,0],"b":[100,50]},
            {"a":[0,70],"b":[100,85]}])
        self.assertAlmostEqual(vp["pixel"][0],200,places=7)
        self.assertAlmostEqual(vp["pixel"][1],100,places=7)

    def test_degenerate_anchors_fail(self):
        anchors = [{"pixel":[10*i,20*i],"scene_uv":[i,2*i]} for i in range(4)]
        with self.assertRaises(GeometryError): fit_plane(anchors,1920,1080)

    def test_bad_fov_fails(self):
        with self.assertRaises(GeometryError):
            fov_from_orthogonal_vps([1500,540],[2000,540],1920,1080)

    def test_missing_corners_is_not_a_depth_estimate(self):
        out = analyse({
            "image":{"id":"synthetic","width":800,"height":600},
            "planes":[{"id":"ground","anchors":[
                {"pixel":[0,0],"scene_uv":[0,0]},
                {"pixel":[100,0],"scene_uv":[1,0]},
                {"pixel":[100,100],"scene_uv":[1,1]},
                {"pixel":[0,100],"scene_uv":[0,1]}],
                "query":[{"id":"point","pixel":[25,75]}]}]})
        self.assertEqual(out["planes"]["ground"]["derived_plane_points"][0]["id"],"point")
        self.assertIn("Cannot infer off-plane depth",out["planes"]["ground"]["warning"])

    def test_noncollinear_four_anchors(self):
        out=fit_plane([
            {"pixel":[100,100],"scene_uv":[0,0]},
            {"pixel":[300,100],"scene_uv":[1,0]},
            {"pixel":[300,300],"scene_uv":[1,1]},
            {"pixel":[100,300],"scene_uv":[0,1]}
        ],640,480)
        self.assertLess(out["fitting"]["max_px"],1e-7)


if __name__ == "__main__":
    unittest.main(verbosity=2)
