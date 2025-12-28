#!/usr/bin/env python3
"""
Script to extract GIF frames to PNG images.
- Extract all frames from each GIF
- Remove background (make transparent)
- Resize to 96x64
- Each GIF becomes a folder with its frames
"""

import os
import sys
from pathlib import Path

try:
    from PIL import Image
    import numpy as np
except ImportError:
    print("Installing required packages...")
    os.system(f"{sys.executable} -m pip install Pillow numpy")
    from PIL import Image
    import numpy as np


def remove_background(img, threshold=30):
    """
    Remove background from image by making near-black pixels transparent.
    
    Args:
        img: PIL Image in RGBA mode
        threshold: Pixels with R, G, B all below this value are considered background
    
    Returns:
        PIL Image with transparent background
    """
    # Convert to numpy array
    data = np.array(img)
    
    # Find pixels that are close to black (background)
    # Check if R, G, B are all below threshold
    r, g, b, a = data[:, :, 0], data[:, :, 1], data[:, :, 2], data[:, :, 3]
    black_mask = (r < threshold) & (g < threshold) & (b < threshold)
    
    # Make those pixels transparent
    data[:, :, 3] = np.where(black_mask, 0, a)
    
    return Image.fromarray(data)


def extract_gif_frames(gif_path, output_dir, target_size=(96, 64), remove_bg=True, bg_threshold=240):
    """
    Extract all frames from a GIF file.
    
    Args:
        gif_path: Path to the GIF file
        output_dir: Directory to save the extracted frames
        target_size: Tuple of (width, height) for resizing
        remove_bg: Whether to remove background
        bg_threshold: Threshold for background removal (pixels above this are considered background)
    """
    gif_name = Path(gif_path).stem
    frame_dir = Path(output_dir) / gif_name
    frame_dir.mkdir(parents=True, exist_ok=True)
    
    try:
        with Image.open(gif_path) as img:
            frame_count = 0
            
            # Get the number of frames
            try:
                while True:
                    # Convert to RGBA for transparency support
                    frame = img.convert('RGBA')
                    
                    # Resize using high-quality resampling
                    frame = frame.resize(target_size, Image.Resampling.LANCZOS)
                    
                    # Remove background if requested
                    if remove_bg:
                        frame = remove_background(frame, threshold=bg_threshold)
                    
                    # Save frame
                    frame_path = frame_dir / f"frame_{frame_count:04d}.png"
                    frame.save(frame_path, 'PNG')
                    
                    frame_count += 1
                    
                    # Move to next frame
                    img.seek(img.tell() + 1)
                    
            except EOFError:
                # End of frames
                pass
            
            print(f"  ✓ {gif_name}: {frame_count} frames extracted")
            return frame_count
            
    except Exception as e:
        print(f"  ✗ Error processing {gif_name}: {e}")
        return 0


def main():
    # Get the script directory
    script_dir = Path(__file__).parent
    
    # GIF source directory
    gif_dir = script_dir.parent / "main" / "gif"
    
    # Output directory
    output_dir = script_dir.parent / "main" / "gif_frames"
    
    # Check if gif directory exists
    if not gif_dir.exists():
        print(f"Error: GIF directory not found: {gif_dir}")
        sys.exit(1)
    
    # Create output directory
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Find all GIF files
    gif_files = sorted(gif_dir.glob("*.gif"))
    
    if not gif_files:
        print(f"No GIF files found in {gif_dir}")
        sys.exit(1)
    
    print(f"Found {len(gif_files)} GIF files")
    print(f"Output directory: {output_dir}")
    print(f"Target size: 96x64")
    print("-" * 50)
    
    total_frames = 0
    processed_count = 0
    
    for gif_path in gif_files:
        frames = extract_gif_frames(
            gif_path, 
            output_dir, 
            target_size=(96, 64),
            remove_bg=True,
            bg_threshold=30  # Adjust this if needed (higher = more aggressive bg removal for dark pixels)
        )
        total_frames += frames
        if frames > 0:
            processed_count += 1
    
    print("-" * 50)
    print(f"Processed {processed_count}/{len(gif_files)} GIF files")
    print(f"Total frames extracted: {total_frames}")
    print(f"Output saved to: {output_dir}")


if __name__ == "__main__":
    main()
