# Reduces the amount of original colors, basically bit-rate but for images
# If result is a larger file size then the change is reverted
# This is a super sloppy code that assumes many things
# QUALITY_THRESHOLD = 8.0 - expected 30% file size reduction
# QUALITY_THRESHOLD = 20.0+ - Expected 70%-80% file size reduction
# This works really well against AI image detectors as a bypasser

import os
import numpy as np
from PIL import Image
from collections import Counter

INPUT_DIR = "input"
OUTPUT_DIR = "output"
INPUT_FILE = os.path.join(INPUT_DIR, "image.png")
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "image.png")

# Tweak this if you want smaller files or higher quality
# Lower (e.g., 15) = Better visual quality, larger file size.
# Higher (e.g., 50) = Allows more banding, smaller file size.
QUALITY_THRESHOLD = 8.0

def setup_folders():
    os.makedirs(INPUT_DIR, exist_ok=True)
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    for filename in os.listdir(OUTPUT_DIR):
        file_path = os.path.join(OUTPUT_DIR, filename)
        if os.path.isfile(file_path):
            os.remove(file_path)

def reduce_scale(img):
    width, height = img.size
    arr_orig = np.array(img)

    for k in range(32, 1, -1):
        if width % k == 0 and height % k == 0:
            small = img.resize((width // k, height // k), Image.Resampling.NEAREST)
            test_img = small.resize((width, height), Image.Resampling.NEAREST)

            if np.array_equal(arr_orig, np.array(test_img)):
                return small, k, (width, height), small.size

    return img, 1, (width, height), (width, height)

def fuzzy_auto_crop(img, tolerance=12.0):
    bbox = img.getbbox()
    if bbox:
        img = img.crop(bbox)

    arr = np.array(img)
    if arr.size == 0:
        return img, img.size, img.size

    corners = [
        tuple(arr[0, 0]),
        tuple(arr[0, -1]),
        tuple(arr[-1, 0]),
        tuple(arr[-1, -1])
    ]
    bg_color_tuple = Counter(corners).most_common(1)[0][0]
    bg_color = np.array(bg_color_tuple)

    arr_float = arr.astype(np.float32)
    bg_float = bg_color.astype(np.float32)

    diff = np.sqrt(np.sum((arr_float - bg_float)**2, axis=2))
    mask = diff > tolerance

    if not np.any(mask):
        return img, img.size, img.size

    coords = np.argwhere(mask)
    y0, x0 = coords.min(axis=0)
    y1, x1 = coords.max(axis=0) + 1

    y0, x0 = max(0, y0), max(0, x0)
    y1, x1 = min(img.size[1], y1), min(img.size[0], x1)

    cropped_img = img.crop((x0, y0, x1, y1))
    return cropped_img, img.size, cropped_img.size

def optimize_colors(img, quality_threshold):
# Reduces the amount of original colors, basically bit-rate but for images
    img_rgba = img.convert("RGBA")
    alpha = img_rgba.getchannel("A")
    img_rgb = img_rgba.convert("RGB")
    best_rgba = img_rgba
    best_palette_img = None
    best_colors = "Original"

    colors = img_rgba.getcolors(maxcolors=1000000)
    original_color_count = len(colors) if colors else 1000000

    test_palettes = [256, 224, 192, 160, 128, 96, 64, 48, 32, 24, 16]
    arr_orig = np.array(img_rgba, dtype=np.float32)

    for num_colors in test_palettes:
        if num_colors >= original_color_count:
            continue

        q_img = img_rgb.quantize(
            colors=num_colors,
            method=Image.Quantize.MEDIANCUT,
            dither=Image.Dither.NONE,
        )

        q_rgba = q_img.convert("RGBA")
        q_rgba.putalpha(alpha)

        arr_quant = np.array(q_rgba, dtype=np.float32)
        pixel_mse = np.mean((arr_orig - arr_quant) ** 2, axis=2)
        worst_pixels_error = np.percentile(pixel_mse, 95)

        if worst_pixels_error <= quality_threshold:
            best_rgba = q_rgba
            best_palette_img = q_img
            best_colors = num_colors
        else:
            break

    return best_rgba, best_palette_img, original_color_count, best_colors

def save_image(img_rgba, palette_img, output_path):

    if palette_img is not None:

        alpha_channel = np.array(img_rgba.getchannel("A"))  # H×W uint8
        index_map = np.array(palette_img)

        transparency = [255] * 256
        for idx in range(256):
            mask = index_map == idx
            if mask.any():

                transparency[idx] = int(alpha_channel[mask].min())

        palette_img.save(output_path, optimize=True, transparency=bytes(transparency))
    else:
        img_rgba.save(output_path, optimize=True)

def main():
    setup_folders()

    if not os.path.exists(INPUT_FILE):
        print(f"Please place an image named 'image.png' in the '{INPUT_DIR}' folder and run again.")
        return

    print("Starting Image Optimization\n")
    original_file_size = os.path.getsize(INPUT_FILE)

    img = Image.open(INPUT_FILE).convert("RGBA")

    # 1. Reverse Pixel-Art Scaling
    img, scale_factor, orig_res, new_res = reduce_scale(img)
    scale_report = "None (Image is native resolution)"
    if scale_factor > 1:
        scale_report = f"Detected {scale_factor}x pixel-art upscale. Reverted to {new_res[0]}x{new_res[1]}."

    # 2. Auto Crop
    img, orig_crop, new_crop = fuzzy_auto_crop(img, tolerance=12.0)
    crop_report = "None (No solid background found)"
    if orig_crop != new_crop:
        crop_report = f"Cropped empty space. Reduced from {orig_crop[0]}x{orig_crop[1]} to {new_crop[0]}x{new_crop[1]}."

    # 3. Palette Reduction
    img_rgba, palette_img, orig_colors, final_colors = optimize_colors(img, QUALITY_THRESHOLD)
    orig_colors_str = "1,000,000+" if orig_colors == 1000000 else orig_colors
    color_report = f"Reduced from {orig_colors_str} unique colors down to {final_colors} colors."

    # Save Output — use compact indexed PNG whenever possible
    save_image(img_rgba, palette_img, OUTPUT_FILE)
    optimized_file_size = os.path.getsize(OUTPUT_FILE)

    # If we somehow made it bigger, just keep the original unchanged.
    if optimized_file_size >= original_file_size:
        import shutil
        shutil.copy2(INPUT_FILE, OUTPUT_FILE)
        optimized_file_size = os.path.getsize(OUTPUT_FILE)
        color_report += " (reverted — palette PNG was larger than original)"
        final_colors = orig_colors_str

    saved_bytes = original_file_size - optimized_file_size
    saved_percent = (saved_bytes / original_file_size) * 100

    print("Optimization Report:")
    print("-" * 45)
    print(f"1. Scale Reduction : {scale_report}")
    print(f"2. Auto Crop       : {crop_report}")
    print(f"3. Color Palette   : {color_report}")
    print("-" * 45)
    print(f"Original File Size : {original_file_size / 1024:.2f} KB")
    print(f"Optimized Size     : {optimized_file_size / 1024:.2f} KB")
    print(f"Total Space Saved  : {saved_percent:.2f}%")
    print(f"\n[+] Success! Check the '{OUTPUT_DIR}' folder.")

if __name__ == "__main__":
    main()