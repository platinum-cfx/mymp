# 🎮 My FiveM — Client Package

This folder gives you **the FiveM client** — the app that runs on top of your GTA V
so you can play on FiveM servers — plus a **custom launcher** of your own.

---

## 📦 What's in this folder

| File | What it is |
|---|---|
| `FiveM_Installer.exe` | The **official FiveM client installer** (from Cfx.re's own download servers) |
| `Launch My FiveM.bat` | Your own launcher — double-click it to find your GTA V, install/check FiveM, and start it |
| `launch-fivem.ps1` | The logic behind the launcher (PowerShell) |

---

## ✅ What you need

- **A legal copy of GTA V** (Steam, Epic Games Store, or Rockstar Games Launcher).
  FiveM is a *modification* — it uses your GTA V files; there is no way around owning the game.
- **Windows 10 or 11** (the client does not run on Linux/macOS).

---

## 🚀 How to install (2 minutes)

1. Copy this whole folder to your PC (e.g. `C:\MyFiveM`).
2. Run **`FiveM_Installer.exe`** — it will find your GTA V (or ask you to pick the folder)
   and install the FiveM client into a `FiveM.app` folder inside your GTA V directory.
3. First launch downloads the rest of the client content — let it finish.
4. After that, use **`Launch My FiveM.bat`** whenever you want to play.

> The launcher also fixes things for you: it finds GTA V automatically
> (Steam / Epic / Rockstar), and if FiveM is missing it downloads and starts the
> installer for you.

---

## 🧩 Why "my own FiveM" is the official client (important, one read)

I checked the CitizenFX repositories you linked (`github.com/citizenfx`), and here's
the honest picture:

- The **FiveM client itself is closed-source**. The `citizenfx/fivem` repository
  contains the Cfx.re platform code — **FXServer (the server)**, shared framework
  components, and docs — but **not** the client. Even its README says:
  *"To play, download the launcher from the FiveM website."*
- The code that *is* in the repository is under the **Rockstar Games Creator
  Platform License Agreement** — it explicitly doesn't let anyone ship their own
  competing FiveM client.
- GTA V itself is Rockstar's proprietary game. No repository contains it.

So there is no legitimate "compile the repos and get FiveM.exe" path — that's true
for everyone, including the creators' own build system. **The official client is the
real FiveM**, and now it's yours, installed by your own launcher.

---

## 💡 Want more later?

If you ever want to run **your own server** too (a server only costs your PC time,
no game copy needed), tell me — I'll set up FXServer from the `citizenfx` repos,
which is exactly what that code is for. But the client you asked for is what's in
this folder.
