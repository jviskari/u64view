import socket
import struct
import numpy as np
import cv2
import time
from collections import deque
from threading import Thread, Lock

# --- Configuration ---
MULTICAST_GROUP = '239.0.1.64'
SERVER_ADDRESS = ('', 11000)
FRAME_INTERVAL = 1.0 / 50.0      # PAL 50 Hz
PACKETS_PER_FRAME = 68           # 272 lines / 4 lines per packet
JITTER_BUFFER_FRAMES = 2         # Buffer depth for smoothing

# VIC-II palette (RGB -> BGR for OpenCV)
colors = [
    (0x00, 0x00, 0x00), (0xEF, 0xEF, 0xEF), (0x8D, 0x2F, 0x34), (0x6A, 0xD4, 0xCD),
    (0x98, 0x35, 0xA4), (0x4C, 0xB4, 0x42), (0x2C, 0x29, 0xB1), (0xEF, 0xEF, 0x5D),
    (0x98, 0x4E, 0x20), (0x5B, 0x38, 0x00), (0xD1, 0x67, 0x6D), (0x4A, 0x4A, 0x4A),
    (0x7B, 0x7B, 0x7B), (0x9F, 0xEF, 0x93), (0x6D, 0x6A, 0xEF), (0xB2, 0xB2, 0xB2),
]
colors_bgr = np.array([(b, g, r) for (r, g, b) in colors], dtype=np.uint8)

# --- Shared state ---
frame_queue = deque(maxlen=10)
queue_lock = Lock()
last_frame_img = None
last_displayed_frame_num = None

# --- UDP setup ---
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(SERVER_ADDRESS)
sock.setblocking(False)
mreq = struct.pack('4sL', socket.inet_aton(MULTICAST_GROUP), socket.INADDR_ANY)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

# --- Receiver thread ---
def receiver():
    current_frame_num = None
    packets_collected = []

    while True:
        try:
            data, _ = sock.recvfrom(1024)
        except BlockingIOError:
            time.sleep(0.001)
            continue

        if not data:
            continue

        seq, frm, lin, width, lp, bp, enc = struct.unpack("<HHHHBBH", data[:12])
        pixel_bytes = data[12:]

        if current_frame_num is None:
            current_frame_num = frm
            packets_collected = []

        if frm != current_frame_num:
            if len(packets_collected) == PACKETS_PER_FRAME:
                img = np.zeros((272, 384, 3), dtype=np.uint8)
                y = 0
                for pkt in packets_collected:
                    i = 0
                    for _ in range(4):  # 4 lines per packet
                        for x in range(192):
                            b = pkt[i]
                            img[y, 2*x]   = colors_bgr[b & 0xF]
                            img[y, 2*x+1] = colors_bgr[b >> 4]
                            i += 1
                        y += 1
                with queue_lock:
                    frame_queue.append((current_frame_num, img))
            current_frame_num = frm
            packets_collected = []

        packets_collected.append(pixel_bytes)

# Start receiver thread
recv_thread = Thread(target=receiver, daemon=True)
recv_thread.start()

# --- Main thread: display loop (macOS safe) ---
next_display_time = time.time()
try:
    while True:
        now = time.time()
        if now >= next_display_time:
            next_display_time += FRAME_INTERVAL

            with queue_lock:
                if len(frame_queue) >= JITTER_BUFFER_FRAMES:
                    frm_num, frame_img = frame_queue.popleft()
                    if last_displayed_frame_num is None:
                        last_displayed_frame_num = frm_num
                        last_frame_img = frame_img
                    else:
                        expected_next = (last_displayed_frame_num + 1) & 0xFFFF
                        if frm_num == expected_next:
                            last_displayed_frame_num = frm_num
                            last_frame_img = frame_img
                        elif frm_num != last_displayed_frame_num:
                            # Skip ahead if lagging
                            last_displayed_frame_num = frm_num
                            last_frame_img = frame_img
                        # else duplicate -> keep last_frame_img

            if last_frame_img is not None:
                cv2.imshow("Ultimate 64 Viewer", last_frame_img)

        # Exit on ESC
        if cv2.waitKey(1) & 0xFF == 27:
            break

        time.sleep(0.001)

finally:
    cv2.destroyAllWindows()