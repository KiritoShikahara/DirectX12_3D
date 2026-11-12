# タイトル画像に焼き込まれた「PRESS A TO START」を消す(入力機器に合わせた表記はゲーム側で描く)
# 文字を inpaint で消し、明るさを周囲に合わせてから境目をなじませる
# 使い方: python -I Tools/remove_title_prompt.py <in.png> <out.png>
import sys
import cv2
import numpy as np

src, dst = sys.argv[1], sys.argv[2]
img = cv2.imread(src)
h, w = img.shape[:2]

# 文字がある帯(元画像 1536x1024 に対する割合)
y0, y1 = int(h * 0.695), int(h * 0.800)
x0, x1 = int(w * 0.24), int(w * 0.76)

gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
mask = np.zeros((h, w), np.uint8)
mask[y0:y1, x0:x1] = (gray[y0:y1, x0:x1] > 55).astype(np.uint8) * 255
mask = cv2.dilate(mask, np.ones((7, 7), np.uint8), iterations=3)
filled = cv2.inpaint(img, mask, 15, cv2.INPAINT_TELEA).astype(np.float32)

# 文字のにじみで明るくなった分を、帯の外の明るさに合わせて落とす
ref = np.concatenate([img[y0 - 20:y0, x0:x1], img[y1:y1 + 20, x0:x1]]).astype(np.float32)
band = filled[y0:y1, x0:x1]
band *= ref.mean(axis=(0, 1)) / np.maximum(band.mean(axis=(0, 1)), 1e-3)
filled[y0:y1, x0:x1] = band

# 補正した範囲を柔らかくなじませる
weight = np.zeros((h, w), np.float32)
weight[y0:y1, x0:x1] = 1.0
weight = cv2.GaussianBlur(weight, (0, 0), sigmaX=40, sigmaY=12)
weight = np.maximum(weight / weight.max(), (mask > 0).astype(np.float32))[..., None]

out = img.astype(np.float32) * (1 - weight) + filled * weight
noise = np.random.default_rng(1).normal(0, 2.0, out.shape).astype(np.float32)
out = np.clip(out + noise * weight, 0, 255).astype(np.uint8)
cv2.imwrite(dst, out)
print("done")
