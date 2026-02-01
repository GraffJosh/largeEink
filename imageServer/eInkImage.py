from PIL import Image, ImageOps, ImageEnhance
import random
from io import BytesIO
import threading


class EinkImage:
    def __init__(self, width=None, height=None, bw=None):
        self.image_data = None
        self.width = width
        self.height = height
        self.bw = bw

    def set_image(self, image):
        """Set image from PIL Image object"""
        self.image_data = image
        # self.prepare_image()
        # self.lastImageCount = (self.lastImageCount + 1) % 255

    def set_image(self, image_path):
        """Set image from file path"""
        self.image_data = Image.open(image_path)
        # self.prepare_image()
        # self.lastImageCount = (self.lastImageCount + 1) % 255

    def set_image_data(self, image_bytes):
        """Set image from bytes"""
        self.image_data = Image.open(BytesIO(image_bytes))
        # self.prepare_image()
        # self.lastImageCount = (self.lastImageCount + 1) % 255

    def get_image(self):
        return self.image_data

    def prepare_image(self, image_data=None, width=None, height=None, bw=None):
        print("Preparing image...")
        if image_data is None:
            image_data = self.image_data
        if width is None:
            width = self.width
        if height is None:
            height = self.height
        if bw is None:
            bw = self.bw
        image_data = self._resize_image(image_data, height=height, width=width, bw=bw)
        if image_data:
            print(
                "Prepared Image Height: {}, Width: {}".format(image_data.height, image_data.width)
            )
        else:
            print("Image preparation failed, no image data")
        image_data = ImageOps.equalize(image_data)
        enhancer = ImageEnhance.Brightness(image_data)
        # to reduce brightness by 50%, use factor 0.5
        image_data = enhancer.enhance(1)
        image_data = ImageOps.autocontrast(image_data, cutoff=0)
        self.image_data = image_data
        return image_data

    def _resize_image(self, image_data, width, height, bw):
        """Resize image maintaining aspect ratio based on longest edge"""
        if not image_data:
            if not self.image_data:
                print("No image data to resize")
                return None
            else:
                image_data = self.image_data

        img = image_data.copy()

        if image_data.width > image_data.height:
            ratio = width / image_data.width
        else:
            ratio = height / image_data.height

        new_width = int(image_data.width * ratio)
        new_height = int(image_data.height * ratio)

        # Create white canvas with target dimensions
        if bw:
            canvas = Image.new("L", (width, height), 255)
        else:
            canvas = Image.new("RGB", (width, height), 255)
        # Paste resized image centered on canvas
        offset_x = (width - new_width) // 2
        offset_y = (height - new_height) // 2
        canvas.paste(
            image_data.resize((new_width, new_height), Image.Resampling.LANCZOS),
            (offset_x, offset_y),
        )
        return canvas

        return img.resize((new_width, new_height), Image.Resampling.LANCZOS)
