#!/usr/bin/env python3
"""
Feishu <-> MimiClaw relay (Socket Mode).

Requirements:
  pip install lark-oapi websocket-client

Env:
  APP_ID, APP_SECRET, MIMI_WS_URL
Optional:
  FEISHU_DOMAIN (default: https://open.feishu.cn)
  MIMI_MAX_SESSIONS (default: 4)
  MIMI_DEDUPE_TTL_S (default: 86400)
"""

import json
import logging
import os
import signal
import sys
import threading
import time
from collections import OrderedDict

try:
    import lark_oapi as lark
    from lark_oapi.api.im.v1 import CreateMessageRequest, CreateMessageRequestBody
except Exception as exc:
    print("Missing dependency: lark-oapi. Run: pip install lark-oapi", file=sys.stderr)
    sys.exit(1)

try:
    import websocket
except Exception as exc:
    print("Missing dependency: websocket-client. Run: pip install websocket-client", file=sys.stderr)
    sys.exit(1)

LOG = logging.getLogger("feishu_relay")


class DedupeCache:
    def __init__(self, max_size=1024, ttl_s=86400):
        self.max_size = max_size
        self.ttl_s = ttl_s
        self.lock = threading.Lock()
        self.items = OrderedDict()

    def seen(self, key: str) -> bool:
        if not key:
            return False
        now = time.time()
        with self.lock:
            # expire old
            while self.items:
                k, t = next(iter(self.items.items()))
                if now - t < self.ttl_s:
                    break
                self.items.popitem(last=False)
            if key in self.items:
                return True
            self.items[key] = now
            if len(self.items) > self.max_size:
                self.items.popitem(last=False)
            return False


class FeishuSender:
    def __init__(self, app_id: str, app_secret: str, domain: str):
        self.client = lark.Client.builder() \
            .app_id(app_id) \
            .app_secret(app_secret) \
            .domain(domain) \
            .log_level(lark.LogLevel.INFO) \
            .build()
        self.lock = threading.Lock()

    def send_text(self, chat_id: str, text: str) -> None:
        if not chat_id or text is None:
            return
        body = CreateMessageRequestBody.builder() \
            .receive_id(chat_id) \
            .msg_type("text") \
            .content(json.dumps({"text": text}, ensure_ascii=False)) \
            .build()
        req = CreateMessageRequest.builder() \
            .receive_id_type("chat_id") \
            .request_body(body) \
            .build()
        with self.lock:
            resp = self.client.im.v1.message.create(req)
        if not resp.success():
            LOG.error("feishu send failed code=%s msg=%s log_id=%s", resp.code, resp.msg, resp.get_log_id())


class MimiWsSession:
    def __init__(self, ws_url: str, chat_id: str, on_response, ping_interval=30, ping_timeout=10):
        self.ws_url = ws_url
        self.chat_id = chat_id
        self.on_response = on_response
        self.ping_interval = ping_interval
        self.ping_timeout = ping_timeout
        self._ws = None
        self._thread = None
        self._connected = threading.Event()
        self._stop = threading.Event()
        self._send_lock = threading.Lock()

    def start(self) -> None:
        if self._thread and self._thread.is_alive():
            return
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def close(self) -> None:
        self._stop.set()
        if self._ws:
            try:
                self._ws.close()
            except Exception:
                pass

    def send(self, text: str, timeout_s: int = 5) -> bool:
        if not self._connected.wait(timeout=timeout_s):
            LOG.warning("ws not connected for chat_id=%s", self.chat_id)
            return False
        payload = {
            "type": "message",
            "chat_id": self.chat_id,
            "content": text,
        }
        with self._send_lock:
            try:
                self._ws.send(json.dumps(payload, ensure_ascii=False))
                return True
            except Exception as exc:
                LOG.error("ws send failed chat_id=%s err=%s", self.chat_id, exc)
                return False

    def _run(self) -> None:
        while not self._stop.is_set():
            self._connected.clear()
            self._ws = websocket.WebSocketApp(
                self.ws_url,
                on_open=self._on_open,
                on_message=self._on_message,
                on_error=self._on_error,
                on_close=self._on_close,
            )
            self._ws.run_forever(ping_interval=self.ping_interval, ping_timeout=self.ping_timeout)
            if not self._stop.is_set():
                time.sleep(2)

    def _on_open(self, ws):
        self._connected.set()
        LOG.info("ws connected chat_id=%s", self.chat_id)

    def _on_close(self, ws, status_code, msg):
        self._connected.clear()
        LOG.warning("ws closed chat_id=%s code=%s msg=%s", self.chat_id, status_code, msg)

    def _on_error(self, ws, error):
        self._connected.clear()
        LOG.error("ws error chat_id=%s err=%s", self.chat_id, error)

    def _on_message(self, ws, message):
        try:
            data = json.loads(message)
        except Exception:
            return
        if data.get("type") != "response":
            return
        chat_id = data.get("chat_id") or self.chat_id
        content = data.get("content")
        if content is None:
            return
        self.on_response(chat_id, content)


class FeishuRelay:
    def __init__(self, app_id: str, app_secret: str, domain: str, ws_url: str, max_sessions: int):
        self.sender = FeishuSender(app_id, app_secret, domain)
        self.ws_url = ws_url
        self.max_sessions = max_sessions
        self.sessions = OrderedDict()
        self.lock = threading.Lock()
        self.dedupe = DedupeCache(ttl_s=int(os.getenv("MIMI_DEDUPE_TTL_S", "86400")))

    def get_session(self, chat_id: str) -> MimiWsSession:
        with self.lock:
            sess = self.sessions.get(chat_id)
            if sess:
                self.sessions.move_to_end(chat_id)
                return sess
            if len(self.sessions) >= self.max_sessions:
                old_chat_id, old_sess = self.sessions.popitem(last=False)
                old_sess.close()
                LOG.warning("evict ws session chat_id=%s", old_chat_id)
            sess = MimiWsSession(self.ws_url, chat_id, self.send_to_feishu)
            self.sessions[chat_id] = sess
            sess.start()
            return sess

    def send_to_mimi(self, chat_id: str, text: str) -> None:
        sess = self.get_session(chat_id)
        if not sess.send(text):
            LOG.warning("drop message to mimi chat_id=%s", chat_id)

    def send_to_feishu(self, chat_id: str, text: str) -> None:
        self.sender.send_text(chat_id, text)

    def handle_im_message(self, data: "lark.im.v1.P2ImMessageReceiveV1") -> None:
        try:
            event = data.event
            if not event or not event.message:
                return
            sender_type = event.sender.sender_type if event.sender else ""
            if sender_type == "app":
                return
            msg = event.message
            if msg.message_type != "text":
                return
            if self.dedupe.seen(msg.message_id):
                return
            content = msg.content or ""
            text = content
            try:
                payload = json.loads(content)
                if isinstance(payload, dict) and "text" in payload:
                    text = payload.get("text") or ""
            except Exception:
                pass
            text = text.strip()
            if not text:
                return
            chat_id = msg.chat_id
            if not chat_id:
                return
            self.send_to_mimi(chat_id, text)
        except Exception as exc:
            LOG.error("handle_im_message error=%s", exc)

    def close_all(self) -> None:
        with self.lock:
            for _, sess in self.sessions.items():
                sess.close()
            self.sessions.clear()


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="[%(asctime)s] %(levelname)s %(message)s")

    app_id = os.getenv("APP_ID", "").strip()
    app_secret = os.getenv("APP_SECRET", "").strip()
    ws_url = os.getenv("MIMI_WS_URL", "").strip()
    domain = os.getenv("FEISHU_DOMAIN", lark.FEISHU_DOMAIN).strip()
    max_sessions = int(os.getenv("MIMI_MAX_SESSIONS", "4"))

    if not app_id or not app_secret or not ws_url:
        print("Missing env: APP_ID, APP_SECRET, MIMI_WS_URL", file=sys.stderr)
        sys.exit(2)

    relay = FeishuRelay(app_id, app_secret, domain, ws_url, max_sessions)

    def _handle_sig(_sig, _frame):
        relay.close_all()
        sys.exit(0)

    signal.signal(signal.SIGINT, _handle_sig)
    signal.signal(signal.SIGTERM, _handle_sig)

    event_handler = lark.EventDispatcherHandler.builder("", "") \
        .register_p2_im_message_receive_v1(relay.handle_im_message) \
        .build()

    cli = lark.ws.Client(app_id, app_secret,
                         event_handler=event_handler,
                         log_level=lark.LogLevel.INFO,
                         domain=domain)

    LOG.info("Feishu relay started, ws_url=%s", ws_url)
    cli.start()


if __name__ == "__main__":
    main()
