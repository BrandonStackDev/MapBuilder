import os
from PIL import Image
import math

# Configuration
skybox_faces = ["sky_front.png", "sky_back.png", "sky_left.png", "sky_right.png", "sky_up.png"]
output_suffix = "_smooth"
#target_color = (100, 140, 200)  # soft sky blue (R, G, B)
target_color = (122, 165, 215)

# Radius where fade starts (from center outward)
def radial_fade(img, inner_radius_frac=0.4, outer_radius_frac=0.9, blend_factor=1.0):
    width, height = img.size
    center_x = width // 2
    center_y = height // 2
    max_dist = math.hypot(center_x, center_y)

    inner_radius = inner_radius_frac * max_dist
    outer_radius = outer_radius_frac * max_dist

    pixels = img.load()
    new_img = img.copy()
    new_pixels = new_img.load()

    def lerp(c1, c2, t):
        return tuple([
            int(c1[i] * (1 - t) + c2[i] * t)
            for i in range(3)
        ])

    for y in range(height):
        for x in range(width):
            dx = x - center_x
            dy = y - center_y
            dist = math.hypot(dx, dy)

            if dist > inner_radius:
                t = min((dist - inner_radius) / (outer_radius - inner_radius), 1.0)
                t *= blend_factor
                new_pixels[x, y] = lerp(pixels[x, y], target_color, t)

    return new_img

# Main process
for face in skybox_faces:
    if not os.path.isfile(face):
        print(f"Skipping missing: {face}")
        continue

    img = Image.open(face).convert("RGB")
    faded = radial_fade(img,0.5,0.8,1.5)
    name, ext = os.path.splitext(face)
    output_path = f"{name}{output_suffix}{ext}"
    faded.save(output_path)
    print(f"Saved: {output_path}")
