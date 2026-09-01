# VMultiplayer (V:Multiplayer) — attribution

- Source: https://github.com/MedAnisBenSalah/VMultiplayer
- Original project: **V:Multiplayer (V:MP)** — https://www.vmultiplayer.com
  (author: OrMisicL; distributed as "V-Multiplayer" by VMPTEAM, 2015–2016)
- License: **Apache-2.0** (LICENSE file kept in place)
- Downloaded 2026-09-01; pruned: ipch/, */x64 build artifacts, Vendor/DirectX SDK
- What it is: from-scratch GTA V multiplayer base — in-game client with
  D3D11/DXGI swapchain hooks + DirectInput8 hooks + custom GUI engine
  (CChatWindow, CConnectWindow), C++ dedicated server (CServer, CPlayerManager,
  CNetwork), Launcher + LaunchHelper, shared network/message layer.
- Use per Apache-2.0: we may reuse/adapt with this notice kept; our integrations
  will credit OrMisicL / V:Multiplayer.
- Key blueprints for MyMP: D3D11 overlay rendering (in-game chat UI),
  DirectInput8 input capture, server player/entity management shape.
