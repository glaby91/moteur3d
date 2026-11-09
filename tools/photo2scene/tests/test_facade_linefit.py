"""Numerical QA for architectural line families from photographs."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from facade_linefit import fit_structural_lines
from photo2scene import GeometryError


def segment(slope,intercept,x0,x1):
    return [x0,slope*x0+intercept,x1,slope*x1+intercept]


class FacadeLineFittingTest(unittest.TestCase):
    def test_three_levels_with_common_vanishing_point(self):
        # Three genuinely concurrent lines, with independent image segments.
        vp_x,vp_y=-300,220
        slopes=[-.1,-.2,-.3]
        data={}
        for key,m in zip(("roof","upper_openings","balcony_front"),slopes):
            b=vp_y-m*vp_x
            data[key]=[segment(m,b,100,250),segment(m,b,300,450)]
        result=fit_structural_lines(data,1024,680)
        self.assertTrue(result["stable_common_vanishing_point"])
        self.assertLess(result["vanishing_candidate_spread_px"],1e-5)
        self.assertIsNone(result["fov_horizontal_deg"])
        self.assertIsNone(result["three_dimensional_distances"])

    def test_false_convergence_rejected(self):
        # Similar-looking 2D levels do not necessarily meet at one VP.
        data={
          "roof":[segment(-.1,210,0,250),segment(-.1,210,250,400)],
          "upper_openings":[segment(-.06,280,0,250),segment(-.06,280,350,450)],
          "balcony_front":[segment(-.04,410,0,250),segment(-.04,410,350,450)],
        }
        result=fit_structural_lines(data,1024,680)
        self.assertFalse(result["stable_common_vanishing_point"])
        self.assertEqual(result["calibration_status"],"NOT_VALIDATED")
        self.assertGreater(result["vanishing_candidate_spread_px"],10)

    def test_missing_planes_are_not_silently_invented(self):
        with self.assertRaises(GeometryError):
            fit_structural_lines({"roof":[[0,0,100,20]]},1024,680)


if __name__=="__main__":
    unittest.main(verbosity=2)
