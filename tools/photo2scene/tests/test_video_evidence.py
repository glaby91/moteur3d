"""Synthetic tests: do not need user-owned video on public CI."""
import unittest
import numpy as np
import cv2
from tools.photo2scene import video_evidence as video

class TestVideoEvidence(unittest.TestCase):
    def test_planar_warp_projects_four_image_corners(self):
        corners=[[32,24],[258,31],[230,175],[47,151]]
        patch,H=video.rectify(np.zeros((200,300,3),np.uint8),corners,(480,240))
        transformed=cv2.perspectiveTransform(np.float32(corners).reshape(-1,1,2),H).reshape((-1,2))
        target=np.array([[0,0],[479,0],[479,239],[0,239]])
        self.assertEqual(patch.shape[:2],(240,480))
        self.assertTrue(np.allclose(transformed,target,atol=0.0001))
    def test_line_detector_recovers_vertical_grid(self):
        img=np.ones((240,320,3),np.uint8)*230
        for x in (35,85,135,185,235):
            cv2.line(img,(x,20),(x,215),(10,10,10),3)
        seg=video.find_lines(img,[0,0,320,240],25)
        self.assertGreaterEqual(sum(s["category"]=="vertical" for s in seg),8)
    def test_planar_correspondence_cannot_claim_metric_depth(self):
        rng=np.random.default_rng(20261109)
        image=rng.integers(0,255,size=(240,320,3),dtype=np.uint8)
        warped=cv2.warpAffine(image,np.float32([[1,0,12],[0,1,4]]),(320,240))
        result=video.match(image,warped)
        self.assertGreater(result["matches"],100)
        self.assertEqual(result["status"],"planar_or_rotation_degenerate")
        self.assertIs(result["metric_3d_available"],False)

if __name__=="__main__":
    unittest.main()
