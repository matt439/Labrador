# Serving a development page to a phone on the LAN

How to put a browser build in front of a phone on the same network and read
its console from a desktop: the procedure `SM-14` asks for, which `SM-13`'s
iPhone session uses. **Drafted 2026-11-04 and not yet tried.** Every step
below is written from the browsers' documentation and settings as they stand,
not from a session on the owner's phone; the iPhone half is tried, and this
document corrected, in `SM-13`'s session, and the Android half waits for a
borrowed phone (`SM-44`). Menu names move between releases: where one here
does not match, the session records what it was called.

## 1. Build and serve

Build the page as usual, for example the development page for LineSweeper
or the minimal sample under `wasm-release`:

```text
cmake --preset wasm-release
cmake --build --preset wasm-release --target LineSweeperSample MinimalSample
```

[`tools/lan_serve.py`](../../tools/lan_serve.py) serves a folder on every
interface, sends `.wasm` as `application/wasm` and every file with
`Cache-Control: no-store`, so a reload after a rebuild fetches the new build,
and prints the addresses a phone can use:

```text
python tools/lan_serve.py out/build/wasm-release/samples/minimal
python tools/lan_serve.py out/build/wasm-release/samples/minimal --cert lan.pem --key lan-key.pem
```

The first form is plain HTTP on port 8000; the second, with the certificate
from section 3, is HTTPS on port 8443. It is a development server for a
trusted network: anything that can reach the port can read the folder.

The phone and the computer must be on the same network, and the computer must
let the phone in:

- **Windows.** The network's profile must be *Private* (Settings, Network &
  internet, the connection's properties); a *Public* profile drops inbound
  connections. The first time Python listens, Windows Defender Firewall asks
  whether to allow it: allow it on private networks only. Allowed later, the
  rule is under *Allow an app through firewall*.
- **macOS.** The firewall (System Settings, Network, Firewall), if on, asks
  the same on the first listen.
- **The router.** A guest network, and some mesh systems' "client isolation"
  or "AP isolation", keep devices from reaching each other; use the main
  network, or turn isolation off for the session.

Open the printed address on the phone. If it does not load, open it on the
computer by the same LAN address first: that separates a server or firewall
problem from a network one.

## 2. What a LAN address withholds: the secure context

A page is a [secure context](https://developer.mozilla.org/en-US/docs/Web/Security/Secure_Contexts)
when it is served over HTTPS, or from `localhost` or `127.0.0.1`. A page
served over plain HTTP at a LAN address such as `http://192.168.1.20:8000/`
is not one, and the browser withholds the features marked secure-only.

What the engine's browser build uses (`engine/` and the samples' shells, as of
2026-11-04), and which of it needs a secure context:

| Feature | Used for | Needs a secure context |
|---|---|---|
| WebAssembly, WebGL2 | the module and drawing | no |
| Web Audio (`AudioContext`) | sound, after the first gesture | no |
| `navigator.userActivation` | the audio unlock | no |
| `matchMedia` | reduced motion, the touch gate | no |
| Fullscreen (`requestFullscreen`) | the bundle page's button | no |
| Gamepad API (`navigator.getGamepads`, `vibrationActuator`) | pads and rumble | **yes** by the specification, and Firefox withholds it from an insecure page; whether Safari and Chrome do as well is recorded in the session |

So plain HTTP is enough for touch, sound, layout and drawing on the phone,
which is what `SM-13` records. A pad paired to the phone should be tried over HTTPS. Check
`isSecureContext` in the remote console (section 4) to see which a page got.

## 3. HTTPS on the LAN, with a certificate the phone trusts

A self-signed certificate makes Safari and Chrome show a warning, and an
untrusted certificate does not make a secure context. The way round both is a
local certificate authority that the phone is told to trust.
[mkcert](https://github.com/FiloSottile/mkcert) makes one:

1. Install mkcert on the computer (`winget install FiloSottile.mkcert` on
   Windows, `brew install mkcert` on a Mac), then run `mkcert -install`,
   which creates the authority and trusts it on this computer.
2. Make a certificate for the computer's LAN address, and its name if the
   phone will use that (`lan_serve.py` prints the address):

   ```text
   mkcert -cert-file lan.pem -key-file lan-key.pem 192.168.1.20 my-pc.local
   ```

   Keep `lan-key.pem` out of the repository.
3. Give the phone the authority's root certificate, `rootCA.pem` in the folder
   `mkcert -CAROOT` prints. Never share `rootCA-key.pem`: whoever has it can
   make certificates this phone trusts for any site.
   - **iPhone.** Send `rootCA.pem` to the phone (AirDrop or mail) and open it;
     iOS offers to download a profile. Install it in Settings, General, VPN &
     Device Management. Then turn on full trust for it in Settings, General,
     About, Certificate Trust Settings; an installed certificate is not trusted
     for websites until this switch is on.
   - **Android.** Copy `rootCA.pem` to the phone and install it in Settings,
     Security (or Security & privacy), More security settings, Encryption &
     credentials, Install a certificate, *CA certificate*. The path differs by
     maker and version.
4. Serve with `--cert lan.pem --key lan-key.pem` and open
   `https://192.168.1.20:8443/MinimalSample.html` on the phone.

When the session is over, remove the profile from the iPhone (Settings,
General, VPN & Device Management) or the certificate from Android, and run
`mkcert -uninstall` on the computer if the authority is no longer wanted.

An Android phone has a way round the certificate: with Chrome's port
forwarding (section 5), the phone opens `http://localhost:8000/`, which is a
secure context. iOS has no equivalent.

## 4. The iPhone's console in Safari's Web Inspector

Remote inspection of iOS Safari needs a Mac running Safari; the current
MacBook Air (Safari 27.0.1 on macOS 27.0.1, `SM-09`'s ceiling) is that Mac.

1. On the iPhone: Settings, Apps, Safari, Advanced, turn on **Web
   Inspector**.
2. On the Mac: Safari, Settings, Advanced, turn on **Show features for web
   developers**, which adds the Develop menu.
3. Connect the iPhone to the Mac with a cable, unlock it, and trust the Mac
   when asked. Leave the page open in Safari on the iPhone.
4. On the Mac: Develop, the iPhone's name, then the page. Web Inspector opens
   for the phone's tab: the Console shows the page's messages and errors, the
   Network tab its requests (the live demo's check that no `linesweeper.wasm`
   is fetched), and the Timelines tab its frames and, with the Memory
   timeline, the tab's memory, which `SM-13` records.

Once the phone has been paired over a cable, the Develop menu can also offer
it over the network; the cable is the first thing to try when it does not.
The Mac does not need the page itself: the phone fetches it from the server
in section 1, which can run on the Windows desktop.

## 5. Android Chrome's console in desktop Chrome

Written for `SM-44`, untried until a phone is borrowed.

1. On the phone: Settings, About phone, tap *Build number* seven times to
   enable Developer options; then in Developer options turn on **USB
   debugging**.
2. Connect it to the computer with a cable and allow debugging from this
   computer when the phone asks.
3. On the computer, open `chrome://inspect/#devices` in Chrome. The phone and
   its open tabs appear; **inspect** under the page opens DevTools for it.
4. For a secure context without a certificate, use **Port forwarding** on the
   same page: forward phone port `8000` to `localhost:8000`, tick *Enable
   port forwarding*, serve with `lan_serve.py` (plain HTTP), and open
   `http://localhost:8000/MinimalSample.html` on the phone. The page is served
   from the computer over the cable, and is a secure context because it is
   `localhost`.

Without a cable, the phone opens the LAN address as in section 1 and 3; the
console is then only reachable by inspecting over USB.

## 6. What a session records

For each page and device, as `SM-13` asks: the device, the OS and browser
versions, the URL and whether it was a secure context, the console's errors
and warnings (none is the passing case, as in the page tests), and what was
played and seen. Correct this document where a step was wrong, and record
that it was tried, with the date, at its top.
