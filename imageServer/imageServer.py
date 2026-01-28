from PIL import Image, ImageOps, ImageEnhance

from io import BytesIO
import threading
from flask import Flask, request, send_file
import random


class ImageServer:
    def __init__(self, host="0.0.0.0", port=5000):
        self.host = host
        self.port = port
        self.image_data = None
        self.app = Flask(__name__)
        self._setup_routes()
        self.lastImageCount = random.randint(1, 255)

    def set_image(self, image):
        """Set image from PIL Image object"""
        self.image_data = image
        self.adjustImage()
        self.lastImageCount = (self.lastImageCount + 1) % 255

    def set_image(self, image_path):
        """Set image from file path"""
        self.image_data = Image.open(image_path)
        self.adjustImage()
        self.lastImageCount = (self.lastImageCount + 1) % 255

    def set_image_data(self, image_bytes):
        """Set image from bytes"""
        self.image_data = Image.open(BytesIO(image_bytes))
        self.adjustImage()
        self.lastImageCount = (self.lastImageCount + 1) % 255

    def adjustImage(self):
        self.image_data = ImageOps.equalize(self.image_data)
        enhancer = ImageEnhance.Brightness(self.image_data)
        # to reduce brightness by 50%, use factor 0.5
        self.image_data = enhancer.enhance(1.1)
        self.image_data = ImageOps.autocontrast(self.image_data, cutoff=0)

    def _resize_image(self, width, height):
        """Resize image maintaining aspect ratio based on longest edge"""
        if not self.image_data:
            return None

        img = self.image_data.copy()

        if self.image_data.width > self.image_data.height:
            ratio = width / self.image_data.width
        else:
            ratio = height / self.image_data.height

        new_width = int(self.image_data.width * ratio)
        new_height = int(self.image_data.height * ratio)

        # Create white canvas with target dimensions
        canvas = Image.new("L", (width, height), 255)
        # Paste resized image centered on canvas
        offset_x = (width - new_width) // 2
        offset_y = (height - new_height) // 2
        canvas.paste(
            self.image_data.resize((new_width, new_height), Image.Resampling.LANCZOS),
            (offset_x, offset_y),
        )
        print("Height: {}, Width: {}".format(new_height, new_width))
        return canvas

        return img.resize((new_width, new_height), Image.Resampling.LANCZOS)

    def _setup_routes(self):
        @self.app.route("/image", methods=["GET"])
        def serve_image():
            width = request.args.get("width", 800, type=int)
            height = request.args.get("height", 480, type=int)
            bw = request.args.get("bw", 1, type=int)

            resized = self._resize_image(width, height)
            if bw:
                resized = resized.convert("L")
            if not resized:
                return "No image set", 404

            output = BytesIO()
            resized.save(output, format="BMP")
            output.seek(0)
            return send_file(output, mimetype="image/bmp")

        @self.app.route("/imageCount", methods=["GET"])
        def serve_imageCounter():
            response = str(self.lastImageCount)
            return response

    def run_daemon(self):
        """Run server in background thread"""
        thread = threading.Thread(
            target=lambda: self.app.run(self.host, self.port, debug=False, use_reloader=False),
            daemon=True,
        )
        thread.start()

    def run(self):
        """Run server in foreground"""
        self.app.run(self.host, self.port, debug=False)


if __name__ == "__main__":
    server = ImageServer(host="0.0.0.0", port=5000)
    server.set_image("DSC_2975.jpg")
    server.run()
