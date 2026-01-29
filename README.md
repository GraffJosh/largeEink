This repository displays web downloaded BMP images on a Seeed Xiao 7.5" display powered by the esp32S3 plus (available here: https://www.seeedstudio.com/TRMNL-7-5-Inch-OG-DIY-Kit-p-6481.html). 

This codebase at present can handle multiple 'pages' ie. URLs that point to 8bit BMP images, and render them on the display. This is useful for endpoints like puppet on home assistant that can render a bmp of a dashboard. 
Or, it can display an image served from a personal host on the network, which responds to GET with a BMP image; I have included an example of the server code as well. It's still rudimentary but I'll be integrating it into another server of mine soon enough. 

I would have used espHome (for a lot of reasons) but it can't handle grayscale, and it makes a huge difference. 

I didn't include my secrets.h or keys.py (for obvious reasons), if you actually want to use this repository email me and if you can't figure out what's required I'll help you set it up (because I hate writing docs). 

GenAI helped me write this library, as it does for most code these days, but it was mostly me. 
