from io import BytesIO
import threading
from flask import Flask, request, send_file
import random
from eInkImage import EinkImage
import os


class EinkImageServer:
    def __init__(self, host="", port=5000):
        self.host = host
        self.port = port
        self.image_data = None
        self.app = Flask(__name__)
        self._setup_routes()
        self.lastImageCount = random.randint(1, 255)

        @self.app.route("/image", methods=["GET"])
        def serve_image():
            width = request.args.get("width", 800, type=int)
            height = request.args.get("height", 480, type=int)
            bw = request.args.get("bw", 1, type=int)

            eink_image = EinkImage(width, height, bw)

            image_files = [
                f
                for f in os.listdir(self.imageFolderName)
                if f.lower().endswith((".png", ".jpg", ".jpeg", ".bmp"))
            ]
            if image_files:
                self.lastImageCount = (self.lastImageCount + 1) % len(image_files)
                image_path = os.path.join(self.imageFolderName, image_files[self.lastImageCount])
                # image_data = Image.open(image_path)
            else:
                return "No images found", 404

            eink_image.set_image(image_path)
            print("Set Image:", image_path)
            resized = eink_image.prepare_image(width=width, height=height, bw=bw)
            print("Resized Image")
            if not resized:
                print("No resized image, returning 404")
                return "No image set", 404

            output = BytesIO()
            resized.save(output, format="BMP")
            output.seek(0)
            return send_file(output, mimetype="image/bmp")

        @self.app.route("/imageCount", methods=["GET"])
        def serve_imageCounter():
            response = str(self.lastImageCount)
            return response

    # IANEG

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
    server = EinkImageServer(host="0.0.0.0", port=5000)
    # server.set_image("DSC_2975.jpg")
    server.run()
