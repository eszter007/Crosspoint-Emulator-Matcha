#!/usr/bin/env python3
"""List Kobo books and save one validated ACSM fulfillment token.

This intentionally stops at ACSM. It never fulfills DRM or downloads EPUB content.
"""

from __future__ import annotations

import argparse
import base64
import html
import json
import os
import re
import secrets
import sys
import time
import webbrowser
from http.cookiejar import CookieJar
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import parse_qs, urlencode, urljoin, urlsplit
from urllib.request import HTTPCookieProcessor, Request, build_opener


APP_VERSION = "4.38.23171"
AFFILIATE = "Kobo"
PLATFORM_ID = "00000000-0000-0000-0000-000000000373"
DEVICE_OS = "3.0.35+"
DEVICE_OS_VERSION = "NA"
USER_AGENT = (
    "Mozilla/5.0 (Linux; U; Android 2.0; en-us;) AppleWebKit/538.1 "
    "(KHTML, like Gecko) Version/4.0 Mobile Safari/538.1 "
    "(Kobo Touch 0373/4.38.23171)"
)
ACTIVATE_URL = "https://auth.kobobooks.com/ActivateOnWeb"
ACTIVATION_SITE = "https://www.kobo.com/activate"
AUTH_DEVICE_URL = "https://storeapi.kobo.com/v1/auth/device"
AUTH_REFRESH_URL = "https://storeapi.kobo.com/v1/auth/refresh"


def kobo_host(url: str) -> bool:
    host = urlsplit(url).hostname or ""
    host = host.lower()
    return (
        host == "kobo.com"
        or host.endswith(".kobo.com")
        or host == "kobobooks.com"
        or host.endswith(".kobobooks.com")
    )


class Response:
    def __init__(self, status: int, url: str, headers, body: bytes):
        self.status = status
        self.url = url
        self.headers = headers
        self.body = body


class KoboHTTPError(RuntimeError):
    def __init__(self, response: Response):
        snippet = response.body.decode("utf-8", "replace").strip().replace("\x00", "")
        if len(snippet) > 800:
            snippet = snippet[:800] + "…"
        super().__init__(f"HTTP {response.status} from {response.url}: {snippet}")
        self.response = response


class KoboClient:
    def __init__(self, session_file: Path, debug: bool = False):
        self.session_file = session_file.expanduser()
        self.debug = debug
        self.session = None
        self.cookies = CookieJar()
        self.opener = build_opener(HTTPCookieProcessor(self.cookies))

    def request(self, method: str, url: str, headers=None, body: bytes | None = None) -> Response:
        request_headers = {"User-Agent": USER_AGENT}
        request_headers.update(headers or {})
        request = Request(url, data=body, headers=request_headers, method=method)
        try:
            with self.opener.open(request, timeout=45) as reply:
                response = Response(reply.status, reply.geturl(), reply.headers, reply.read())
        except HTTPError as error:
            response = Response(error.code, error.geturl(), error.headers, error.read())
        except URLError as error:
            raise RuntimeError(f"network error for {url}: {error.reason}") from error
        if self.debug and response.status >= 400:
            print(f"[{response.status}] {method} {url}", file=sys.stderr)
        return response

    def json_request(self, method: str, url: str, payload=None, headers=None) -> tuple[object, Response]:
        request_headers = {"Accept": "application/json"}
        request_headers.update(headers or {})
        body = None
        if payload is not None:
            request_headers["Content-Type"] = "application/json"
            body = json.dumps(payload).encode()
        response = self.request(method, url, request_headers, body)
        if not 200 <= response.status < 300:
            raise KoboHTTPError(response)
        try:
            return json.loads(response.body.decode("utf-8")), response
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            raise RuntimeError(f"Kobo returned invalid JSON from {url}") from error

    def auth_headers(self) -> dict[str, str]:
        return {"Authorization": f"Bearer {self.session['access_token']}"}

    def save_session(self) -> None:
        self.session_file.parent.mkdir(parents=True, exist_ok=True)
        data = json.dumps(self.session, indent=2) + "\n"
        descriptor = os.open(self.session_file, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        with os.fdopen(descriptor, "w") as stream:
            stream.write(data)
        os.chmod(self.session_file, 0o600)

    def load_session(self) -> bool:
        try:
            session = json.loads(self.session_file.read_text(encoding="utf-8"))
        except (FileNotFoundError, OSError, json.JSONDecodeError):
            return False
        required = {"device_id", "serial_number", "access_token", "refresh_token"}
        if not required.issubset(session):
            return False
        self.session = session
        return True

    def refresh_access(self) -> None:
        payload = {
            "AppVersion": APP_VERSION,
            "ClientKey": base64.b64encode(PLATFORM_ID.encode()).decode(),
            "PlatformId": PLATFORM_ID,
            "RefreshToken": self.session["refresh_token"],
        }
        data, response = self.json_request(
            "POST",
            AUTH_REFRESH_URL,
            payload,
            {"Authorization": f"Bearer {self.session['access_token']}"},
        )
        if data.get("TokenType") != "Bearer" or not data.get("AccessToken") or not data.get("RefreshToken"):
            raise RuntimeError(f"incomplete Kobo refresh response (HTTP {response.status})")
        self.session["access_token"] = data["AccessToken"]
        self.session["refresh_token"] = data["RefreshToken"]
        self.save_session()

    def api_json(self, method: str, url: str, payload=None, headers=None, retry=True) -> tuple[object, Response]:
        request_headers = self.auth_headers()
        request_headers.update(headers or {})
        body = None
        if payload is not None:
            request_headers["Content-Type"] = "application/json"
            body = json.dumps(payload).encode()
        response = self.request(method, url, request_headers, body)
        if response.status == 401 and retry:
            self.refresh_access()
            return self.api_json(method, url, payload, headers, retry=False)
        if not 200 <= response.status < 300:
            raise KoboHTTPError(response)
        try:
            return json.loads(response.body.decode("utf-8")), response
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            raise RuntimeError(f"Kobo returned invalid JSON from {url}") from error

    def authenticate_device(self, device_id: str, serial_number: str, user_key: str, user_id: str, email: str) -> None:
        payload = {
            "AffiliateName": AFFILIATE,
            "AppVersion": APP_VERSION,
            "ClientKey": base64.b64encode(PLATFORM_ID.encode()).decode(),
            "DeviceId": device_id,
            "PlatformId": PLATFORM_ID,
            "SerialNumber": serial_number,
            "UserKey": user_key,
        }
        data, response = self.json_request("POST", AUTH_DEVICE_URL, payload)
        if data.get("TokenType") != "Bearer" or not data.get("AccessToken") or not data.get("RefreshToken"):
            raise RuntimeError(f"incomplete Kobo device-auth response (HTTP {response.status})")
        self.session = {
            "device_id": device_id,
            "serial_number": serial_number,
            "user_id": user_id,
            "email": email,
            "access_token": data["AccessToken"],
            "refresh_token": data["RefreshToken"],
        }
        self.save_session()

    def initialization(self) -> dict:
        data, _ = self.api_json("GET", "https://storeapi.kobo.com/v1/initialization")
        resources = data.get("Resources") or data.get("resources")
        if not resources:
            raise RuntimeError("Kobo initialization response has no Resources map")
        return resources

    def books(self, resources: dict) -> list[dict]:
        items = []
        sync_token = ""
        for _ in range(100):
            headers = {"x-kobo-synctoken": sync_token} if sync_token else {}
            data, response = self.api_json("GET", resources["library_sync"], headers=headers)
            page = data if isinstance(data, list) else data.get("Items", data.get("items", []))
            for item in page:
                if item.get("Audiobook") or item.get("audiobook"):
                    continue
                product_id = item.get("RevisionId") or item.get("revisionId") or item.get("Id") or item.get("id")
                if not product_id or any(book["id"] == str(product_id) for book in items):
                    continue
                items.append({
                    "id": str(product_id),
                    "title": str(item.get("Title") or item.get("title") or "Kobo book").strip(),
                    "author": str(item.get("Author") or item.get("author") or "").strip(),
                    "archived": bool(item.get("Archived") or item.get("archived")),
                })
            if response.headers.get("x-kobo-sync", "").lower() != "continue":
                return items
            sync_token = response.headers.get("x-kobo-synctoken", "")
            if not sync_token:
                raise RuntimeError("Kobo requested another library page without a sync token")
        raise RuntimeError("Kobo library exceeded the 100-page safety limit")

    def adobe_url(self, resources: dict, book_id: str, profiles: list[str]) -> tuple[str, str] | None:
        template = resources["content_access_book"]
        for profile in profiles:
            endpoint = template.replace("{ProductId}", book_id)
            separator = "&" if "?" in endpoint else "?"
            endpoint += separator + urlencode({"DisplayProfile": profile})
            data, _ = self.api_json("GET", endpoint)
            urls = data.get("DownloadUrls") or data.get("downloadUrls") or data.get("ContentUrls") or []
            if not isinstance(urls, list):
                continue
            for entry in urls:
                drm = str(entry.get("DRMType") or entry.get("DrmType") or "").lower()
                url = entry.get("DownloadUrl") or entry.get("Url") or entry.get("downloadUrl") or entry.get("url")
                if drm == "adobedrm" and url:
                    return urljoin("https://storeapi.kobo.com", url), profile
        return None

    def fetch_candidate(self, url: str) -> bytes:
        headers = self.auth_headers() if kobo_host(url) else {}
        response = self.request("GET", url, headers)
        if not 200 <= response.status < 300:
            raise KoboHTTPError(response)
        return response.body


def activation(client: KoboClient, no_browser: bool) -> None:
    device_id = secrets.token_hex(32)
    serial_number = secrets.token_hex(16)
    params = {
        "pwspid": PLATFORM_ID,
        "wsa": AFFILIATE,
        "pwsdid": device_id,
        "pwsav": APP_VERSION,
        "pwsdm": PLATFORM_ID,
        "pwspos": DEVICE_OS,
        "pwspov": DEVICE_OS_VERSION,
    }
    response = client.request("GET", ACTIVATE_URL + "?" + urlencode(params))
    if not 200 <= response.status < 300:
        raise KoboHTTPError(response)
    page = response.body.decode("utf-8", "replace")
    endpoint_match = re.search(r"data-poll-endpoint=[\"']([^\"']+)[\"']", page, re.I)
    code_match = re.search(
        r"qrcodegenerator/generate[^\"'<>]*?(?:%26|&amp;|&)code(?:%3D|=)(\d+)", page, re.I
    )
    if not endpoint_match or not code_match:
        raise RuntimeError("Kobo activation page format changed; use --debug to inspect the response")
    poll_url = urljoin(response.url, html.unescape(endpoint_match.group(1)))
    code = code_match.group(1)
    print("This activation code was generated by the CLI; no physical Kobo reader is needed.")
    print(f"Open {ACTIVATION_SITE} and enter this code: {code}")
    if not no_browser:
        webbrowser.open(ACTIVATION_SITE)
    print("Waiting for browser activation…", flush=True)
    for attempt in range(120):
        poll = client.request("POST", poll_url, body=b"")
        if not 200 <= poll.status < 300:
            raise KoboHTTPError(poll)
        try:
            result = json.loads(poll.body.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            raise RuntimeError("Kobo activation poll returned invalid JSON") from error
        if str(result.get("Status", "")).lower() == "complete":
            redirect = result.get("RedirectUrl") or result.get("redirectUrl")
            if not redirect:
                raise RuntimeError("Kobo activation response has no RedirectUrl")
            query = parse_qs(urlsplit(redirect).query)
            user_key = query.get("userKey", [""])[0]
            if not user_key:
                raise RuntimeError("Kobo activation response has no userKey")
            client.authenticate_device(
                device_id,
                serial_number,
                user_key,
                query.get("userId", [""])[0],
                query.get("email", [""])[0],
            )
            return
        print(f"  still waiting ({attempt + 1}/120)", end="\r", flush=True)
        time.sleep(5)
    raise RuntimeError("Kobo activation timed out")


def safe_name(title: str) -> str:
    name = re.sub(r"[\\/:*?\"<>|\x00-\x1f\x7f]", " ", title)
    return re.sub(r"\s+", " ", name).strip()[:80] or "Kobo book"


def select_book(books: list[dict], requested: int | None) -> dict:
    if requested is not None:
        if not 1 <= requested <= len(books):
            raise RuntimeError(f"--book must be between 1 and {len(books)}")
        return books[requested - 1]
    while True:
        choice = input("Select a book number (q to quit): ").strip()
        if choice.lower() == "q":
            raise SystemExit(0)
        try:
            index = int(choice)
        except ValueError:
            print("Enter a number.")
            continue
        if 1 <= index <= len(books):
            return books[index - 1]
        print(f"Enter a number from 1 to {len(books)}.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.cwd() / "kobo-acsm")
    parser.add_argument("--session-file", type=Path,
                        default=Path("~/.config/crosspoint/kobo-session.json").expanduser())
    parser.add_argument("--fresh", action="store_true", help="ignore the saved refresh token and activate again")
    parser.add_argument("--no-browser", action="store_true", help="print the activation URL without opening it")
    parser.add_argument("--book", type=int, help="download this 1-based book number without prompting")
    parser.add_argument("--list-only", action="store_true")
    parser.add_argument("--profiles", default="Desktop,Android",
                        help="comma-separated DisplayProfile values to try")
    parser.add_argument("--debug", action="store_true")
    args = parser.parse_args()

    client = KoboClient(args.session_file, args.debug)
    if args.fresh or not client.load_session():
        activation(client, args.no_browser)
    else:
        print(f"Using saved Kobo session: {client.session_file}")

    resources = client.initialization()
    books = client.books(resources)
    if not books:
        print("No Kobo books found.")
        return 0
    for index, book in enumerate(books, 1):
        author = f" — {book['author']}" if book["author"] else ""
        archived = " [archived]" if book["archived"] else ""
        print(f"{index:3}. {book['title']}{author}{archived}")
    if args.list_only:
        return 0

    book = select_book(books, args.book)
    profiles = [profile.strip() for profile in args.profiles.split(",") if profile.strip()]
    candidate = client.adobe_url(resources, book["id"], profiles)
    if not candidate:
        print(f"No AdobeDrm download is available for: {book['title']}", file=sys.stderr)
        return 2
    url, profile = candidate
    print(f"Trying Adobe candidate ({profile})…")
    body = client.fetch_candidate(url)
    text = body.decode("utf-8", "replace")
    if not re.search(r"<(?:[\w.-]+:)?fulfillmentToken\b", text, re.I):
        print("The Adobe candidate was not an ACSM; no file was written.", file=sys.stderr)
        return 2

    args.output.mkdir(parents=True, exist_ok=True)
    destination = args.output / f"{safe_name(book['title'])}.acsm"
    if destination.exists():
        stem = destination.stem
        for number in range(2, 1000):
            candidate_path = args.output / f"{stem} ({number}).acsm"
            if not candidate_path.exists():
                destination = candidate_path
                break
        else:
            raise RuntimeError("too many ACSM files with the same title")
    destination.write_bytes(body)
    os.chmod(destination, 0o600)
    print(f"Wrote ACSM only: {destination}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nCancelled.", file=sys.stderr)
        raise SystemExit(130)
    except (KoboHTTPError, RuntimeError) as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(2)
